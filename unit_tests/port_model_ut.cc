#include <port_model.h>

#include <boost/ut.hpp>

#include <algorithm>
#include <cmath>

using namespace boost::ut;

namespace
  {
///\brief twice the area, positive when the polygon goes clockwise with y growing downwards
[[nodiscard]]
auto turn(port_model::facet_t const & facet) -> float
  {
  float sum{};
  for(size_t i{}; i != facet.points.size(); ++i)
    {
    auto const & a{facet.points[i]};
    auto const & b{facet.points[(i + 1u) % facet.points.size()]};
    sum += a[0] * b[1] - b[0] * a[1];
    }
  return sum;
  }

constexpr std::array kinds{
  port_model::kind_e::coriolis,
  port_model::kind_e::orbis,
  port_model::kind_e::ocellus,
  port_model::kind_e::dodec,
  port_model::kind_e::outpost,
  port_model::kind_e::asteroid
};
  }  // namespace

int main()
  {
  "every model fits its circle and fills clockwise"_test = []
  {
    for(port_model::kind_e const kind: kinds)
      {
      auto const facets{port_model::facets(kind, 5.f, -1.f, 0.f)};
      expect(facets.size() > 4u) << static_cast<int>(kind);
      expect(facets.size() < 200u) << static_cast<int>(kind);
      for(port_model::facet_t const & facet: facets)
        {
        expect(facet.points.size() >= 3u);
        expect(turn(facet) > 0.f) << static_cast<int>(kind);
        expect(facet.shade >= 0.f and facet.shade <= 1.f);
        for(auto const & p: facet.points)
          expect(std::hypot(p[0], p[1]) <= 5.001f) << static_cast<int>(kind);
        }
      }
  };

  "the Coriolis and the Dodec show their slot, over a face of the hull"_test = []
  {
    for(port_model::kind_e const kind: {port_model::kind_e::coriolis, port_model::kind_e::dodec})
      {
      auto const facets{port_model::facets(kind, 1.f, 0.f, -1.f)};
      auto const slot{std::ranges::find(facets, port_model::part_e::slot, &port_model::facet_t::part)};
      expect(slot != facets.end() and slot != facets.begin()) << static_cast<int>(kind);
      }
  };

  "the side towards the star is the brighter one"_test = []
  {
    auto const brightness = [](float light_x) -> float
    {
      float left{};
      float right{};
      for(port_model::facet_t const & facet: port_model::facets(port_model::kind_e::ocellus, 1.f, light_x, 0.f))
        {
        float x{};
        for(auto const & p: facet.points)
          x += p[0];
        (x < 0.f ? left : right) += facet.shade;
        }
      return left - right;
    };
    expect(brightness(-1.f) > 0.f);
    expect(brightness(1.f) < 0.f);
  };
  }
