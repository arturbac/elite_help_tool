#include <boost/ut.hpp>
#include <route_progress.h>

#include <algorithm>
#include <vector>

namespace
  {
[[nodiscard]]
auto route_of(std::vector<uint64_t> const & systems) -> std::vector<info::neutron_waypoint_t>
  {
  std::vector<info::neutron_waypoint_t> route;
  for(uint64_t const system: systems)
    route.push_back(info::neutron_waypoint_t{.route_name = "test", .position = uint32_t(route.size()), .system = {}, .system_address = system, .loc_x = 0.0, .loc_y = 0.0, .loc_z = 0.0, .neutron = false, .distance = 0.0});
  return route;
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  // a loop: out from 1 through 2 and 3, and back through 2 to 1
  auto const loop{route_of({1u, 2u, 3u, 2u, 1u})};

  "an arrival at a waypoint puts it behind us"_test = [&]
  {
    expect(route_progress::advance(loop, 0u, 1u) == 1_ul);
    expect(route_progress::advance(loop, 1u, 3u) == 3_ul) << "a waypoint skipped over is behind us too";
  };

  "a system off the route moves nothing, back to the start least of all"_test = [&]
  { expect(route_progress::advance(loop, 2u, 99u) == 2_ul); };

  "a system passed twice counts from the next waypoint on"_test = [&]
  {
    expect(route_progress::advance(loop, 3u, 2u) == 4_ul) << "the way back's pass, not the way out's";
    expect(route_progress::advance(loop, 4u, 1u) == 5_ul) << "the return to the start is the end";
    expect(route_progress::advance(loop, 1u, 2u) == 2_ul) << "from the start the first pass comes first";
  };

  "an arrived route stays arrived"_test = [&]
  { expect(route_progress::advance(loop, 5u, 1u) == 5_ul); };

  "a route is told by its systems in order"_test = [&]
  {
    auto reversed{route_of({1u, 2u, 3u, 4u})};
    auto const forward{reversed};
    std::ranges::reverse(reversed);
    expect(route_progress::route_key(forward) == route_progress::route_key(route_of({1u, 2u, 3u, 4u})));
    expect(route_progress::route_key(forward) != route_progress::route_key(reversed)) << "reversed is another route";
    expect(route_progress::route_key(forward) != route_progress::route_key(route_of({1u, 2u, 3u})));
  };
  }
