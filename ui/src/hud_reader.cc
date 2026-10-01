#include <hud_reader.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <utility>

#if EHT_HAVE_TESSERACT
#include <tesseract/baseapi.h>
#include <tesseract/resultiterator.h>
#endif

namespace
  {
///\brief this many newer pictures waiting behind one, and it goes unread
constexpr size_t newer_to_skip{2u};
///\brief the layer writes the picture asked for within a frame or two; one not there by then never comes
constexpr std::chrono::milliseconds picture_wait{3000};
///\brief the HUD's text is the brightest on the screen, in whatever colour it is set - the brightest channel of
/// a pixel at or above this is text
constexpr uint8_t text_level{180u};

struct grey_t
  {
  uint32_t width{};
  uint32_t height{};
  std::vector<uint8_t> pixels;
  };

///\brief a binary PPM as the layer writes it, as black text on white - what tesseract reads best
[[nodiscard]]
auto read_as_text_mask(std::filesystem::path const & path) -> std::optional<grey_t>
  {
  std::ifstream in{path, std::ios::binary};
  std::string magic;
  uint32_t width{};
  uint32_t height{};
  uint32_t depth{};
  if(not(in >> magic >> width >> height >> depth) or magic != "P6" or depth != 255u or width == 0u or height == 0u)
    return std::nullopt;
  in.get();
  std::vector<uint8_t> rgb(size_t{width} * height * 3u);
  if(not in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size())))
    return std::nullopt;
  grey_t grey{.width = width, .height = height, .pixels = std::vector<uint8_t>(size_t{width} * height)};
  for(size_t i{}; i != grey.pixels.size(); ++i)
    {
    uint8_t const brightest{std::max({rgb[i * 3u], rgb[i * 3u + 1u], rgb[i * 3u + 2u]})};
    grey.pixels[i] = brightest >= text_level ? 0u : 255u;
    }
  return grey;
  }

[[nodiscard]]
auto wait_for(std::filesystem::path const & path, std::stop_token const & stop) -> bool
  {
  auto const until{std::chrono::steady_clock::now() + picture_wait};
  std::error_code ec;
  while(not std::filesystem::exists(path, ec))
    {
    if(stop.stop_requested() or std::chrono::steady_clock::now() > until)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds{20});
    }
  return true;
  }
  }  // namespace

hud_reader_t::hud_reader_t() : worker_{[this](std::stop_token stop) { work(std::move(stop)); }} {}

hud_reader_t::~hud_reader_t()
  {
  worker_.request_stop();
  wake_.notify_all();
  }

auto hud_reader_t::available() noexcept -> bool
  { return EHT_HAVE_TESSERACT != 0; }

auto hud_reader_t::submit(job_t job) -> void
  {
    {
    std::scoped_lock const lock{mutex_};
    jobs_.push_back(std::move(job));
    }
  wake_.notify_one();
  }

auto hud_reader_t::take() -> std::vector<result_t>
  {
  std::scoped_lock const lock{mutex_};
  return std::exchange(done_, {});
  }

auto hud_reader_t::work(std::stop_token stop) -> void
  {
#if EHT_HAVE_TESSERACT
  std::unique_ptr<tesseract::TessBaseAPI> api;
#endif
  bool broken{};
  while(not stop.stop_requested())
    {
    job_t job;
    bool skip{};
      {
      std::unique_lock lock{mutex_};
      if(not wake_.wait(lock, stop, [this] { return not jobs_.empty(); }))
        return;
      job = std::move(jobs_.front());
      jobs_.pop_front();
      skip = jobs_.size() >= newer_to_skip;
      }
    result_t result{.ms = job.ms, .target = job.target, .label = {}};
    if(wait_for(job.path, stop))
      {
      // a picture is deleted even unread - the layer writes it whatever becomes of it
      auto const mask{skip ? std::nullopt : read_as_text_mask(job.path)};
      std::error_code ec;
      std::filesystem::remove(job.path, ec);
#if EHT_HAVE_TESSERACT
      if(not api and not broken)
        {
        api = std::make_unique<tesseract::TessBaseAPI>();
        if(api->Init(nullptr, "eng") != 0)
          {
          spdlog::error("hot drop: tesseract found no English data - the HUD cannot be read");
          api.reset();
          broken = true;
          }
        else
          {
          api->SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
          // the HUD's letters are some 20-40 px high on a 4K screen
          api->SetVariable("user_defined_dpi", "96");
          }
        }
      if(api and mask)
        {
        api->SetImage(mask->pixels.data(), int(mask->width), int(mask->height), 1, int(mask->width));
        std::vector<hot_drop::text_line_t> lines;
        if(api->Recognize(nullptr) == 0)
          if(std::unique_ptr<tesseract::ResultIterator> it{api->GetIterator()}; it)
            do
              {
              std::unique_ptr<char[]> const text{it->GetUTF8Text(tesseract::RIL_TEXTLINE)};
              if(text == nullptr)
                continue;
              hot_drop::text_line_t line{.text = text.get()};
              while(not line.text.empty() and (line.text.back() == '\n' or line.text.back() == ' '))
                line.text.pop_back();
              it->BoundingBox(tesseract::RIL_TEXTLINE, &line.left, &line.top, &line.right, &line.bottom);
              lines.push_back(std::move(line));
              }
            while(it->Next(tesseract::RIL_TEXTLINE));
        result.label = hot_drop::read_label(lines, job.target);
        if(result.label)
          {
          // the whole picture read at once takes a 1 for a 4 now and then; each line alone, with room around
          // it and only the characters it can hold, is read right
          auto const again = [&api](hot_drop::text_line_t const & box, char const * characters) -> std::string
          {
            int const pad{std::max(6, (box.bottom - box.top) / 2)};
            api->SetPageSegMode(tesseract::PSM_SINGLE_LINE);
            api->SetVariable("tessedit_char_whitelist", characters);
            api->SetRectangle(box.left - pad, box.top - pad, box.right - box.left + 2 * pad, box.bottom - box.top + 2 * pad);
            std::unique_ptr<char[]> const text{api->GetUTF8Text()};
            api->SetVariable("tessedit_char_whitelist", "");
            api->SetPageSegMode(tesseract::PSM_SPARSE_TEXT);
            return text == nullptr ? std::string{} : std::string{text.get()};
          };
          if(auto const distance{hot_drop::parse_distance_ls(again(result.label->distance_box, "0123456789.LsMmk"))}; distance)
            result.label->distance_ls = *distance;
          if(result.label->seconds_box.right > result.label->seconds_box.left)
            if(auto const seconds{hot_drop::parse_seconds(again(result.label->seconds_box, "0123456789:"))}; seconds)
              result.label->seconds = *seconds;
          }
        api->Clear();
        }
#else
      (void)mask;
      broken = true;
#endif
      }
    std::scoped_lock const lock{mutex_};
    done_.push_back(std::move(result));
    }
  }
