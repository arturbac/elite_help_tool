#pragma once
#include <events/common.h>
#include <events/system_info.h>
#include <simple_enum/simple_enum.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

///\brief journal events: jumps, routes and where the ship is
namespace events
  {

struct nav_route_t
  {
  struct item_t
    {
    std::string StarSystem;
    uint64_t SystemAddress;
    std::array<double, 3> StarPos;
    std::string StarClass;
    };

  std::vector<nav_route_t::item_t> Route;
  };

struct nav_route_clear_t
  {
  };

struct fsd_target_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Name;
  std::string StarClass;
  uint64_t SystemAddress;
  uint32_t RemainingJumpsInRoute;
  };

enum struct jump_type_e
  {
  Hyperspace,
  Supercruise
  };

consteval auto adl_enum_bounds(jump_type_e)
  {
  using enum jump_type_e;
  return simple_enum::adl_info{Hyperspace, Supercruise};
  }

///\brief When written: at the start of a Hyperspace or Supercruise jump (start of countdown)
struct start_jump_t
  {
  std::chrono::sys_seconds timestamp;
  jump_type_e JumpType;
  std::optional<std::string> StarSystem;
  std::optional<uint64_t> SystemAddress;
  std::optional<std::string> StarClass;
  std::optional<bool> Taxi;
  };

///\brief When written: when jumping from one star system to another
///\detail Note, when following a multi-jump route, this will typically appear for the next star, during a jump, ie
/// after "StartJump" but before the "FSDJump"
struct fsd_jump_t
  {
  std::chrono::sys_seconds timestamp;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::array<double, 3> StarPos;  // [x, y, z]
  std::string Body;
  double JumpDist;
  double FuelUsed;
  double FuelLevel;
  int BoostUsed;
  bool Taxi;
  bool Multicrew;

  [[nodiscard]]
  constexpr auto player_position() const noexcept -> body_location_t
    { return body_location_t{{}, StarPos[0], StarPos[1], StarPos[2]}; }

  system_faction_t SystemFaction;
  std::string SystemAllegiance;
  std::string SystemEconomy_Localised;
  std::string SystemSecondEconomy_Localised;
  std::string SystemGovernment_Localised;
  std::string SystemSecurity_Localised;
  std::string ControllingPower;
  std::string PowerplayState;
  double PowerplayStateControlProgress;
  uint32_t PowerplayStateReinforcement;
  uint32_t PowerplayStateUndermining;
  std::vector<std::string> Powers;
  uint64_t Population;

  bool Wanted;
  std::vector<faction_info_t> Factions;
  std::vector<conflict_t> Conflicts;
  };

/*
{ 
"timestamp":"2026-09-04T01:49:45Z",
"event":"FSDJump",
"Taxi":false,
"Multicrew":false,
"StarSystem":"M25 Sector PI-T d3-51",
"SystemAddress":1762740111931,
"StarPos":[-419.03125,-153.18750,2099.09375],
"SystemAllegiance":"",
"SystemEconomy":"$economy_None;",
"SystemEconomy_Localised":"None",
"SystemSecondEconomy":"$economy_None;",
"SystemSecondEconomy_Localised":"None",
"SystemGovernment":"$government_None;",
"SystemGovernment_Localised":"None",
"SystemSecurity":"$GAlAXY_MAP_INFO_state_anarchy;",
"SystemSecurity_Localised":"Anarchy",
"Population":0,
"Body":"M25 Sector PI-T d3-51",
"BodyID":0,
"BodyType":"Star",
"JumpDist":454.287,
"FuelUsed":6.287253,
"FuelLevel":38.012375,
"BoostUsed":4
  }

 */
struct location_t
  {
  system_faction_t SystemFaction;
  bool Docked;
  ///\brief started on foot - in a station's concourse the game gives no StationName, only Body
  bool OnFoot{};
  ///\brief the port, only when Docked
  std::string StationName;
  uint64_t MarketID{};
  bool Taxi;
  bool Multicrew;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::array<double, 3> StarPos;
  std::string SystemAllegiance;
  std::string SystemEconomy_Localised;
  std::string SystemSecondEconomy_Localised;
  std::string SystemGovernment_Localised;
  std::string SystemSecurity_Localised;
  uint64_t Population;
  std::string Body;
  body_id_t BodyID;
  std::string BodyType;

  std::vector<faction_info_t> Factions;
  std::vector<conflict_t> Conflicts;
  };

///\brief entering supercruise - it ends the stay at a settlement
struct supercruise_entry_t
  {
  uint64_t SystemAddress;
  };

///\brief leaving supercruise anywhere - at a destination, by hand, or thrown out of it
struct supercruise_exit_t
  {
  uint64_t SystemAddress;
  std::string Body;
  std::string BodyType;
  };

///\brief leaving supercruise at the destination it was flown to - written just before SupercruiseExit, and only
/// when the drop was the destination's own: a port's name in Type, with its MarketID
struct supercruise_destination_drop_t
  {
  std::string Type;
  std::string Type_Localised;
  uint64_t MarketID;
  };

///\brief the frame shift drive supercharged in a neutron star's or a white dwarf's cone
struct jet_cone_boost_t
  {
  double BoostValue{};
  };

  }  // namespace events
