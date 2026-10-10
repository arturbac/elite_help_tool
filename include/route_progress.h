#pragma once
#include <data/navigation.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

///\brief how far along a neutron route the ship is - shared by the Route window and the overlay
namespace route_progress
  {

///\brief the waypoints behind us once the ship arrived in the system `here`
///\detail Progress only moves forward, and only to the first match from the next waypoint on: a route
/// there and back passes the same system again and its earlier place is behind us, and the game plots
/// its own course to the next waypoint through systems that are not on the list - "somewhere along the
/// way", not "back to the start"
[[nodiscard]]
auto advance(std::span<info::neutron_waypoint_t const> route, size_t reached, uint64_t here) noexcept -> size_t;

///\brief what tells one route from another: the number of waypoints and a hash of their systems in
/// flight order - the same route reversed, or loaded again from another file, is another route
[[nodiscard]]
auto route_key(std::span<info::neutron_waypoint_t const> route) -> std::string;

  }  // namespace route_progress
