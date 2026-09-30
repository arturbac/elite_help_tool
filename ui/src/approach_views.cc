#include <approach_views.h>
#include <codex.h>
#include <eht_settings.h>
#include <planet_faces.h>

#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>

namespace
  {
///\brief a layer that never answers must not hold the next picture back forever
constexpr std::chrono::seconds pending_limit{5};
///\brief how often the sample is made while flying at a planet
constexpr uint32_t sample_every_ms{250u};
///\brief the rectangle asked for is the ball's with this much of its radius around it
constexpr float margin{1.12f};

struct sample_read_t
  {
  overlay::sample_header_t header;
  planet_face::image_t image;
  };

///\brief the sample the layer wrote last, when it is newer than the one seen and whole
[[nodiscard]]
auto read_sample(uint64_t seen) -> std::optional<sample_read_t>
  {
  std::filesystem::path const path{overlay::sample_file_path()};
  std::ifstream in{path, std::ios::binary};
  if(not in)
    return std::nullopt;
  overlay::sample_header_t header{};
  in.read(reinterpret_cast<char *>(&header), sizeof(header));
  if(
    not in or header.magic != overlay::sample_header_t{}.magic or header.seq == seen or (header.seq & 1u) != 0u
    or header.width == 0u or header.height == 0u or header.width > overlay::sample_max_side
    or header.height > overlay::sample_max_side
  )
    return std::nullopt;
  std::vector<uint8_t> rgba(size_t{header.width} * header.height * 4u);
  in.read(reinterpret_cast<char *>(rgba.data()), std::streamsize(rgba.size()));
  // written over meanwhile - the next one will do
  overlay::sample_header_t after{};
  in.seekg(0);
  in.read(reinterpret_cast<char *>(&after), sizeof(after));
  if(not in or after.seq != header.seq)
    return std::nullopt;
  sample_read_t read{.header = header, .image = {.width = header.width, .height = header.height, .rgb = {}}};
  read.image.rgb.resize(size_t{header.width} * header.height * 3u);
  for(size_t at{}; at != size_t{header.width} * header.height; ++at)
    std::memcpy(read.image.rgb.data() + at * 3u, rgba.data() + at * 4u, 3u);
  return read;
  }
  }  // namespace

auto approach_views_t::observe(bool ship_view, std::string const & system, std::string const & body) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->exploration.approach_faces or not ship_view or body.empty())
    {
    target_.reset();
    return;
    }
  target_ = target_t{.system = system, .body = body};
  }

auto approach_views_t::sample_request() const -> overlay::sample_t
  {
  auto const cfg{eht::settings()};
  if(target_)
    return overlay::sample_t{
      .every_ms = sample_every_ms, .size = cfg->exploration.sky_size, .aspect = 16.f / 9.f, .width = 256u
    };
  // kept the rest of the time too when the settings say so, for looking at what the game shows
  if(eht::overlay_sample_t const & always{cfg->overlay.sample}; always.always and always.every_ms != 0u)
    return overlay::sample_t{
      .every_ms = always.every_ms, .size = always.size, .aspect = 16.f / 9.f, .width = always.width
    };
  return {};
  }

auto approach_views_t::tick(planet_faces_t & faces) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  auto const cfg{eht::settings()};

  // the real picture, once the layer has it
  if(pending_)
    {
    std::error_code ec;
    if(std::filesystem::exists(pending_->spool, ec))
      {
      QImage image{QString::fromStdString(pending_->spool.string())};
      std::filesystem::remove(pending_->spool, ec);
      if(not image.isNull())
        faces.offer_view(pending_->target.system, pending_->target.body, std::move(image));
      pending_.reset();
      }
    else if(now - pending_->asked > pending_limit)
      pending_.reset();
    }

  // a sample judged: worth a picture of the ball's rectangle, at most one an interval
  if(judging_.valid() and judging_.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    if(auto verdict{judging_.get()}; verdict and not pending_ and target_ and verdict->target.body == target_->body)
      if(now - last_asked_ >= std::chrono::milliseconds{cfg->exploration.approach_interval_ms})
        {
        std::error_code ec;
        std::filesystem::create_directories(codex_files::spool_dir(), ec);
        last_asked_ = now;
        // the moment, made unlike the other pictures' numbers: the sky album's are 4n, the glare's 4n + 2
        uint64_t const id{
          uint64_t(
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
              .count()
          ) * 4u
          + 3u
        };
        request_ = overlay::capture_t{
          .id = id,
          .path = (codex_files::spool_dir() / std::format("approach_{}.ppm", id)).string(),
          .size = cfg->exploration.sky_size,
          .delay_ms = 0u,
          .quiet = true,
          .aspect = 16.f / 9.f,
          .region_left = verdict->left,
          .region_top = verdict->top,
          .region_width = verdict->width,
          .region_height = verdict->height
        };
        pending_ = pending_t{.spool = request_.path, .target = verdict->target, .asked = now};
        }

  // a new sample, while none is being judged
  if(not target_ or (judging_.valid() and judging_.wait_for(std::chrono::seconds{0}) != std::future_status::ready))
    return;
  auto sample{read_sample(seen_seq_)};
  if(not sample)
    return;
  seen_seq_ = sample->header.seq;
  // taken long ago - the ship may have been looking elsewhere
  auto const age_ms{
    int64_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    )
    - int64_t(sample->header.taken_ms)
  };
  if(age_ms > 2000)
    return;
  judging_ = std::async(
    std::launch::async,
    [target{*target_}, read{std::move(*sample)}]() -> std::optional<verdict_t>
    {
      overlay::sample_header_t const & h{read.header};
      float const screen_per_pixel{h.region_width * float(h.surface_width) / float(h.width)};
      auto const judged{planet_face::judge_approach(read.image, screen_per_pixel)};
      // strictly better: a view as good as the one kept would only be the same face again, every interval
      if(not judged or judged->score <= planet_faces_t::best_score(target.body))
        return std::nullopt;
      // the ball's rectangle, a little larger, from the sample's pixels to shares of the whole surface
      float const r{judged->disc.radius * margin};
      auto const across = [&](float x) { return h.left + x / float(h.width) * h.region_width; };
      auto const down = [&](float y) { return h.top + y / float(h.height) * h.region_height; };
      float const left{std::max(h.left, across(judged->disc.x - r))};
      float const top{std::max(h.top, down(judged->disc.y - r))};
      float const right{std::min(h.left + h.region_width, across(judged->disc.x + r))};
      float const bottom{std::min(h.top + h.region_height, down(judged->disc.y + r))};
      if(right <= left or bottom <= top)
        return std::nullopt;
      return verdict_t{.target = target, .left = left, .top = top, .width = right - left, .height = bottom - top};
    }
  );
  }
