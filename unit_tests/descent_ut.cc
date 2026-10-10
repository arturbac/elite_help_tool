#include <boost/ut.hpp>
#include <descent.h>

#include <cmath>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;
  constexpr double radius{1'000'000.0};

  "the line to a place a degree away and ten kilometres down"_test = []
  {
    auto const glide{descent::glide_to({0.0, 0.0}, 10'000.0, {0.0, 1.0}, radius)};
    // a degree of a thousand kilometre body, and the place below the horizon by the height and the curve
    expect(std::abs(glide.distance_m - 17'453.3) < 1.0) << glide.distance_m;
    expect(std::abs(glide.below_deg - 30.19) < 0.05) << glide.below_deg;
  };

  "straight over the place it lies straight down"_test = []
  {
    auto const glide{descent::glide_to({10.0, 20.0}, 500.0, {10.0, 20.0}, radius)};
    expect(glide.distance_m < 0.001);
    expect(std::abs(glide.below_deg - 90.0) < 0.001);
  };

  "speeds and the angle come from two seconds of fixes, not one step"_test = []
  {
    descent::track_t track;
    auto const start{std::chrono::steady_clock::time_point{} + 100s};
    track.push({.at = start, .point = {0.0, 0.0}, .altitude_m = 1'000.0}, radius);
    track.push({.at = start + 1s, .point = {0.0, 0.0001}, .altitude_m = 990.0}, radius);
    expect(not track.vertical_speed(start + 1s).has_value()) << "one second is too short to say";
    track.push({.at = start + 2s, .point = {0.0, 0.0002}, .altitude_m = 980.0}, radius);
    auto const vs{track.vertical_speed(start + 2s)};
    expect(vs.has_value() and std::abs(*vs + 10.0) < 0.001);
    auto const angle{track.path_angle_deg(start + 2s)};
    // 0.0002 degrees of a thousand kilometres is about 3.5 m against 20 m down
    expect(angle.has_value() and *angle < -79.0 and *angle > -81.0) << angle.value_or(0.0);
    expect(not track.vertical_speed(start + 10s).has_value()) << "an old track says nothing of now";
  };

  "another body starts the track over"_test = []
  {
    descent::track_t track;
    auto const start{std::chrono::steady_clock::time_point{} + 100s};
    track.push({.at = start, .point = {0.0, 0.0}, .altitude_m = 1'000.0}, radius);
    track.push({.at = start + 1s, .point = {0.0, 0.0}, .altitude_m = 900.0}, radius * 2.0);
    expect(track.fixes().size() == 1_u);
  };

  "a height from the other level starts the track over: the step between them is the ground's"_test = []
  {
    descent::track_t track;
    auto const start{std::chrono::steady_clock::time_point{} + 100s};
    track.push({.at = start, .point = {0.0, 0.0}, .altitude_m = 20'000.0, .over_mean_radius = true}, radius);
    track.push({.at = start + 1s, .point = {0.0, 0.0001}, .altitude_m = 19'900.0, .over_mean_radius = true}, radius);
    track.push({.at = start + 2s, .point = {0.0, 0.0002}, .altitude_m = 16'500.0}, radius);
    expect(track.fixes().size() == 1_u);
    expect(not track.vertical_speed(start + 2s).has_value()) << "no speed across the step";
  };

  "the profile is the distance to the place against the height"_test = []
  {
    std::deque<descent::fix_t> const fixes{
      descent::fix_t{.at = {}, .point = {0.0, 1.0}, .altitude_m = 5'000.0},
      descent::fix_t{.at = {}, .point = {0.0, 0.0}, .altitude_m = 100.0}
    };
    auto const points{descent::profile(fixes, {0.0, 0.0}, radius)};
    expect(points.size() == 2_u);
    expect(std::abs(points[0].distance_m - 17'453.3) < 1.0 and points[1].distance_m < 0.001);
    expect(points[0].altitude_m == 5'000.0_d and points[1].altitude_m == 100.0_d);
  };
  }
