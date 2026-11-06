#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

///\brief the rings of a planet on the system map: thin ellipses round the ball, leaning as the planet leans
namespace overlay
  {
struct ring_point_t
  {
  float x{};
  float y{};
  };

///\brief how much of its length an ellipse is high
inline constexpr float ring_flatness{0.3f};
inline constexpr size_t ring_half_points{17u};

///\brief the half length of ring number index of count, inside out
///\detail a ring is four times its planet on average and the columns of the map are not, so the sizes keep
/// only an order: the rings share evenly the room between the ball and reach, or twice the ball if less
[[nodiscard]]
inline auto ring_radius(float ball, size_t index, size_t count, float reach) noexcept -> float
  {
  float const outer{std::fmax(ball, std::fmin(ball * 2.f, reach))};
  return ball + (outer - ball) * static_cast<float>(index + 1u) / static_cast<float>(count);
  }

///\brief the lean of an ellipse from a planet's axial tilt in radians, -pi/2 to pi/2: turned half way round
/// an ellipse is the same ellipse
[[nodiscard]]
inline auto ring_lean(double axial_tilt) noexcept -> float
  {
  return static_cast<float>(std::remainder(axial_tilt, std::numbers::pi));
  }

///\brief the near or the far half of an ellipse as offsets from the ball's middle, y growing downwards: the
/// near half passes below the middle of the ball before the lean turns it
[[nodiscard]]
inline auto half_ring(float radius, float lean, bool near) noexcept -> std::array<ring_point_t, ring_half_points>
  {
  std::array<ring_point_t, ring_half_points> points;
  float const c{std::cos(lean)};
  float const s{std::sin(lean)};
  for(size_t i{}; i != ring_half_points; ++i)
    {
    float const t{std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(ring_half_points - 1u)};
    float const along{radius * std::cos(t)};
    float const across{radius * ring_flatness * std::sin(t) * (near ? 1.f : -1.f)};
    points[i] = ring_point_t{.x = along * c - across * s, .y = along * s + across * c};
    }
  return points;
  }
  }  // namespace overlay
