#include <screenshots.h>
#include <time_zone_warning.h>
#include <eht_settings.h>

#include <overlay_protocol.h>
#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <charconv>
#include <format>

namespace
  {
///\brief the name a screenshot is kept under - the local time it was taken, as the player remembers it
[[nodiscard]]
auto target_name(std::string_view stem, std::string_view extension) -> std::string
  {
  uint64_t moment{};
  std::string_view const digits{stem.substr(overlay::screenshot_prefix.size())};
  if(std::from_chars(digits.data(), digits.data() + digits.size(), moment).ec != std::errc{})
    return std::format("{}.{}", stem, extension);

  std::chrono::sys_time<std::chrono::milliseconds> const taken{std::chrono::milliseconds{moment}};
  try
    {
    std::chrono::zoned_time const local{std::chrono::current_zone(), taken};
    return std::format("ED {:%Y-%m-%d %H-%M-%S}.{}", local.get_local_time(), extension);
    }
  catch(...)
    {
    eht::warn_no_time_zone();
    return std::format("ED {:%Y-%m-%d %H-%M-%S} UTC.{}", taken, extension);
    }
  }

auto file(std::filesystem::path const & spool) -> void
  {
  auto const cfg{eht::settings()};
  bool const jpeg{cfg->screenshots.format == "jpg" or cfg->screenshots.format == "jpeg"};
  std::filesystem::path const dir{std::filesystem::absolute(cfg->screenshots.dir)};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::filesystem::path const target{dir / target_name(spool.stem().string(), jpeg ? "jpg" : "png")};

  QImage const image{QString::fromStdString(spool.string())};
  if(image.isNull())
    {
    spdlog::error("screenshot: {} could not be read, left where it is", spool.string());
    return;
    }
  bool const saved{
    jpeg ? image.save(QString::fromStdString(target.string()), "JPG", int(cfg->screenshots.jpeg_quality))
         : image.save(QString::fromStdString(target.string()), "PNG")
  };
  if(not saved)
    {
    spdlog::error("screenshot: could not be saved as {}, left in {}", target.string(), spool.string());
    return;
    }
  // the spool file is the layer's hand-over to us - it named it for nobody else
  std::filesystem::remove(spool, ec);
  spdlog::info("screenshot: {}", target.string());
  }
  }  // namespace

screenshots_t::~screenshots_t()
  {
  for(std::future<void> & job: work_)
    job.wait();
  }

auto screenshots_t::collect() -> void
  {
  std::erase_if(
    work_,
    [](std::future<void> const & job) { return job.wait_for(std::chrono::seconds{}) == std::future_status::ready; }
  );

  // a directory listing is cheap, but not worth every frame - a second's delay nobody notices
  auto const now{std::chrono::steady_clock::now()};
  if(now - looked_ < std::chrono::seconds{1})
    return;
  looked_ = now;

  std::filesystem::path const spool{overlay::default_spool_path()};
  std::error_code ec;
  // the layer writes into it but cannot make it - the game's container may not
  std::filesystem::create_directories(spool, ec);
  for(std::filesystem::directory_entry const & entry: std::filesystem::directory_iterator{spool, ec})
    {
    std::filesystem::path const & path{entry.path()};
    // .part is a picture still being written; only the renamed one is complete
    if(path.extension() != ".ppm" or not path.filename().string().starts_with(overlay::screenshot_prefix))
      continue;

      {
      std::lock_guard const lock{shared_->mutex};
      if(not shared_->busy.insert(path).second)
        continue;
      }
    work_.push_back(
      std::async(
        std::launch::async,
        [path, shared = shared_]
        {
          try
            {
            file(path);
            }
          catch(std::exception const & e)
            {
            spdlog::error("screenshot: {} failed: {}", path.string(), e.what());
            }
          std::lock_guard const lock{shared->mutex};
          shared->busy.erase(path);
        }
      )
    );
    }
  }
