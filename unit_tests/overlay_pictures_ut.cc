#include <picture_grid.h>

#include <boost/ut.hpp>

#include <cmath>

using namespace boost::ut;

namespace
  {
[[nodiscard]]
auto near(float a, float b) -> bool
  { return std::fabs(a - b) < 1e-3f; }
  }  // namespace

auto main() -> int
  {
  using overlay::pictures::grid;

  "no box: the most columns across the width, as before"_test = []
  {
    auto const g{grid(6u, 3u, 1600.f, 0.f, 8.f, 1.f)};
    expect(g.columns == 3u);
    expect(near(g.cell, (1600.f - 16.f) / 3.f));
  };

  "six square views in the lower left quarter of a 1600 x 2160 band: two columns of three"_test = []
  {
    auto const g{grid(6u, 3u, 800.f, 1080.f, 8.f, 1.f)};
    expect(g.columns == 2u);
    // three rows down the 1080 px: (1080 - 2 * 8) / 3
    expect(near(g.cell, (1080.f - 16.f) / 3.f));
  };

  "one view takes a single column as wide as the box allows"_test = []
  {
    auto const g{grid(1u, 3u, 800.f, 1080.f, 8.f, 1.f)};
    expect(g.columns == 1u);
    expect(near(g.cell, 800.f));
  };

  "a wide box puts the views side by side, up to the most columns"_test = []
  {
    auto const g{grid(6u, 3u, 1600.f, 300.f, 8.f, 1.f)};
    expect(g.columns == 3u);
    expect(near(g.cell, (300.f - 8.f) / 2.f));
  };

  "the column count is held to 1..8"_test = []
  {
    expect(grid(4u, 0u, 400.f, 0.f, 0.f, 1.f).columns == 1u);
    expect(grid(4u, 20u, 400.f, 0.f, 0.f, 1.f).columns == 8u);
  };
  }
