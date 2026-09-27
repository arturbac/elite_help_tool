#include <scanner_sheet.h>
#include <codex.h>
#include <eht_settings.h>

#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <algorithm>
#include <cmath>
#include <format>
#include <fstream>
#include <limits>

namespace
  {
using codex_files::codex_dir;
using codex_files::file_safe;
using codex_files::spool_dir;

///\brief the side of the grey copy two views are compared by
constexpr int signature_side{32};

///\brief a layer that never answers must not hold the next picture back forever
constexpr std::chrono::seconds pending_limit{5};

[[nodiscard]]
auto body_dir(std::string const & body) -> std::filesystem::path
  { return codex_dir() / "scanner" / file_safe(body); }

[[nodiscard]]
auto signature_of(QImage const & image) -> std::vector<uint8_t>
  {
  QImage const small{image.scaled(signature_side, signature_side, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_Grayscale8)};
  std::vector<uint8_t> signature;
  signature.reserve(size_t(signature_side) * signature_side);
  for(int y{}; y != small.height(); ++y)
    {
    uchar const * const line{small.constScanLine(y)};
    signature.insert(signature.end(), line, line + small.width());
    }
  return signature;
  }

///\brief the average difference of two grey copies, 0..255
[[nodiscard]]
auto difference(std::vector<uint8_t> const & a, std::vector<uint8_t> const & b) -> float
  {
  if(a.size() != b.size() or a.empty())
    return std::numeric_limits<float>::max();
  uint64_t sum{};
  for(size_t ix{}; ix != a.size(); ++ix)
    sum += uint64_t(std::abs(int(a[ix]) - int(b[ix])));
  return float(sum) / float(a.size());
  }

///\brief a binary PPM, the one format the layer reads without a library
auto write_ppm(QImage const & image, std::filesystem::path const & path) -> bool
  {
  QImage const rgb{image.convertToFormat(QImage::Format_RGB888)};
  std::filesystem::path const partial{path.string() + ".partial"};
  {
  std::ofstream out{partial, std::ios::binary | std::ios::trunc};
  if(not out)
    return false;
  out << std::format("P6\n{} {}\n255\n", rgb.width(), rgb.height());
  for(int y{}; y != rgb.height(); ++y)
    out.write(reinterpret_cast<char const *>(rgb.constScanLine(y)), std::streamsize(rgb.width()) * 3);
  if(not out)
    return false;
  }
  // put in place only whole, so the layer never reads half a picture
  std::error_code ec;
  std::filesystem::rename(partial, path, ec);
  return not ec;
  }
  }  // namespace

auto scanner_sheet_t::views_of(std::string const & body) -> std::vector<view_t> &
  {
  auto const [it, fresh]{views_.try_emplace(body)};
  if(not fresh)
    return it->second;

  std::error_code ec;
  std::vector<std::filesystem::path> files;
  for(auto const & entry: std::filesystem::directory_iterator{body_dir(body), ec})
    if(entry.path().extension() == ".jpg")
      files.push_back(entry.path());
  std::ranges::sort(files);
  for(std::filesystem::path const & file: files)
    if(QImage const image{QString::fromStdString(file.string())}; not image.isNull())
      it->second.push_back(view_t{.file = file, .signature = signature_of(image), .thumbnail = {}, .generation = 0u});
  return it->second;
  }

auto scanner_sheet_t::observe(bool scanner_open, std::string const & body) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->exploration.scanner_pictures or not scanner_open or body.empty() or pending_)
    return;

  auto const now{std::chrono::steady_clock::now()};
  if(now - last_asked_ < std::chrono::milliseconds{cfg->exploration.scanner_interval_ms})
    return;

  std::error_code ec;
  std::filesystem::create_directories(spool_dir(), ec);
  if(ec)
    return;

  last_asked_ = now;
  // the number is the moment, and never the codex's own: a picture of a sample asked in the same
  // millisecond is not to be taken for this one
  uint64_t const id{
    uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
               .count())
      * 2u
    + 1u
  };
  request_ = overlay::capture_t{
    .id = id,
    .path = (spool_dir() / std::format("scanner_{}.ppm", id)).string(),
    .size = cfg->exploration.scanner_size,
    .delay_ms = 0u,
    .quiet = true
  };
  pending_ = pending_t{.spool = request_.path, .body = body, .asked = now};
  }

auto scanner_sheet_t::collect() -> void
  {
  if(not pending_)
    return;

  std::error_code ec;
  if(not std::filesystem::exists(pending_->spool, ec))
    {
    if(std::chrono::steady_clock::now() - pending_->asked > pending_limit)
      pending_.reset();
    return;
    }

  QImage const image{QString::fromStdString(pending_->spool.string())};
  std::filesystem::remove(pending_->spool, ec);
  std::string const body{std::move(pending_->body)};
  pending_.reset();
  if(image.isNull())
    return;

  auto const cfg{eht::settings()};
  std::vector<view_t> & views{views_of(body)};
  std::vector<uint8_t> signature{signature_of(image)};

  view_t * nearest{};
  float closest{std::numeric_limits<float>::max()};
  for(view_t & view: views)
    if(float const d{difference(view.signature, signature)}; d < closest)
      {
      closest = d;
      nearest = &view;
      }

  std::filesystem::path file;
  if(nearest != nullptr and closest < cfg->exploration.scanner_difference)
    // the same filter a moment later - the newer picture is kept in its place
    file = nearest->file;
  else if(views.size() < cfg->exploration.scanner_views)
    {
    std::filesystem::create_directories(body_dir(body), ec);
    uint32_t number{uint32_t(views.size()) + 1u};
    while(std::filesystem::exists(body_dir(body) / std::format("{:02}.jpg", number), ec))
      ++number;
    file = body_dir(body) / std::format("{:02}.jpg", number);
    views.push_back(view_t{.file = file, .signature = {}, .thumbnail = {}, .generation = 0u});
    nearest = &views.back();
    spdlog::info("scanner: a new view of {} kept as {}", body, file.string());
    }
  else
    return;

  if(not image.save(QString::fromStdString(file.string()), "JPG", int(cfg->exploration.jpeg_quality)))
    {
    spdlog::error("scanner: the view could not be saved as {}", file.string());
    return;
    }
  nearest->signature = std::move(signature);
  // the thumbnail is made again when next wanted, under a new name the layer will read afresh
  nearest->thumbnail.clear();
  ++nearest->generation;
  }

auto scanner_sheet_t::pictures(std::string const & body) -> std::vector<overlay::picture_t>
  {
  std::vector<overlay::picture_t> result;
  if(body.empty())
    return result;

  auto const cfg{eht::settings()};
  std::vector<view_t> & views{views_of(body)};
  std::error_code ec;
  for(view_t & view: views)
    {
    if(view.thumbnail.empty() or not std::filesystem::exists(view.thumbnail, ec))
      {
      QImage const image{QString::fromStdString(view.file.string())};
      if(image.isNull())
        continue;
      int const side{int(std::clamp(cfg->exploration.scanner_thumbnail, 64u, 640u))};
      std::filesystem::path const thumbnail{
        spool_dir()
        / std::format("thumb_{}_{}_{}.ppm", file_safe(body), view.file.stem().string(), view.generation)
      };
      std::filesystem::create_directories(spool_dir(), ec);
      if(not write_ppm(image.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation), thumbnail))
        continue;
      view.thumbnail = thumbnail;
      }
    result.push_back(overlay::picture_t{.path = view.thumbnail.string()});
    }
  return result;
  }
