#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

///\brief database rows: stations and settlements, and who held them
namespace info
  {
///\brief a station's identity, one row per MarketID
///\detail rebuildable from journals - the Docked and Market events - so it lives in the main database
struct station_t
  {
  uint64_t market_id;
  uint64_t system_address;
  std::string name;
  std::string station_type;
  ///\brief the economy and government of the place - they say what to look for there, the settlement's name does not
  std::string economy;
  std::string government;
  ///\brief the faction holding the place - its influence is what missions handed in here raise
  std::string controlling_faction;
  ///\brief how far out from the arrival star it lies, from Docked - the journal never names the body a
  /// station orbits, but it circles close to it, so the body at the same distance is the one; 0 unknown
  double dist_from_star_ls{};
  ///\brief the body a settlement or a surface port stands on, from ApproachSettlement; unknown for
  /// a port in space - the game never tells it
  std::optional<uint32_t> body_id;
  ///\brief where on that body it stands, from ApproachSettlement - lets a distance to another
  /// settlement of the same body be a real surface distance instead of a Ls one
  std::optional<double> latitude;
  std::optional<double> longitude;
  };

///\brief whether a place stands on the ground and can be a conflict zone on foot - not a port in space,
/// a carrier, a building site, nor the colonisation ship, which calls itself a surface station under a name
/// the game never localised
[[nodiscard]]
inline auto is_ground_settlement(station_t const & station) -> bool
  {
  static constexpr std::array<std::string_view, 11> in_space{
    "Coriolis", "Orbis", "Ocellus", "Outpost", "Bernal", "Dodec", "AsteroidBase", "MegaShip", "FleetCarrier",
    "SpaceConstructionDepot", "PlanetaryConstructionDepot"
  };
  if(std::ranges::find(in_space, std::string_view{station.station_type}) != in_space.end())
    return false;
  return not station.name.starts_with('$') and not station.name.starts_with("Planetary Construction Site:")
         and not station.name.starts_with("Orbital Construction Site:");
  }

///\brief who held a settlement over a stretch of time - one row per owner in turn
///\detail a war can hand a settlement over, and what was at stake is said by whom it belonged to before
struct settlement_owner_t
  {
  int64_t oid{-1};
  uint64_t market_id;
  uint64_t system_address;
  std::string faction;
  std::chrono::sys_seconds first_seen;
  std::chrono::sys_seconds last_seen;
  };
  }  // namespace info
