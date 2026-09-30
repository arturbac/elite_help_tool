#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

///\brief journal events: the ship flown and the fleet at the shipyards
namespace events
  {

struct cargo_t
  {
  ///\brief Ship or SRV - only what stays on the ship counts
  std::string Vessel;
  uint32_t Count;
  };

struct fuel_scoop_t
  {
  float Scooped;
  float Total;
  };

struct fuel_capacity_t
  {
  float Main;
  float Reserve;
  };

struct module_t
  {
  std::string Slot;
  std::string Item;  //": "mandalay_armour_grade1",
  bool On;
  uint8_t Priority;
  float Health;
  };

struct loadout_t
  {
  std::string Ship;
  uint32_t ShipID;
  std::string ShipName;
  std::string ShipIdent;
  uint32_t HullValue;
  uint32_t ModulesValue;
  float HullHealth;
  float UnladenMass;
  uint16_t CargoCapacity;
  float MaxJumpRange;
  uint32_t Rebuy;
  fuel_capacity_t FuelCapacity;
  std::vector<module_t> Modules;
  };

///\brief changing ships at a shipyard - the ship left behind keeps its hold
struct shipyard_swap_t
  {
  std::string ShipType;
  std::string ShipType_Localised;
  ///\brief the ship taken
  uint64_t ShipID{};
  ///\brief the ship left here, unless it was sold in the same move
  std::string StoreOldShip;
  std::optional<uint64_t> StoreShipID;
  std::optional<uint64_t> SellShipID;
  uint64_t MarketID{};
  };

///\brief a ship waiting at the shipyard we stand in
struct stored_ship_here_t
  {
  uint64_t ShipID{};
  std::string ShipType;
  std::string ShipType_Localised;
  std::string Name;
  uint64_t Value{};
  bool Hot{};
  };

///\brief a ship waiting elsewhere, or on its way - one in transit has no system
struct stored_ship_remote_t
  {
  uint64_t ShipID{};
  std::string ShipType;
  std::string ShipType_Localised;
  std::string Name;
  std::string StarSystem;
  uint64_t ShipMarketID{};
  uint64_t TransferPrice{};
  ///\brief seconds it would take to bring it here
  uint64_t TransferTime{};
  uint64_t Value{};
  bool Hot{};
  bool InTransit{};
  };

///\brief every ship but the one flown, written on opening a shipyard - the whole fleet at once
struct stored_ships_t
  {
  std::string StationName;
  uint64_t MarketID{};
  std::string StarSystem;
  std::vector<stored_ship_here_t> ShipsHere;
  std::vector<stored_ship_remote_t> ShipsRemote;
  };

///\brief a ship bought - the one flown so far stays here or is sold; ShipyardNew names the new one
struct shipyard_buy_t
  {
  std::string ShipType;
  std::string ShipType_Localised;
  std::optional<uint64_t> StoreShipID;
  std::optional<uint64_t> SellShipID;
  uint64_t MarketID{};
  };

///\brief the ship just bought, with the id the game gave it
struct shipyard_new_t
  {
  std::string ShipType;
  std::string ShipType_Localised;
  uint64_t NewShipID{};
  };

///\brief a stored ship sold
struct shipyard_sell_t
  {
  std::string ShipType;
  uint64_t SellShipID{};
  uint64_t MarketID{};
  };

///\brief a ship sold instead of paying the rebuy
struct sell_ship_on_rebuy_t
  {
  std::string ShipType;
  uint64_t SellShipId{};
  };

///\brief a ship renamed
struct set_user_ship_name_t
  {
  std::string Ship;
  uint64_t ShipID{};
  std::string UserShipName;
  std::string UserShipId;
  };

///\brief an order to move a ship between ports
///
/// The one moment the game says when the ship will arrive - afterwards it never mentions it again,
/// and the arrival itself has no event of its own
struct shipyard_transfer_t
  {
  std::string ShipType;
  std::string ShipType_Localised;
  uint64_t ShipID;
  ///\brief the system the ship flies from
  std::string System;
  ///\brief the market it flies from - for a carrier that is its MarketID
  uint64_t ShipMarketID;
  double Distance;
  uint64_t TransferPrice;
  ///\brief delivery time in seconds
  uint64_t TransferTime;
  ///\brief the destination market, that is the one we are standing in
  uint64_t MarketID;
  };

  }  // namespace events

///\brief the ship flown right now, as the tool keeps it between Loadout events
struct ship_loadout_t
  {
  std::string Ship;
  uint32_t ShipID;
  std::string ShipName;
  std::string ShipIdent;
  float HullHealth;
  uint16_t CargoCapacity;
  uint16_t CargoUsed;
  events::fuel_capacity_t FuelCapacity;
  float FuelLevel;
  std::vector<events::module_t> Modules;
  };
