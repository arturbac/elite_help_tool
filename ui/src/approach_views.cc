#include <approach_views.h>
#include <codex.h>
#include <eht_settings.h>
#include <planet_faces.h>

#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <format>

namespace
  {
///\brief a layer that never answers must not hold the next picture back forever
constexpr std::chrono::seconds pending_limit{5};
  }  // namespace

auto approach_views_t::observe(bool ship_view, std::string const & system, std::string const & body) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->exploration.approach_faces or not ship_view or body.empty() or pending_)
    return;
  auto const now{std::chrono::steady_clock::now()};
  if(now - last_asked_ < std::chrono::milliseconds{cfg->exploration.approach_interval_ms})
    return;

  std::error_code ec;
  std::filesystem::create_directories(codex_files::spool_dir(), ec);
  if(ec)
    return;
  last_asked_ = now;
  // the moment, made unlike the other pictures' numbers: the sky album's are 4n, the glare's 4n + 2
  uint64_t const id{
    uint64_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    ) * 4u
    + 3u
  };
  request_ = overlay::capture_t{
    .id = id,
    .path = (codex_files::spool_dir() / std::format("approach_{}.ppm", id)).string(),
    .size = cfg->exploration.sky_size,
    .delay_ms = 0u,
    .quiet = true,
    .aspect = 16.f / 9.f
  };
  pending_ = pending_t{.spool = request_.path, .system = system, .body = body, .asked = now};
  }

auto approach_views_t::collect(planet_faces_t & faces) -> void
  {
  if(not pending_)
    return;
  std::error_code ec;
  if(not std::filesystem::exists(pending_->spool, ec))
    {
    // another picture took the layer's turn, or it never came - the next one is asked for in its time
    if(std::chrono::steady_clock::now() - pending_->asked > pending_limit)
      pending_.reset();
    return;
    }
  QImage image{QString::fromStdString(pending_->spool.string())};
  std::filesystem::remove(pending_->spool, ec);
  pending_t const done{std::move(*pending_)};
  pending_.reset();
  if(not image.isNull())
    faces.offer_view(done.system, done.body, std::move(image));
  }
