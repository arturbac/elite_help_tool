#include <descent.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace descent
  {
namespace
  {
constexpr double rad{std::numbers::pi / 180.0};
///\brief the shortest window speeds are read over - Status.json is written about every half second, and a
/// late write would make one step look like a dive or a halt
constexpr std::chrono::seconds speed_window{2};
///\brief how long the profile reaches back
constexpr std::chrono::minutes kept{5};

///\brief the angle between two points of a sphere, radians - haversine, which keeps the metres near the place
[[nodiscard]]
auto central_angle(bio::surface_point_t a, bio::surface_point_t b) noexcept -> double
  {
  double const a1{a.latitude * rad};
  double const a2{b.latitude * rad};
  double const sl{std::sin((a2 - a1) / 2.0)};
  double const so{std::sin((b.longitude - a.longitude) * rad / 2.0)};
  return 2.0 * std::asin(std::min(1.0, std::sqrt(sl * sl + std::cos(a1) * std::cos(a2) * so * so)));
  }
  }  // namespace

auto glide_to(bio::surface_point_t here, double altitude_m, bio::surface_point_t place, double radius_m) noexcept
  -> glide_t
  {
  double const c{central_angle(here, place)};
  double const horizontal{radius_m * std::sin(c)};
  double const vertical{radius_m + altitude_m - radius_m * std::cos(c)};
  return glide_t{
    .distance_m = c * radius_m,
    .below_deg = horizontal == 0.0 and vertical == 0.0 ? 0.0 : std::atan2(vertical, horizontal) / rad
  };
  }

auto track_t::push(fix_t fix, double radius_m) -> void
  {
  if(radius_m != radius_m_)
    {
    fixes_.clear();
    radius_m_ = radius_m;
    }
  // the same fix read again is no news - the file is written while nothing moves, too
  if(not fixes_.empty() and fixes_.back().point.latitude == fix.point.latitude
     and fixes_.back().point.longitude == fix.point.longitude and fixes_.back().altitude_m == fix.altitude_m)
    return;
  fixes_.push_back(fix);
  while(fixes_.size() > 2u and fix.at - fixes_.front().at > kept)
    fixes_.pop_front();
  }

auto track_t::window_start(clock::time_point now) const -> fix_t const *
  {
  if(fixes_.size() < 2u or now - fixes_.back().at > speed_window)
    return nullptr;
  for(auto it{fixes_.rbegin() + 1}; it != fixes_.rend(); ++it)
    if(fixes_.back().at - it->at >= speed_window)
      return &*it;
  return nullptr;
  }

auto track_t::vertical_speed(clock::time_point now) const -> std::optional<double>
  {
  fix_t const * const start{window_start(now)};
  if(start == nullptr)
    return std::nullopt;
  double const seconds{std::chrono::duration<double>(fixes_.back().at - start->at).count()};
  return (fixes_.back().altitude_m - start->altitude_m) / seconds;
  }

auto track_t::path_angle_deg(clock::time_point now) const -> std::optional<double>
  {
  fix_t const * const start{window_start(now)};
  if(start == nullptr)
    return std::nullopt;
  fix_t const & end{fixes_.back()};
  double const horizontal{central_angle(start->point, end.point) * (radius_m_ + (start->altitude_m + end.altitude_m) / 2.0)};
  double const vertical{end.altitude_m - start->altitude_m};
  if(horizontal == 0.0 and vertical == 0.0)
    return std::nullopt;
  return std::atan2(vertical, horizontal) / rad;
  }

auto profile(std::deque<fix_t> const & fixes, bio::surface_point_t place, double radius_m) -> std::vector<profile_point_t>
  {
  std::vector<profile_point_t> points;
  points.reserve(fixes.size());
  for(fix_t const & fix: fixes)
    points.push_back(profile_point_t{.distance_m = central_angle(fix.point, place) * radius_m, .altitude_m = fix.altitude_m});
  return points;
  }
  }  // namespace descent
