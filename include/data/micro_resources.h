#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <simple_enum/simple_enum.hpp>

///\brief database rows: micro resources on foot - the dictionary, the counters, consumables and kills
namespace info
  {
///\brief the micro resource dictionary, glued together from two sources that know different things
///\detail FCMaterials.json gives a numeric id and a readable name, SellMicroResources the category,
/// and the shared key is the internal name - "$weaponschematic_name;" and "weaponschematic" are the
/// same material
struct micro_resource_t
{
  std::string name;
  uint64_t id;
  std::string localised;
  std::string category;
};

///\brief a micro resource sale - dropped at a station's bartender or delivered to a player's carrier
///\detail a journal event, so rebuildable after the fact; the station type says which case it is
///\brief which way micro resources went at a counter
enum struct micro_trade_e : uint8_t
{
  ///\brief to a bartender for credits
  sold,
  ///\brief for credits - consumables at Pioneer Supplies, goods at a bar
  bought,
  ///\brief a barter at a bartender - kinds given away for one received
  bartered
};

consteval auto adl_enum_bounds(micro_trade_e)
{
  using enum micro_trade_e;
  return simple_enum::adl_info{sold, bartered};
}

struct micro_sale_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t market_id;
  ///\brief the credits of the whole transaction - the game gives no price per kind; zero for a barter
  uint64_t price;
  uint32_t total_count;
  micro_trade_e kind{micro_trade_e::sold};
};

///\brief one consumable used up on foot - a grenade thrown, a medkit or an energy cell spent
///\detail seq tells apart two in the same second, which a frag fight makes common - a replay of the
/// journal counts them in the same order, so the pair of time and seq is the row's identity
struct consumable_use_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint32_t seq;
  std::string name;
  uint32_t count;
};

///\brief where a kill on foot was made - a conflict zone pays bonds, a settlement raid is murders and bounties
enum struct foot_kill_e : uint8_t
{
  ///\brief a bond in a conflict zone
  conflict_zone,
  ///\brief a clean victim at a settlement - a murder
  murder,
  ///\brief a wanted one - a bounty
  bounty
};

consteval auto adl_enum_bounds(foot_kill_e)
{
  using enum foot_kill_e;
  return simple_enum::adl_info{conflict_zone, bounty};
}

///\brief a kill on foot
struct foot_kill_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint32_t seq;
  foot_kill_e kind;
  ///\brief a frag grenade went a moment before - the game does not say what killed, so it is a guess
  bool grenade;
  ///\brief what was in hand, from Status.json - known only for kills seen live, empty otherwise
  std::string weapon;
};

///\brief what went through the counters, kind by kind, over a period
struct bartender_summary_t
{
  std::string name;
  std::string localised;
  std::string category;
  uint32_t sold;
  uint32_t bought;
  uint32_t bartered_away;
  uint32_t bartered_for;
};

///\brief the transactions of one kind over a period, and their credits
struct bartender_total_t
{
  micro_trade_e kind;
  uint32_t transactions;
  uint64_t credits;
};

///\brief how many of one consumable were used over a period
struct consumable_summary_t
{
  std::string name;
  std::string localised;
  uint32_t used;
};

///\brief kills on foot of one place and one weapon over a period
struct foot_kill_summary_t
{
  foot_kill_e kind;
  bool grenade;
  std::string weapon;
  uint32_t kills;
};

///\brief a collected micro resource together with the place it came from
///\detail market_id points at the settlement, and through it at its economy; zero when found outside one
///\brief where the micro resource came from
enum struct acquisition_source_e : uint8_t
{
  ///\brief picked up at a settlement, a data port or a container
  collected,
  ///\brief a mission reward; it goes straight to the locker, passing the backpack by
  mission_reward
};

consteval auto adl_enum_bounds(acquisition_source_e)
{
  using enum acquisition_source_e;
  return simple_enum::adl_info{collected, mission_reward};
}

struct micro_acquisition_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t market_id;
  std::string name;
  uint32_t count;
  acquisition_source_e source;
};

///\brief the finds of one material, split by method and place
struct acquisition_summary_t
{
  std::string name;
  std::string localised;
  std::string category;
  uint32_t collected;
  uint32_t from_missions;
  std::string top_economy;
  std::chrono::sys_seconds last_seen;
};

///\brief a transaction row, joined to the dictionary by the internal name
struct micro_sale_item_t
{
  int64_t oid{-1};
  int64_t sale_oid;
  std::string name;
  uint32_t count;
  ///\brief came into our hands - bought, or the one kind a barter gave; given away otherwise
  bool received{};
};
  }  // namespace info
