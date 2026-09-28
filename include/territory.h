#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief the systems one's own factions are in, side by side - the BGS of a cluster at one look
///
/// The overlay and the System info window speak of one system, the one we stand in. The work of a
/// cluster is decided between systems, though: which one to fly to after the tick, where a faction is
/// nearest to taking control, where a rival retreats, where one's own lead has grown thin. Everything
/// here is what the journals said at the last visit of each system. Nothing is forecast, and a system
/// not seen since the tick says so rather than showing yesterday as today
namespace territory
  {
///\brief one faction of a system as last read there
struct faction_t
  {
  std::string name;
  ///\brief in percent, as the game's panel shows it
  double influence{};
  ///\brief the move at the last tick, in points - empty when the system was not read since that wave began
  std::optional<double> moved;
  ///\brief the states of this system only, from ActiveStates and PendingStates
  std::string active;
  std::string pending;
  };

///\brief a conflict running or announced in a system
struct war_t
  {
  std::string war_type;
  std::string faction1;
  std::string faction2;
  uint32_t won_days1{};
  uint32_t won_days2{};
  ///\brief zero is decided at the next war tick
  uint32_t ticks_left{};
  ///\brief an announced one has not started yet
  bool active{};
  };

///\brief whether the result of the last tick is known in a system
enum struct tick_seen_e : uint8_t
  {
  ///\brief the influence changed since the wave began - the result is in
  known,
  ///\brief read since the wave began and nothing moved - the tick may not have come, or moved nothing
  unchanged,
  ///\brief not read since the wave began - the result is still to be seen
  not_seen
  };

///\brief one system of the territory
struct system_t
  {
  uint64_t system_address{};
  std::string name;
  uint64_t population{};
  std::string controlling;
  ///\brief empty when the position is not known
  std::optional<std::array<double, 3>> position;
  ///\brief the factions present at the newest reading, the strongest first
  std::vector<faction_t> factions;
  std::vector<war_t> wars;
  ///\brief the newest reading of the factions, whether anything changed or not
  std::optional<std::chrono::sys_seconds> seen;
  ///\brief the last change of influence seen here - the last tick seen here
  std::optional<std::chrono::sys_seconds> changed;
  ///\brief one's own missions' pluses since the tick, for all the factions together
  int32_t pushed_up{};
  int32_t pushed_down{};
  };

///\param wave the start of the newest influence wave, empty when none is known
[[nodiscard]]
auto tick_seen(system_t const & system, std::optional<std::chrono::sys_seconds> wave) noexcept -> tick_seen_e;

///\brief how far the controlling faction stands above the strongest of the rest
///\detail below zero when another faction has overtaken it - control changes only through a conflict,
/// so the controlling faction is the one the game names, not the strongest
struct lead_t
  {
  std::string rival;
  double margin{};
  };

[[nodiscard]]
auto lead(system_t const & system) -> std::optional<lead_t>;

///\brief where one faction stands across the territory
struct standing_t
  {
  std::string faction;
  ///\brief the systems it is in, and those it controls
  size_t present{};
  size_t controls{};
  ///\brief among those it does not control, the one where it is least behind the controlling faction
  std::string closest_system;
  std::string closest_controller;
  std::optional<double> closest_gap;
  ///\brief among those it controls, the one where its lead is the thinnest
  std::string thinnest_system;
  std::string thinnest_rival;
  std::optional<double> thinnest_lead;
  };

[[nodiscard]]
auto standings(std::span<system_t const> systems, std::span<std::string const> own) -> std::vector<standing_t>;

///\brief what in a system asks for attention: its wars and elections, and the factions retreating from it
///\detail Expansion is left out - the game writes it into every system of the faction, so it would stand
/// on every row and say nothing about any of them
///\param own one's own factions - one of them below retreat_below points is warned of before the game says Retreat
[[nodiscard]]
auto notes(system_t const & system, std::span<std::string const> own, double retreat_below) -> std::vector<std::string>;

///\brief the sector most of the territory's procedural names share - "Bleia Eohn" for "Bleia Eohn BD-I a64-1";
/// empty when fewer than two systems share one
[[nodiscard]]
auto common_sector(std::span<system_t const> systems) -> std::string;

///\brief the name without that sector - the rest is what tells the systems of a cluster apart
[[nodiscard]]
auto short_name(std::string_view name, std::string_view sector) -> std::string;

///\brief the distance in light years, empty when either position is unknown
[[nodiscard]]
auto distance(
  std::optional<std::array<double, 3>> const & from, std::optional<std::array<double, 3>> const & to
) noexcept -> std::optional<double>;
  }  // namespace territory
