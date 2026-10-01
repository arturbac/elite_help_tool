#pragma once
#include <events/common.h>
#include <events/system_info.h>
#include <simple_enum/simple_enum.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

///\brief journal events: stations and settlements - docking, approaching, stepping out
namespace events
  {

enum struct station_type : uint8_t
  {
  AsteroidBase,
  Bernal,
  Coriolis,
  CraterOutpost,
  CraterPort,
  DockablePlanetStation,
  Dodec,
  FleetCarrier,
  GameplayPOI,
  MegaShip,
  Ocellus,
  OnFootSettlement,
  Orbis,
  Outpost,
  PlanetaryConstructionDepot,
  SpaceConstructionDepot,
  SurfaceStation
  };

consteval auto adl_enum_bounds(station_type)
  {
  using enum station_type;
  return simple_enum::adl_info{AsteroidBase, SurfaceStation};
  }

///\brief approaching a settlement - this is where the context for collecting comes from: the economy and government of the place
struct approach_settlement_t
  {
  uint64_t MarketID;
  uint64_t SystemAddress;
  std::string Name;
  ///\brief the body the settlement stands on
  std::optional<body_id_t> BodyID;
  std::string StationEconomy_Localised;
  std::string StationGovernment_Localised;
  ///\brief the faction holding the place - the journal gives it nested, not as a bare string
  system_faction_t StationFaction;
  ///\brief where on the body it stands - absent for a settlement approached from orbit rather than
  /// walked up to, which the journal does not explain
  std::optional<double> Latitude;
  std::optional<double> Longitude;
  };

///\brief leaving the vehicle - sometimes the only trace of the place when the arrival was by taxi
struct disembark_t
  {
  uint64_t MarketID;
  uint64_t SystemAddress;
  std::string StationName;
  std::string StationType;
  ///\brief the body stepped out onto - with OnPlanet it is the footfall, whether from the ship, an SRV or a taxi
  body_id_t BodyID{};
  bool OnPlanet{};
  };

///\brief boarding the ship, the SRV or a taxi - the commander is no longer on foot
struct embark_t
  {
  uint64_t SystemAddress{};
  bool SRV{};
  bool Taxi{};
  };

///\brief docking - this is where a station's identity comes from, its type included
///\detail StationType tells a player's carrier from an ordinary station, which is what tells selling
/// micro resources to players from dropping them at a station
struct docked_t
  {
  uint64_t MarketID;
  uint64_t SystemAddress;
  std::string StationName;
  std::string StationType;
  std::string StarSystem;
  std::string StationEconomy_Localised;
  std::string StationGovernment_Localised;
  ///\brief the faction holding the place - the journal gives it nested, not as a bare string
  system_faction_t StationFaction;
  double DistFromStarLS;
  ///\brief what the place offers - "exploration" is Universal Cartographics, where cartography is sold
  std::vector<std::string> StationServices;
  };

///\brief lifting off the pad - from this moment the market of that place stops concerning us
struct undocked_t
  {
  uint64_t MarketID;
  std::string StationName;
  ///\brief what the place is now - a construction finished while docked leaves as the station it became
  std::string StationType;
  ///\brief left in a taxi or a dropship, not in a ship of one's own
  bool Taxi;
  ///\brief left in someone else's ship, as crew
  bool Multicrew;
  };

///\brief the Market event from the journal - it carries only a header, the contents go to Market.json
struct market_t
  {
  uint64_t MarketID;
  std::string StationName;
  std::string StationType;
  std::string StarSystem;
  };

  }  // namespace events
