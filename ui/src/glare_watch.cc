// the codex, with the journal's events, before Qt - their "signals" is a word Qt takes for its own
#include <codex.h>
#include <time_zone_warning.h>
#include <glare_watch.h>
#include <backup.h>
#include <eht_settings.h>
#include <evidence_log.h>

#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <format>
#include <algorithm>
#include <fstream>
#include <iterator>

namespace
  {
using codex_files::spool_dir;

///\brief a file a marker for each report still to write, in the working directory of this commander's tool - what is
/// left there when the tool stops is written at the next start, by the tool whose logs the marker belongs to
std::filesystem::path const due_dir{"glare_reports_due"};

///\brief the same picture name keep() gives
[[nodiscard]]
auto picture_of(std::filesystem::path const & dir, std::string_view marker_file) -> std::filesystem::path
  {
  std::filesystem::path picture{dir / "screenshots" / marker_file};
  picture.replace_extension(".png");
  return picture;
  }

///\brief a layer that never answers must not hold the next picture back forever
constexpr std::chrono::seconds pending_limit{5};

///\brief the grey copy measured - enough for a share of the screen, small enough for every few seconds
constexpr int measured_width{192};
constexpr int measured_height{108};

///\brief put in place only whole - the evidence tool acts on a file the moment it appears
auto write_whole(std::filesystem::path const & path, std::string const & text) -> bool
  {
  std::filesystem::path const partial{path.string() + ".tmp"};
  {
  std::ofstream out{partial, std::ios::binary | std::ios::trunc};
  if(not out)
    return false;
  out << text;
  if(not out)
    return false;
  }
  std::error_code ec;
  std::filesystem::rename(partial, path, ec);
  return not ec;
  }

auto keep(std::optional<QImage> const & image, glare::marker_t marker, std::filesystem::path const & dir) -> void
  {
  std::error_code ec;
  std::filesystem::path const pictures{dir / "screenshots"};
  std::filesystem::path const markers{dir / "markers"};
  std::filesystem::create_directories(pictures, ec);
  std::filesystem::create_directories(markers, ec);

  std::filesystem::path const picture{pictures / std::format("{}_{}.png", marker.ts_utc, marker.source)};
  std::filesystem::path const partial{picture.string() + ".tmp"};
  if(image and image->save(QString::fromStdString(partial.string()), "PNG"))
    {
    std::filesystem::rename(partial, picture, ec);
    if(not ec)
      {
      marker.screenshots.push_back(picture.string());
      marker.images.push_back(
        glare::image_t{.path = picture.string(), .width = uint32_t(image->width()), .height = uint32_t(image->height())}
      );
      }
    }
  if(marker.screenshots.empty())
    spdlog::error("glare: the picture could not be saved in {}", pictures.string());

  std::filesystem::path const file{markers / glare::file_name(marker)};
  if(write_whole(file, glare::to_json(marker)))
    spdlog::info("glare: marker {}", file.string());
  else
    spdlog::error("glare: the marker could not be written as {}", file.string());
  }
  }  // namespace

glare_watch_t::~glare_watch_t()
  {
  for(std::future<void> & job: work_)
    job.wait();
  }

auto glare_watch_t::observe(bool inside_settlement) -> void
  {
  auto const cfg{eht::settings()};
  if(cfg->evidence.dir.empty() or pending_)
    return;
  // a glare measured is taken again at once, whole - even if the commander has just stepped out
  bool const full{found_.has_value()};
  if(not full and not inside_settlement)
    return;

  auto const now{std::chrono::steady_clock::now()};
  if(not full and now - last_asked_ < std::chrono::milliseconds{cfg->evidence.interval_ms})
    return;

  std::error_code ec;
  std::filesystem::create_directories(spool_dir(), ec);
  if(ec)
    return;

  last_asked_ = now;
  auto const taken{std::chrono::system_clock::now()};
  // the moment, made unlike the codex's, the scanner's and the sky's numbers
  uint64_t const id{
    uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(taken.time_since_epoch()).count()) * 4u + 2u
  };
  request_ = overlay::capture_t{
    .id = id,
    .path = (spool_dir() / std::format("glare_{}.ppm", id)).string(),
    // measured on a patch of the middle - the whole of a 4K screen every few seconds is some 25 MB a picture
    .size = full ? 1.f : cfg->evidence.measured_size,
    .delay_ms = 0u,
    .quiet = true,
    .aspect = 16.f / 9.f
  };
  pending_ = pending_t{.spool = request_.path, .taken = taken, .asked = now, .full = full};
  }

auto glare_watch_t::write(std::optional<QImage> image) -> void
  {
  if(not found_)
    return;
  auto const cfg{eht::settings()};
  std::filesystem::path const dir{backup::expand_home(cfg->evidence.dir)};
  std::string const name{glare::file_name(*found_)};
  // the report is promised on disk too - a restart before it is due must not lose it
  std::error_code ec;
  std::filesystem::create_directories(due_dir, ec);
  if(not std::ofstream{due_dir / name})
    spdlog::error("glare: the report of {} is not kept over a restart - {} could not be written", name, due_dir.string());
  // the same names keep() gives them
  reports_.push_back(
    report_t{
      .evidence_dir = dir,
      .marker = dir / "markers" / name,
      .picture = picture_of(dir, name),
      .due_file = due_dir / name,
      .moment = found_at_,
      .due = std::chrono::steady_clock::now() + std::chrono::seconds{cfg->evidence.report_after_s + 10u}
    }
  );
  work_.push_back(
    std::async(
      std::launch::async,
      [image = std::move(image),
       marker = std::move(*found_),
       dir = backup::expand_home(eht::settings()->evidence.dir)]
      {
        try
          {
          keep(image, marker, dir);
          }
        catch(std::exception const & e)
          {
          spdlog::error("glare: the marker failed: {}", e.what());
          }
      }
    )
  );
  found_.reset();
  }

auto glare_watch_t::collect(glare::game_t const & game, places_t const & places) -> void
  {
  std::erase_if(
    work_,
    [](std::future<void> const & job) { return job.wait_for(std::chrono::seconds{}) == std::future_status::ready; }
  );
  if(not recovered_)
    recover();
  write_due_reports(places);
  if(not pending_)
    return;

  std::error_code ec;
  if(not std::filesystem::exists(pending_->spool, ec))
    {
    if(std::chrono::steady_clock::now() - pending_->asked > pending_limit)
      {
      // the whole picture never came - the marker is worth writing without it
      if(pending_->full)
        write(std::nullopt);
      pending_.reset();
      }
    return;
    }

  QImage image{QString::fromStdString(pending_->spool.string())};
  std::filesystem::remove(pending_->spool, ec);
  pending_t const done{*pending_};
  pending_.reset();
  if(done.full)
    {
    write(image.isNull() ? std::nullopt : std::optional{std::move(image)});
    return;
    }
  if(image.isNull())
    return;

  auto const cfg{eht::settings()};
  QImage const small{image.scaled(measured_width, measured_height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                       .convertToFormat(QImage::Format_Grayscale8)};
  std::vector<uint8_t> grey;
  grey.reserve(size_t(small.width()) * size_t(small.height()));
  for(int y{}; y != small.height(); ++y)
    {
    uchar const * const line{small.constScanLine(y)};
    grey.insert(grey.end(), line, line + small.width());
    }
  glare::metrics_t const metrics{glare::measure(grey, uint8_t(std::min(cfg->evidence.burnt_out, 255u)))};
  auto const now{std::chrono::steady_clock::now()};
  if(not glare::is_glare(metrics, cfg->evidence.overexposed_pct))
    {
    darkened_ = true;
    return;
    }
  if(not darkened_ and now - marked_ < std::chrono::seconds{cfg->evidence.again_after_s})
    return;
  darkened_ = false;
  marked_ = now;

  found_at_ = done.taken;
  found_ = glare::marker_t{
    .ts_utc = glare::iso_utc(done.taken),
    .note = std::format("{:.0f}% of the middle of the screen burnt out", metrics.overexposed_pct),
    .metrics = metrics,
    .game = game
  };
  std::string local_time;
  try
    {
    local_time = std::format(
      "{:%H:%M:%S}",
      std::chrono::zoned_time{std::chrono::current_zone(), std::chrono::floor<std::chrono::seconds>(done.taken)}
        .get_local_time()
    );
    }
  catch(...)
    {
    eht::warn_no_time_zone();
    local_time = std::format("{:%H:%M:%S} UTC", std::chrono::floor<std::chrono::seconds>(done.taken));
    }
  noticed_ = noticed_t{.local_time = std::move(local_time), .at = now};
  spdlog::info(
    "glare: {} at {} - mean {:.2f}, p99 {:.2f}, {:.0f}% burnt out",
    found_->ts_utc,
    game.settlement.empty() ? game.body : game.settlement,
    metrics.luma_mean,
    metrics.luma_p99,
    metrics.overexposed_pct
  );
  }

auto glare_watch_t::recover() -> void
  {
  recovered_ = true;
  std::error_code ec;
  if(not std::filesystem::is_directory(due_dir, ec))
    return;
  auto const cfg{eht::settings()};
  if(cfg->evidence.dir.empty())
    {
    spdlog::warn("glare: reports left from before the restart wait in {} - evidence.dir is not set", due_dir.string());
    return;
    }
  std::filesystem::path const dir{backup::expand_home(cfg->evidence.dir)};
  auto const now{std::chrono::system_clock::now()};
  auto const after{std::chrono::seconds{cfg->evidence.report_after_s + 10u}};
  for(std::filesystem::directory_entry const & entry: std::filesystem::directory_iterator{due_dir, ec})
    {
    std::string const name{entry.path().filename().string()};
    std::filesystem::path const marker{dir / "markers" / name};
    auto const moment{evidence::marker_moment(name)};
    if(not moment or not std::filesystem::exists(marker, ec))
      {
      spdlog::warn("glare: {} in {} names no marker of {} - let go", name, due_dir.string(), (dir / "markers").string());
      std::filesystem::remove(entry.path(), ec);
      continue;
      }
    // a report already due is written at once, the rest when their minutes have passed
    auto const wait{std::chrono::duration_cast<std::chrono::steady_clock::duration>(
      std::max(std::chrono::system_clock::duration{}, *moment + after - now)
    )};
    reports_.push_back(
      report_t{
        .evidence_dir = dir,
        .marker = marker,
        .picture = picture_of(dir, name),
        .due_file = entry.path(),
        .moment = *moment,
        .due = std::chrono::steady_clock::now() + wait,
        .waits_for_netlog = true
      }
    );
    spdlog::info("glare: the report of {} left from before the restart is written once its netLog is found", name);
    }
  if(ec)
    spdlog::error("glare: {} could not be read: {}", due_dir.string(), ec.message());
  }

auto glare_watch_t::write_due_reports(places_t const & places) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  // a report taken up after a restart waits for the game's Logs - known only once the game runs, for a launcher
  bool const no_netlog{places.netlog_dir.empty()};
  auto const due{std::ranges::partition(
    reports_, [now, no_netlog](report_t const & r) { return r.due > now or (r.waits_for_netlog and no_netlog); }
  )};
  if(due.empty())
    return;
  auto const cfg{eht::settings()};
  std::chrono::seconds offset{};
  try
    {
    offset = std::chrono::current_zone()->get_info(std::chrono::system_clock::now()).offset;
    }
  catch(...)
    {
    eht::warn_no_time_zone();
    }
  for(report_t const & report: due)
    {
    std::ifstream in{report.marker};
    std::string marker{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::error_code ec;
    evidence::report_input_t input{
      .evidence_dir = report.evidence_dir,
      .marker_json = std::move(marker),
      .moment = report.moment,
      .picture = std::filesystem::exists(report.picture, ec) ? report.picture : std::filesystem::path{},
      .journal_dir = places.journal_dir,
      .netlog_dir = places.netlog_dir,
      .before = std::chrono::seconds{cfg->evidence.report_before_s},
      .after = std::chrono::seconds{cfg->evidence.report_after_s},
      .utc_offset = offset
    };
    work_.push_back(
      std::async(
        std::launch::async,
        [input = std::move(input), due_file = report.due_file]
        {
          try
            {
            spdlog::info("glare: report {}", evidence::write_report(input).string());
            }
          catch(std::exception const & e)
            {
            spdlog::error("glare: the report failed: {}", e.what());
            }
          // written or failed, it is not tried again at the next start
          std::error_code ec;
          if(not due_file.empty() and not std::filesystem::remove(due_file, ec) and ec)
            spdlog::error("glare: {} could not be removed: {}", due_file.string(), ec.message());
        }
      )
    );
    }
  reports_.erase(due.begin(), due.end());
  }
