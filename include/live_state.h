#pragma once

#include <events/combat.h>
#include <events/navigation.h>
#include <events/ships.h>
#include <events/station.h>

#include <algorithm>
#include <cctype>
#include <concepts>
#include <span>
#include <string_view>

///\brief what holds only while it lasts - the crosshairs, the port we are at, the crew on board - read from
/// events that say so only by what else they mean
namespace live
  {
///\brief the events at which the game lets a target go without writing a ShipTargeted to say so: in the
/// journals a lock stood through every one of them. Not a touchdown or a liftoff - near the ground a target
/// may be held on
template<typename event_t>
constexpr bool drops_target_lock
  = std::same_as<event_t, events::supercruise_entry_t> or std::same_as<event_t, events::start_jump_t>
    or std::same_as<event_t, events::fsd_jump_t> or std::same_as<event_t, events::docked_t>
    or std::same_as<event_t, events::disembark_t> or std::same_as<event_t, events::died_t>
    or std::same_as<event_t, events::resurrect_t> or std::same_as<event_t, events::shipyard_swap_t>
    or std::same_as<event_t, events::location_t>;

///\brief the events written where a port's market or shipyard is used, so their MarketID is the port we are
/// at - also when nothing said we docked: out of an escape pod the game writes a Location without the port
template<typename event_t>
constexpr bool names_the_port
  = std::same_as<event_t, events::market_t> or std::same_as<event_t, events::shipyard_swap_t>;

///\brief whether the ship has a fighter hangar - the hired pilot flies only from one, and a ship without it
/// has no crew aboard though the journal never says they stayed behind. No modules known (no Loadout yet)
/// counts as a hangar, so nothing is hidden on a guess
[[nodiscard]]
inline auto has_fighter_bay(std::span<events::module_t const> modules) -> bool
  {
  if(modules.empty())
    return true;
  return std::ranges::any_of(
    modules,
    [](events::module_t const & mod)
    {
      constexpr std::string_view bay{"fighterbay"};
      std::string_view const item{mod.Item};
      return std::ranges::search(
               item, bay, [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; }
             ).begin()
             != item.end();
    }
  );
  }
  }  // namespace live
