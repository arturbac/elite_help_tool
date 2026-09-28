#pragma once
#include <elite_data.h>
#include <databse_storage.h>
#include <chrono>
#include <optional>
#include <deque>
#include <mutex>

///\brief the consumables used up and the kills made on foot, with the guess which kills a grenade made
///
/// The game says neither what killed nor which kill a grenade made. What the journal has is the moment a
/// frag grenade left the backpack and the moment of each kill: across the journals kills come most often two
/// to four seconds after a throw, well above the rate at any other delay - so a kill in that window counts
/// as a grenade's. Some of them were the gun's all the same, the window is a guess
class on_foot_tracker_t
  {
public:
  static constexpr std::chrono::seconds grenade_from{2};
  static constexpr std::chrono::seconds grenade_to{4};

  ///\brief a thing gone from the backpack; only a consumable is a use - the rest is a sale or a hand-over
  [[nodiscard]]
  auto removed(std::chrono::sys_seconds when, events::backpack_item_t const & item)
    -> std::optional<info::consumable_use_t>;

  [[nodiscard]]
  auto kill(std::chrono::sys_seconds when, info::foot_kill_e kind, std::string weapon) -> info::foot_kill_t;

private:
  std::chrono::sys_seconds last_frag_{};
  std::chrono::sys_seconds last_use_{};
  uint32_t use_seq_{};
  std::chrono::sys_seconds last_kill_{};
  uint32_t kill_seq_{};
  };

///\brief which weapon was in hand when - from Status.json looked at often, since the journal never says
///
/// In a hard fight the weapon changes three times in two seconds, and Status.json read once, after the kill
/// line has come, often already holds the next one. So every change seen is kept with the file's own time,
/// the game's clock like the journal's. That time is to the second only: a change in the kill's own second
/// could have come before the shot or after it, and then the weapon is not known
class weapon_log_t
  {
public:
  ///\brief what Status.json holds now, written at the given moment
  auto observe(std::chrono::sys_seconds written, std::string weapon) -> void;

  ///\brief the weapon in hand at the moment, empty when not known
  [[nodiscard]]
  auto at(std::chrono::sys_seconds when) const -> std::string;

private:
  struct change_t
    {
    std::chrono::sys_seconds since;
    std::string weapon;
    };
  mutable std::mutex mutex_;
  std::deque<change_t> changes_;
  };

///\brief the kind of a kill on foot a bounty stands for - an NPC on foot wears a suit, a ship does not
[[nodiscard]]
auto is_foot_target(std::string_view target) -> bool;

///\brief a purchase at a counter, with its kinds, into the database - the same in the import and live
auto store_counter_trade(database_storage_t & db, std::chrono::sys_seconds when, events::buy_micro_resources_t const & event)
  -> void;

///\brief a barter at a bartender, with the kinds given and the one received
auto store_counter_trade(database_storage_t & db, std::chrono::sys_seconds when, events::trade_micro_resources_t const & event)
  -> void;
