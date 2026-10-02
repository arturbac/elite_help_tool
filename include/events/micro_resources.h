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

///\brief a row of the ship's locker or of the backpack
struct locker_item_t
  {
  std::string Name;
  std::string Name_Localised;
  ///\brief the mission the thing was handed out for - a virus to upload, a regulator to fit - 0 when it is one's own
  uint64_t MissionID{};
  uint32_t Count{};
  };

///\brief all that is owned on foot and kept aboard
///\detail the event carries the lists, or just as often nothing and leaves them in ShipLocker.json - a thing a
/// mission hands out shows up here and nowhere else, never in a BackpackChange
struct ship_locker_t
  {
  std::vector<locker_item_t> Items;
  std::vector<locker_item_t> Components;
  std::vector<locker_item_t> Consumables;
  std::vector<locker_item_t> Data;
  };

///\brief what is carried on foot, the same lists - or none and Backpack.json beside the journals
struct backpack_t
  {
  std::vector<locker_item_t> Items;
  std::vector<locker_item_t> Components;
  std::vector<locker_item_t> Consumables;
  std::vector<locker_item_t> Data;
  };

///\brief whether the event came with its lists, or only points at the file beside the journals
template<typename inventory_t>
[[nodiscard]]
constexpr auto carries_lists(inventory_t const & inventory) noexcept -> bool
  {
  return not inventory.Items.empty() or not inventory.Components.empty() or not inventory.Consumables.empty()
         or not inventory.Data.empty();
  }

///\brief calls fn(item, category) for every row, the category named as BackpackChange and the bartenders name it
template<typename inventory_t, typename function_t>
constexpr auto for_each_item(inventory_t const & inventory, function_t && fn) -> void
  {
  for(locker_item_t const & item: inventory.Items)
    fn(item, std::string_view{"Item"});
  for(locker_item_t const & item: inventory.Components)
    fn(item, std::string_view{"Component"});
  for(locker_item_t const & item: inventory.Consumables)
    fn(item, std::string_view{"Consumable"});
  for(locker_item_t const & item: inventory.Data)
    fn(item, std::string_view{"Data"});
  }

  }  // namespace events

///\brief a micro resource's internal name, the key shared by both sources
///\detail "$weaponschematic_name;" and "weaponschematic" are the same material
[[nodiscard]]
auto micro_resource_key(std::string_view name) -> std::string;

///\brief the backpack after one change - a find added to its list by Type, a used or handed-in thing taken off
///\detail rows are matched by micro_resource_key, a row brought to nought is dropped
auto apply_backpack_change(events::backpack_t & backpack, events::backpack_change_t const & change) -> void;
