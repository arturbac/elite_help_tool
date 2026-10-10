#pragma once

#include <biology.h>

#include <chrono>
#include <deque>
#include <optional>
#include <vector>

///\brief the way down to a place on a body's surface, from what Status.json gives every half second: where
/// over the body the ship is and how high (PoC)
///\detail the place is taken at the body's mean radius - its own height is not known - so near it the angles
/// say less than they seem to; below a few kilometres the positions Status.json gives repeat and jump, and the
/// angle of the flight path read from them is noise
namespace descent
  {
///\brief the straight line from the ship to the place
struct glide_t
  {
  ///\brief along the surface, in metres
  double distance_m{};
  ///\brief how far below the local horizon the place lies, in degrees; negative when it stands above it
  double below_deg{};
  };

[[nodiscard]]
auto glide_to(bio::surface_point_t here, double altitude_m, bio::surface_point_t place, double radius_m) noexcept
  -> glide_t;

///\brief one reading of where the ship is
struct fix_t
  {
  std::chrono::steady_clock::time_point at;
  bio::surface_point_t point;
  double altitude_m{};
  };

///\brief the last few minutes of fixes over one body: the speeds and the angle of the path flown, and
/// the profile of the descent so far
class track_t
  {
public:
  using clock = std::chrono::steady_clock;

  ///\brief a fix over the body of this radius; another radius is another body, and the track starts over
  auto push(fix_t fix, double radius_m) -> void;
  auto clear() -> void { fixes_.clear(); }

  ///\brief metres a second, + climbing, over the last two seconds or more of fixes
  [[nodiscard]]
  auto vertical_speed(clock::time_point now) const -> std::optional<double>;

  ///\brief the angle of the path flown, degrees, + climbing, over the same window
  [[nodiscard]]
  auto path_angle_deg(clock::time_point now) const -> std::optional<double>;

  ///\brief the fixes kept, the oldest first
  [[nodiscard]]
  auto fixes() const -> std::deque<fix_t> const & { return fixes_; }

private:
  ///\brief the oldest fix of the window ending at the newest one, when the window holds enough time
  [[nodiscard]]
  auto window_start(clock::time_point now) const -> fix_t const *;

  std::deque<fix_t> fixes_;
  double radius_m_{};
  };

///\brief one point of the profile: how far from the place along the surface, and how high
struct profile_point_t
  {
  double distance_m{};
  double altitude_m{};
  };

///\brief the descent flown so far, as distance to the place against height, the oldest first
[[nodiscard]]
auto profile(std::deque<fix_t> const & fixes, bio::surface_point_t place, double radius_m) -> std::vector<profile_point_t>;
  }  // namespace descent
