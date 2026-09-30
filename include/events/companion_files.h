#pragma once
#include <simple_enum/expected.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

///\brief journal events: the files the game keeps beside the journals - Status.json, Market.json, Cargo.json
namespace events
  {

///\brief a row from Cargo.json
struct cargo_item_t
  {
  std::string Name;
  std::string Name_Localised;
  uint32_t Count;
  uint32_t Stolen;
  };

///\brief the contents of Cargo.json, a file overwritten on every change of cargo
struct cargo_file_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Vessel;
  uint32_t Count;
  std::vector<cargo_item_t> Inventory;
  };

///\brief a market row from Market.json
struct market_commodity_t
  {
  uint64_t id;
  ///\brief the internal name, "$steel_name;"
  std::string Name;
  std::string Name_Localised;
  std::string Category_Localised;
  uint32_t BuyPrice;
  uint32_t SellPrice;
  ///\brief the galactic average, a constant of the commodity - the reference point for a price
  uint32_t MeanPrice;
  uint32_t Stock;
  uint32_t Demand;
  ///\brief a price alone means nothing - a station quotes one for commodities it does not trade either
  bool Producer;
  bool Consumer;
  };

///\brief what the game is showing right now, from the Status.json it keeps beside the journals
///\detail the file is rewritten whenever anything in it changes, so it answers questions the journal
/// never does - among them which interface is open, which no amount of reading events can tell
struct status_file_t
  {
  std::chrono::sys_seconds timestamp;
  uint64_t Flags;
  ///\brief 0 none, 1-4 the cockpit panels, 5 station services, 6 galaxy map, 7 system map,
  /// 8 orrery, 9 FSS, 10 surface scanner, 11 codex
  uint32_t GuiFocus;
  ///\brief the body we are near or on, by its full name - absent in open space; on foot in an orbital
  /// station it is the station's name
  std::string BodyName;
  ///\brief where the ship is set to go - only the system when it lies elsewhere, the station and the
  /// body it is on once inside the same system
  struct destination_t
    {
    uint64_t System;
    uint32_t Body;
    std::string Name;
    };
  std::optional<destination_t> Destination;
  ///\brief on foot, in a taxi, in a hangar - the ship's Flags know nothing of the commander's own legs
  uint64_t Flags2;
  ///\brief the place on the body's surface, present only near one or on it - degrees, altitude in metres
  std::optional<double> Latitude;
  std::optional<double> Longitude;
  std::optional<double> Altitude;
  std::optional<double> Heading;
  ///\brief in metres; what turns two points in degrees into a walk in metres
  std::optional<double> PlanetRadius;
  ///\brief what the commander holds on foot - the genetic sampler says a sample is being taken
  std::string SelectedWeapon;
  std::string SelectedWeapon_Localised;
  ///\brief the commander's standing with the law in this system - Clean, Wanted, Hostile, Speeding,
  /// IllegalCargo, PassengerWanted, Warrant. The only word of bounties the game gives: their sums are
  /// in no file, and a squadron's Notoriety Decay changes them without a trace
  std::string LegalState;
  };

///\brief the contents of Market.json, a file overwritten at every docking
struct market_file_t
  {
  std::chrono::sys_seconds timestamp;
  uint64_t MarketID;
  std::string StationName;
  std::string StationType;
  std::string StarSystem;
  std::vector<market_commodity_t> Items;
  };

  }  // namespace events

///\brief reads Status.json; it is absent until the game has run once
[[nodiscard]]
auto load_status(std::string journal_dir_path) -> cxx23::expected<events::status_file_t, std::error_code>;

///\brief loads the Market.json sitting next to the journals
[[nodiscard]]
auto load_market(std::string journal_dir_path) -> cxx23::expected<events::market_file_t, std::error_code>;

///\brief Cargo.json beside the journals, overwritten - it says what is aboard right now
[[nodiscard]]
auto load_cargo(std::string journal_dir_path) -> cxx23::expected<events::cargo_file_t, std::error_code>;
