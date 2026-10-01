#pragma once
#include <array>
#include <cstdint>
#include <string>

///\brief database rows: positions, routes and distances
namespace info
  {
using space_location_t = std::array<double, 3>;

struct route_item_t
  {
  std::string system;
  uint64_t system_address;
  /// star position in light years
  space_location_t star_location;
  std::string star_class;
  double distance;
  bool visited;
  };

///\brief a waypoint of a stored neutron route
///
/// A route plotted outside the game (spansh) and loaded from a file - the game stores it nowhere, and
/// NavRoute is overwritten with every course set, so neutron jumps plotted one at a time
/// would wipe it constantly. A route flown regularly should be remembered, and since it cannot
/// be rebuilt from journals, it lives in the database gathered live
struct neutron_waypoint_t
  {
  int64_t oid{-1};
  ///\brief the route's name, the same on all its waypoints - one is remembered, the one flown
  /// regularly; one-off routes live only until the window closes and reach nowhere
  std::string route_name;
  ///\brief flight order - after any reversal, so zero is the first hop
  uint32_t position;
  std::string system;
  uint64_t system_address;
  double loc_x;
  double loc_y;
  double loc_z;
  ///\brief whether this is a neutron star, that is a stop for a supercharge
  bool neutron;
  ///\brief the distance from the previous waypoint in light years
  double distance;
  };

constexpr double light_speed_mps = 299'792'458.0;

///\returns distance in Ly
[[nodiscard]]
auto distance(space_location_t const & loc1, space_location_t const & loc2) -> double;
  }  // namespace info
