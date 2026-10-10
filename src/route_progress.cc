#include <route_progress.h>

#include <algorithm>
#include <format>
#include <ranges>

namespace route_progress
  {
auto advance(std::span<info::neutron_waypoint_t const> route, size_t reached, uint64_t here) noexcept -> size_t
  {
  if(reached >= route.size())
    return reached;
  auto const ahead{route.subspan(reached)};
  auto const found{std::ranges::find(ahead, here, &info::neutron_waypoint_t::system_address)};
  if(found == ahead.end())
    return reached;
  return reached + size_t(std::ranges::distance(ahead.begin(), found)) + 1u;
  }

auto route_key(std::span<info::neutron_waypoint_t const> route) -> std::string
  {
  // FNV-1a over the addresses' bytes - stable between builds and machines, unlike std::hash
  uint64_t hash{0xcbf29ce484222325ull};
  for(info::neutron_waypoint_t const & waypoint: route)
    for(unsigned shift{}; shift != 64u; shift += 8u)
      {
      hash ^= (waypoint.system_address >> shift) & 0xffu;
      hash *= 0x100000001b3ull;
      }
  return std::format("{}-{:016x}", route.size(), hash);
  }
  }  // namespace route_progress
