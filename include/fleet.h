#pragma once
#include <databse_storage.h>
#include <elite_data.h>
#include <elite_events.h>
#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief the commander's ships and where each of them is
///
/// The game gives the whole fleet only on opening a shipyard (StoredShips) and leaves out the ship being
/// flown. Between two such lists the fleet is kept up by the events that move single ships: a swap leaves
/// one ship here and takes another, a purchase or a sale adds or removes one, a transfer sends one on its way
namespace fleet
  {
///\brief where the commander is when an event names no place of its own
struct here_t
  {
  std::string system;
  std::string station;
  uint64_t market_id{};
  };

///\brief the list from the shipyard replaces what was known, keeping the ship flown and the destination
/// of ships whose transfer was seen ordered
auto apply(std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::stored_ships_t const & event)
  -> void;
///\brief the ship flown, with its name and value
auto apply(
  std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::loadout_t const & event, here_t const & here
) -> void;
auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_swap_t const & event,
  here_t const & here
) -> void;
auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_buy_t const & event,
  here_t const & here
) -> void;
auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_new_t const & event,
  here_t const & here
) -> void;
auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_transfer_t const & event,
  here_t const & here
) -> void;
///\brief an escape pod leaves the ship flown where the commander was docked - a carrier, as a rule
auto apply(
  std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::resurrect_t const & event, here_t const & here
) -> void;
auto apply(std::vector<info::ship_t> & ships, events::shipyard_sell_t const & event) -> void;
auto apply(std::vector<info::ship_t> & ships, events::sell_ship_on_rebuy_t const & event) -> void;
auto apply(std::vector<info::ship_t> & ships, events::set_user_ship_name_t const & event) -> void;

///\brief the events that change the fleet
template<typename event_t>
constexpr bool changes_fleet{
  std::same_as<event_t, events::stored_ships_t> or std::same_as<event_t, events::loadout_t>
  or std::same_as<event_t, events::shipyard_swap_t> or std::same_as<event_t, events::shipyard_buy_t>
  or std::same_as<event_t, events::shipyard_new_t> or std::same_as<event_t, events::shipyard_transfer_t>
  or std::same_as<event_t, events::shipyard_sell_t> or std::same_as<event_t, events::sell_ship_on_rebuy_t>
  or std::same_as<event_t, events::set_user_ship_name_t> or std::same_as<event_t, events::resurrect_t>
};

///\brief applies one event to the fleet in the database - the same for the import and for the live state
///\param system the system the commander is in
///\param market_id the port the commander stands at, 0 away from one - an event naming a market overrides it
template<typename event_t>
auto record(
  database_storage_t & db,
  std::chrono::sys_seconds when,
  event_t const & event,
  std::string_view system,
  uint64_t market_id
) -> void;

///\brief a ship with the place it is at now, and how far that is
struct placed_ship_t
  {
  info::ship_t ship;
  std::string system;
  std::string station;
  ///\brief unknown when the system's position was never seen
  std::optional<double> distance_ly;
  ///\brief stands on a carrier, so the place followed the carrier's jumps
  bool on_carrier{};
  ///\brief a transfer still under way
  bool travelling{};
  };

[[nodiscard]]
auto distance_ly(std::array<double, 3> const & a, std::array<double, 3> const & b) noexcept -> double;

///\brief the place of every ship now: the ship flown is here, one on a carrier went where the carrier went
/// after it was left, a transfer past its time has arrived
///\return the ship flown first, then by distance, those at an unknown distance last
[[nodiscard]]
auto place(
  std::span<info::ship_t const> ships,
  std::span<info::carrier_state_t const> carriers,
  std::map<std::string, std::array<double, 3>> const & positions,
  std::string_view here_system,
  std::array<double, 3> const & here_position,
  std::chrono::sys_seconds now
) -> std::vector<placed_ship_t>;

///\brief the fleet from the database, placed
[[nodiscard]]
auto locate(
  database_storage_t & db,
  std::string_view here_system,
  std::array<double, 3> const & here_position,
  std::chrono::sys_seconds now
) -> expected_ec<std::vector<placed_ship_t>>;

///\brief the ship's own name, or its type when it has none
[[nodiscard]]
auto shown_name(info::ship_t const & ship) -> std::string;
  }  // namespace fleet
