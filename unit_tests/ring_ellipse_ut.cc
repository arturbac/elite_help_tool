#include <ring_ellipse.h>

#include <boost/ut.hpp>

#include <numbers>

using namespace boost::ut;

namespace
  {
auto near_to(float a, float b) -> bool { return std::fabs(a - b) < 0.001f; }
  }  // namespace

suite<"ring_ellipse"> ring_ellipse_suite = []
{
  "a single ring reaches as far as a ring may"_test = []
  {
    expect(near_to(overlay::ring_radius(6.5f, 0u, 1u, 9.f), 9.f));
    expect(near_to(overlay::ring_radius(4.5f, 0u, 1u, 9.f), 9.f));
  };

  "a small moon's ring stays twice the moon"_test = []
  { expect(near_to(overlay::ring_radius(2.f, 0u, 1u, 9.f), 4.f)); };

  "rings share the room evenly, inside out"_test = []
  {
    expect(near_to(overlay::ring_radius(6.f, 0u, 3u, 9.f), 7.f));
    expect(near_to(overlay::ring_radius(6.f, 1u, 3u, 9.f), 8.f));
    expect(near_to(overlay::ring_radius(6.f, 2u, 3u, 9.f), 9.f));
  };

  "a ball wider than the reach keeps its rings on its edge, not inside it"_test = []
  { expect(near_to(overlay::ring_radius(12.f, 0u, 1u, 9.f), 12.f)); };

  "a tilt of half a turn is the same ellipse"_test = []
  {
    float constexpr pi{std::numbers::pi_v<float>};
    expect(near_to(overlay::ring_lean(0.3), 0.3f));
    expect(near_to(overlay::ring_lean(0.3 - std::numbers::pi), 0.3f));
    expect(near_to(overlay::ring_lean(-0.3), -0.3f));
    expect(std::fabs(overlay::ring_lean(-3.1)) <= pi / 2.f);
  };

  "an unleaning ring runs from side to side, its near half below the middle"_test = []
  {
    auto const near{overlay::half_ring(10.f, 0.f, true)};
    auto const far{overlay::half_ring(10.f, 0.f, false)};
    expect(near_to(near.front().x, 10.f) and near_to(near.back().x, -10.f));
    expect(near_to(near[overlay::ring_half_points / 2u].y, 10.f * overlay::ring_flatness));
    expect(near_to(far[overlay::ring_half_points / 2u].y, -10.f * overlay::ring_flatness));
  };

  "the two halves meet at the ends whatever the lean"_test = []
  {
    auto const near{overlay::half_ring(10.f, 0.7f, true)};
    auto const far{overlay::half_ring(10.f, 0.7f, false)};
    expect(near_to(near.front().x, far.front().x) and near_to(near.front().y, far.front().y));
    expect(near_to(near.back().x, far.back().x) and near_to(near.back().y, far.back().y));
  };
};

auto main() -> int {}
