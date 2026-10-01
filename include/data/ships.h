#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

///\brief database rows: the commander's ships - where each is, transfers, and the ports the escape pod returns to
namespace info
  {
///\brief a ship in transit between ports
///
/// The game gives the delivery time once, at the order, and never mentions it again - arrival has
/// no event of its own. Unless that one moment is stored, the information is lost
struct ship_transfer_t
  {
  int64_t oid{-1};
  uint64_t ship_id;
  std::string ship_type;
  std::string from_system;
  uint64_t to_market_id;
  double distance;
  uint64_t price;
  std::chrono::sys_seconds ordered;
  ///\brief worked out at the order: the moment of ordering plus the delivery time
  std::chrono::sys_seconds arrives;
  };

///\brief one ship of the fleet and the place it was last known at
///
/// The full list comes only with StoredShips, written on opening a shipyard; between two of them the
/// swaps, purchases, sales and transfers move single ships. The ship flown has no place of its own -
/// it is wherever the commander is
struct ship_t
  {
  uint64_t ship_id;
  ///\brief the internal name, lower case - the journal writes it in either case
  std::string ship_type;
  ///\brief the name the game shows, when the journal gave one; the internal name otherwise
  std::string type_name;
  std::string name;
  std::string ident;
  std::string system;
  std::string station;
  ///\brief the port it stands in - for a carrier its id, so the ship follows the carrier's jumps
  uint64_t market_id;
  uint64_t value;
  bool hot;
  ///\brief the one being flown
  bool current;
  ///\brief ordered to another port; system, station and market are then the destination
  bool in_transit;
  ///\brief when a transfer arrives, empty when none was seen ordered
  std::chrono::sys_seconds arrives;
  ///\brief when this place was last told
  std::chrono::sys_seconds seen;
  ///\brief when the commander last left a pad in it; empty when never seen undocking
  std::chrono::sys_seconds flown;
  };

///\brief a port we stood at with a ship
///
/// Only a place one docks at with a ship counts - on-foot settlements and carriers are not
/// ports in the sense of the escape pod, which returns you to the last port, not to the last
/// place. The station type is stored, because it is what decides
struct port_visit_t
  {
  uint64_t market_id;
  std::string name;
  std::string system;
  std::string station_type;
  std::chrono::sys_seconds visited;
  };

///\brief whether the escape pod can send you back to this place
///
/// The pod returns you to the last PORT, not to the last place you stopped at. On-foot settlements are
/// out, because they have no landing pad for a ship. A carrier is out despite its pads - checked on
/// 26.09.2026: after a stop at W1V-NXM at 14:33 the pod sent us to Arkush City, where the stop had been at
/// 14:16. Construction sites are not safe ports either, orbital and planetary alike, nor is the
/// colonisation ship, which is a system's first site under another name
[[nodiscard]]
inline auto is_escape_pod_port(std::string_view station_type, std::string_view station_name) noexcept -> bool
  {
  return not station_type.empty() and station_type != "OnFootSettlement" and station_type != "FleetCarrier"
         and station_type != "PlanetaryConstructionDepot" and station_type != "SpaceConstructionDepot"
         and not station_name.starts_with("$EXT_PANEL_ColonisationShip");
  }
  }  // namespace info
