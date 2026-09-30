#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

///\brief journal events: micro resources - the backpack and the bartenders
namespace events
  {

///\brief a backpack row
struct backpack_item_t
  {
  std::string Name;
  std::string Name_Localised;
  ///\brief Data, Item, Component or Consumable
  std::string Type;
  uint32_t Count;
  };

///\brief a change to the backpack's contents - Added is a find from a settlement, a data port or a mission
struct backpack_change_t
  {
  std::vector<backpack_item_t> Added;
  ///\brief a consumable here was used up - a grenade thrown, a medkit or a cell spent. UseConsumable
  /// comes more often than the things go, so it is not the count
  std::vector<backpack_item_t> Removed;
  };

///\brief a micro resource sale row
struct sold_micro_resource_t
  {
  std::string Name;
  std::string Name_Localised;
  std::string Category;
  uint32_t Count;
  };

///\brief selling micro resources to a bartender - at a station or on a player's carrier
struct sell_micro_resources_t
  {
  uint64_t MarketID;
  uint64_t Price;
  uint32_t TotalCount;
  std::vector<sold_micro_resource_t> MicroResources;
  };

///\brief buying micro resources - consumables at Pioneer Supplies one kind at a time, or goods at a bar
///\detail an older form names the one kind in the event itself, a newer one lists them; Price is for all
struct buy_micro_resources_t
  {
  uint64_t MarketID;
  uint64_t Price;
  uint32_t TotalCount{};
  std::string Name;
  std::string Name_Localised;
  std::string Category;
  uint32_t Count{};
  std::vector<sold_micro_resource_t> MicroResources;
  };

///\brief a barter at a bartender - several kinds given away for one received
struct trade_micro_resources_t
  {
  uint64_t MarketID;
  std::vector<sold_micro_resource_t> Offered;
  uint32_t TotalCount{};
  std::string Received;
  std::string Received_Localised;
  std::string Category;
  uint32_t Count{};
  };

  }  // namespace events

///\brief a micro resource's internal name, the key shared by both sources
///\detail "$weaponschematic_name;" and "weaponschematic" are the same material
[[nodiscard]]
auto micro_resource_key(std::string_view name) -> std::string;
