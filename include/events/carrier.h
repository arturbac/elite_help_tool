#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

///\brief journal events: fleet carriers - their stats, their bartender, their jumps
namespace events
  {

struct carrier_space_sage_t
  {
  uint32_t TotalCapacity;
  uint32_t Crew;
  uint32_t Cargo;
  uint32_t CargoSpaceReserved;
  uint32_t ShipPacks;
  uint32_t ModulePacks;
  uint32_t FreeSpace;
  };

struct carrier_finance_t
  {
  uint64_t CarrierBalance;
  uint64_t ReserveBalance;
  uint64_t AvailableBalance;
  uint8_t ReservePercent;
  uint8_t TaxRate_refuel;
  };

struct carrier_stats_t
  {
  ///\brief a number, not the callsign - the same as this carrier's MarketID. FCMaterials.json names
  /// its own field the same way but keeps the callsign in it, so the two sources may be joined
  /// through Callsign alone
  uint64_t CarrierID;
  std::string Callsign;
  std::string Name;
  std::string CarrierType;
  std::string DockingAccess;
  uint16_t FuelLevel;
  double JumpRangeCurr;
  double JumpRangeMax;

  carrier_space_sage_t SpaceUsage;
  carrier_finance_t Finance;
  };

struct fcmaterial_t
{
  uint64_t id;
  ///\brief the internal name, the key shared with micro resource sales
  std::string Name;
  std::string Name_Localised;
  uint32_t Price;
  uint32_t Stock;
  uint32_t Demand;
};

struct fcmaterials_t
{
  std::chrono::sys_seconds timestamp;
  uint64_t MarketID;
  std::string CarrierName;
  std::string CarrierID;
  std::vector<fcmaterial_t> Items;
};

///\brief a carrier's jump ordered - where to and when it leaves; one's own carrier or the squadron's
struct carrier_jump_request_t
  {
  std::string CarrierType;
  uint64_t CarrierID{};
  std::string SystemName;
  std::string Body;
  uint64_t SystemAddress{};
  std::chrono::sys_seconds DepartureTime;
  };

///\brief where a carrier is - written at login, and a minute after a jump when in the game
struct carrier_location_t
  {
  std::string CarrierType;
  uint64_t CarrierID{};
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

struct carrier_jump_cancelled_t
  {
  std::string CarrierType;
  uint64_t CarrierID{};
  };

///\brief a carrier's jump seen from aboard it - the carrier is the station docked at
struct carrier_jump_t
  {
  std::string StarSystem;
  uint64_t SystemAddress{};
  std::string Body;
  std::string StationType;
  uint64_t MarketID{};
  };

  }  // namespace events
