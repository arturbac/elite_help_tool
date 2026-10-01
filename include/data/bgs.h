#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <simple_enum/simple_enum.hpp>

namespace events
  {
struct conflict_t;
struct faction_effect_t;
struct faction_info_t;
struct faction_state_entry_t;
  }  // namespace events

///\brief database rows: the BGS as the tool keeps it - factions, their influence and conflicts, the missions' work on them and the ticks
namespace info
  {
enum struct government_e : uint8_t
  {
  unknown,
  anarchy,
  communism,
  confederacy,
  cooperative,
  corporate,
  democracy,
  dictatorship,
  feudal,
  patronage,
  prison_colony,
  theocracy,
  engineer,
  private_ownership
  };

consteval auto adl_enum_bounds(government_e)
  {
  using enum government_e;
  return simple_enum::adl_info{unknown, private_ownership};
  }
enum struct allegiance_e : uint8_t
  {
  unknown,
  independent,
  alliance,
  empire,
  federation,
  thargoid,
  guardian
  };

consteval auto adl_enum_bounds(allegiance_e)
  {
  using enum allegiance_e;
  return simple_enum::adl_info{unknown, guardian};
  }
enum struct happiness_e
  {
  unknown,
  elated,
  happy,
  discontented,
  unhappy,
  despondent
  };

consteval auto adl_enum_bounds(happiness_e)
  {
  using enum happiness_e;
  return simple_enum::adl_info{unknown, despondent};
  }

///\brief reputation is personal while the faction is shared - hence the key is the name, not the faction oid
struct faction_reputation_t
  {
  std::string faction;
  double reputation;
  };

struct faction_info_t
  {
  std::string name;
  int64_t oid{-1};
  double reputation;
  government_e government;
  allegiance_e allegiance;
  happiness_e happiness;

  // assuming same name and skips oid verification
  [[nodiscard]]
  auto operator==(faction_info_t const &) const noexcept -> bool;
  };

[[nodiscard]]
auto to_native(events::faction_info_t && faction) -> faction_info_t;

/// influence is a per-system value, recorded over time by the event's date
struct faction_influence_t
  {
  int64_t oid{-1};
  int64_t faction_oid{-1};
  uint64_t system_address;
  std::chrono::sys_seconds timestamp;
  double influence;
  std::string faction_state;
  std::string pending_states;
  std::string active_states;
  std::string recovering_states;
  };

///\brief a conflict in a system, recorded over time by the event's date
struct conflict_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  std::chrono::sys_seconds timestamp;
  std::string war_type;
  std::string status;
  std::string faction1;
  std::string stake1;
  uint32_t won_days1;
  std::string faction2;
  std::string stake2;
  uint32_t won_days2;

  ///\brief without oid and time, to detect whether the conflict state has changed
  [[nodiscard]]
  auto operator==(conflict_t const &) const noexcept -> bool;
  };

[[nodiscard]]
auto to_conflict(uint64_t system_address, std::chrono::sys_seconds timestamp, events::conflict_t const & conflict)
  -> conflict_t;

///\brief the population cut down to its order of magnitude - 74k, 9.9M, 2.0B
///
/// When comparing systems the order of magnitude is what counts, not single people: what decides is whether
/// the system is a forty-thousand or a forty-million one, because that governs how much work
/// a percentage point of influence costs. The full number takes room and adds nothing
[[nodiscard]]
auto format_population(uint64_t value) -> std::string;

///\brief the state names joined by commas, for storing and for showing in a table
[[nodiscard]]
auto join_states(std::span<events::faction_state_entry_t const> states) -> std::string;

[[nodiscard]]
auto to_influence(
  int64_t faction_oid,
  uint64_t system_address,
  std::chrono::sys_seconds timestamp,
  events::faction_info_t const & faction
) -> faction_influence_t;

///\brief a trace of "the faction was present at this reading of the system"
///
/// influence is stored only when it changed, so the last entry's date speaks of the last change,
/// not of the last sighting. Without a separate trace a faction that left the system stays on the list
struct faction_presence_t
  {
  int64_t oid{-1};
  int64_t faction_oid;
  uint64_t system_address;
  std::chrono::sys_seconds last_seen;
  };

///\brief a light projection for the list of those present
struct faction_ref_t
  { int64_t faction_oid; };

///\brief how much influence one handed-in mission added for one faction in one system
///
/// the game counts this in pluses, not percent - "+++" means it got three times what "+" gets,
/// but how many percentage points that is depends on the system and on what others did that day.
/// So the raw count of pluses is what is kept, and the rate in percent comes only from setting it
/// against [[faction_influence_t]] after the tick
struct mission_influence_t
  {
  int64_t oid{-1};
  uint64_t mission_id;
  ///\brief when the mission was handed in - that moment decides which BGS day it falls into
  std::chrono::sys_seconds timestamp;
  std::string faction;
  uint64_t system_address;
  ///\brief signed: positive when the faction rises, negative when this mission pushes it down
  int32_t pluses;
  ///\brief how many times the mission moved the faction's economy bar here, signed like pluses -
  /// the game gives a direction only, so each effect counts as one
  int32_t economy{};
  ///\brief the same for the security bar
  int32_t security{};
  };

///\brief how a mission moved a faction's economy and security in one of the systems it moved its influence in
struct state_shift_t
  {
  int32_t economy{};
  int32_t security{};
  };

///\brief the state effects of one faction_effect_t that fall on its influence_ix-th system
///\detail the effects name no system. When there are as many of them as systems they go in order - so it
/// is in every journal seen - otherwise all of them go to the first system
[[nodiscard]]
auto state_shift(events::faction_effect_t const & effect, size_t influence_ix) -> state_shift_t;

///\brief the missions' pushes on one faction's influence, economy and security in one system since the tick
///\detail up and down apart: two up and two down is work on both sides, not no work
struct state_effort_t
  {
  std::string faction;
  int32_t influence_up;
  int32_t influence_down;
  int32_t economy_up;
  int32_t economy_down;
  int32_t security_up;
  int32_t security_down;
  };

///\brief the work put into one faction in one system over one BGS day, set against
/// against what that day actually produced
struct bgs_effort_t
  {
  uint64_t system_address;
  std::string system_name;
  ///\brief the system's population - without it a count of pluses means nothing
  ///
  /// the game divides a mission's influence by the size of the system, so the same 10 pluses give in a
  /// forty-million system a fraction of what they give in a forty-thousand one. The rate only makes sense
  /// within a single system and must never be averaged across systems
  uint64_t population;
  std::string faction;
  ///\brief the wave that closed this day - pluses handed in before it count towards it.
  /// The boundary is detected, not derived from an hour, because the tick moves every few days
  std::chrono::sys_seconds closed_by;
  ///\brief pluses pushing a faction up and those pushing it down, kept apart - two different levers
  int32_t pushed_up;
  int32_t pushed_down;
  int32_t missions;
  ///\brief influence from the last sample before the closing wave and the first after it, in percent.
  /// Empty when we were not in the system on one side of it - that day cannot then be settled
  /// and must not be guessed at
  std::optional<double> influence_before;
  std::optional<double> influence_after;

  ///\brief the faction's state from the reading before the closing wave, the one in force that day
  ///
  /// Without it the rate can be unreadable, because the state changes both sides of the equation. Most of all
  /// **Retreat**: missions for a faction in retreat are far more effective, and at the same time it loses
  /// about two percentage points a day - the measured gain is therefore what survived
  /// that drain, and the work was more effective still
  std::string faction_state;

  ///\brief all the upward work put into this system that day, across every faction together
  ///
  /// Influence is a percentage share, so factions pushed on the same day divide one gain between
  /// them rather than getting two independent ones. Working the rate out for each separately
  /// inflates it the more factions were worked at once - which is why the cost of a point is a
  /// property of the system, not of the faction.
  ///
  /// **A split of work is not a split of the gain.** The same five points lift a faction lying
  /// at the bottom far more than one already holding ninety percent - because percentages are measured
  /// against the total, and at the top that total is already mostly its own. The share of work therefore says how much
  /// effort went where, not how many percentage points will come of it; that also depends on
  /// where the faction stands in the field - which is why every row shows its influence before the wave
  int32_t system_pushed_up;
  ///\brief the combined influence gain of the factions pushed up that day, in percentage points.
  /// Also empty when any one of them lacks a reading - the split must cover the whole
  /// or there is no split at all.
  ///
  /// The cost of a point worked out from this is an average over whoever was pushed that day: a day spent
  /// on a faction at the bottom comes out cheaper than the same effort put into the system's leader
  std::optional<double> system_gain;
  };

///\brief how late after the announcement the war actually started
///
/// The faction support panel shows the transition into the war state at once, but nothing reaches the journal -
/// the only trace is the conflict status at the next reading of the system. Both marks are therefore
/// bounds, not moments: the war started somewhere between them, and settlements enter the war state
/// later still. The spread across many wars says when to plan the trip for
struct war_onset_t
  {
  uint64_t system_address;
  std::string system_name;
  std::string war_type;
  std::string faction1;
  std::string faction2;
  ///\brief the last reading in which the war was still only announced
  std::chrono::sys_seconds pending_last;
  ///\brief the first in which it was already running
  std::chrono::sys_seconds active_first;
  ///\brief the result from the last reading of this war - days won by each side
  uint32_t won_days1;
  uint32_t won_days2;
  ///\brief the status from the last reading; empty means the war is already over
  std::string status;
  };

///\brief how much is left of a running conflict
///
/// A conflict is decided once one side gathers four won days - in this database 53 of 90 closed
/// conflicts end that way, the rest being last readings from before we left the system.
/// That lets the end of a war be counted down from won_days alone, without knowing the hour of the recalculation;
/// the hour only says when in that day
struct war_countdown_t
  {
  uint64_t system_address;
  std::string war_type;
  std::string faction1;
  std::string faction2;
  uint32_t won_days1;
  uint32_t won_days2;
  ///\brief zero means the conflict is decided at the next war recalculation - worth having
  /// bonds in hand by then, because after a win they pay a premium
  uint32_t ticks_left;
  ///\brief whether the conflict is already running - an announced one is yet to start and has four full days ahead
  bool active;
  };

///\brief which of the game's daily recalculations - these are two separate clocks
///\detail they usually run together, but not always: on 4 August 2026 influence recalculated at 16:30
/// and wars at 14:30; on the seventh influence at 16:30 and wars at 11:30
enum struct tick_kind_e : uint8_t
  {
  ///\brief the recalculation of faction influence
  influence,
  ///\brief the recalculation of days won in conflicts
  war
  };

consteval auto adl_enum_bounds(tick_kind_e)
  {
  using enum tick_kind_e;
  return simple_enum::adl_info{influence, war};
  }

///\brief the trace of one tick: the span between the last reading with the old value and the first
/// with the new one
///
/// The game announces no tick. All that can be seen is that between two looks at a system the value
/// changed - which means only that the tick fell somewhere in that interval. The more systems
/// visited close together in time, the tighter the intervals intersect
struct tick_observation_t
  {
  int64_t oid{-1};
  tick_kind_e kind;
  uint64_t system_address;
  ///\brief the last reading that still showed the old value
  std::chrono::sys_seconds window_begin;
  ///\brief the first reading with the new value
  std::chrono::sys_seconds window_end;
  };

///\brief one recalculation wave, made of observations across successive systems
///
/// A tick is not a moment. The galaxy recalculates system by system, and neighbours can drift apart
/// by hours, so intersecting windows from different systems would give the empty set - and it does not, because
/// each system has a moment of its own. So instead of one hour we keep both ends of the wave:
///
/// - the **start** is what matters for influence: missions must be handed in before it, because after it
///   the pluses go towards the next day,
/// - the **end** is what matters for wars: only after it do bonds sell the new way.
///
/// Waves do not come every 24h - at weekends there can be no tick for nearly two days, and after
/// a game update the servers lose it entirely. No field here is a forecast
struct tick_fact_t
  {
  tick_kind_e kind;
  ///\brief the window in which the first system recalculated - the wave began here
  std::chrono::sys_seconds start_begin;
  std::chrono::sys_seconds start_end;
  ///\brief the window in which the last one recalculated - the wave ended here
  std::chrono::sys_seconds end_begin;
  std::chrono::sys_seconds end_end;
  ///\brief how many observations, and how many distinct systems, went into this wave
  uint32_t samples;
  uint32_t systems;
  };

///\brief how regularly the recalculation comes at all
///
/// It answers "has the tick come today" other than by adding a day: it shows the typical
/// and the longest gap observed, so it is plain at once that at a weekend it may not come
struct tick_stats_t
  {
  tick_kind_e kind;
  uint32_t waves;
  ///\brief the median and the longest gap between the starts of successive waves
  std::chrono::minutes typical_gap;
  std::chrono::minutes longest_gap;
  ///\brief how many waves were seen in more than one system - only those say anything about the spread
  uint32_t multi_system_waves;
  ///\brief the widest propagation seen, from the start of a wave to its end
  std::chrono::minutes widest_spread;
  ///\brief the median width of the window, that is how exactly this was measured at all
  ///
  /// The window is the gap between a reading with the old value and one with the new, so rarer visits
  /// widen it. That does not move the tick - it simply means less is known about it,
  /// and this number is what says so
  std::chrono::minutes typical_window;
  };
  }  // namespace info
