#pragma once
#include <events/common.h>
#include <star_system.h>
#include <chrono>
#include <span>
#include <vector>

// where the bodies of a system stand now, their orbits carried forward from their scans

///\brief every scanned body's own position at the given moment, in the order given - the moons and the
/// companion stars' own orbits added onto whatever they in turn circle, up to the system's own centre.
/// Each body's orbit is carried forward from its own scan to the given moment, so a body scanned long
/// ago and a fresh one are both placed at their true position now, not frozen at their scan's moment
[[nodiscard]]
auto body_positions_now(
  std::span<bary_centre_t const> barycentres, std::vector<body_t const *> const & scans, std::chrono::sys_seconds now
) -> std::vector<events::body_location_t>;

///\brief the same positions as body_positions_now(), walked as a sensible route: nearest-neighbour from
/// the first body given, which stands for wherever the route starts
[[nodiscard]]
auto order_calculation(
  std::span<bary_centre_t const> barycentres, std::vector<body_t const *> const & scans, std::chrono::sys_seconds now
) -> std::vector<events::body_location_t>;
