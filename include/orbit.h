#pragma once
#include <events/common.h>
#include <star_system.h>
#include <chrono>
#include <optional>
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

///\brief one stop of a route inside a system, and how far it lies from the stop before it
struct visit_stop_t
  {
  events::body_id_t body_id{};
  double distance_ls{};
  ///\brief how far it lies from where the route starts, straight
  double from_start_ls{};
  };

///\brief the chosen bodies in the order to fly them: nearest neighbour, then 2-opt, as an open route
///\param bodies all the system's bodies, not only the chosen - a moon stands where its planet stands
///\param start the body the commander is at; the system's centre when none or when it is not among the bodies
[[nodiscard]]
auto visit_order(
  std::span<bary_centre_t const> barycentres,
  std::vector<body_t const *> const & bodies,
  std::span<events::body_id_t const> chosen,
  std::optional<events::body_id_t> start,
  std::chrono::sys_seconds now
) -> std::vector<visit_stop_t>;
