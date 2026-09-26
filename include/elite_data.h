#pragma once
#include <elite_events.h>

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

///\brief how far the journal has already been read into this database
///\detail one row, so that starting the tool again does not walk the whole session back through
/// the database writing what is already there. Compared against journal stamps and nothing else:
/// the game's clock and this machine's need not agree, so the only safe comparison is like for like
struct journal_progress_t
  {
  ///\brief always 1; the key exists so the write has something to conflict on
  uint32_t id;
  std::chrono::sys_seconds last_event;
  };

///\brief the account this personal database belongs to
///\detail written during the import; the GUI reads it and refuses to add another commander's career, should
/// somebody log into a second account from the same game profile
struct db_owner_t
  {
  std::string fid;
  std::string name;
  };

///\brief what THIS commander did in the galaxy - this must not be shared between accounts
///\detail the world is shared, but a scan and a mapping belong to one commander. telling
/// one commander that something is mapped when another did it leads straight to a bad decision when
/// planning a flight - so these tables stay in the personal database and key themselves naturally,
/// on names and numbers from the game rather than oids, which change with every galaxy rebuild
struct system_progress_t
  {
  uint64_t system_address;
  bool fss_complete;
  };

struct body_progress_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  uint32_t body_id;
  bool mapped;
  bool footfalled;
  };

struct genus_progress_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  uint32_t body_id;
  std::string genus;
  bool sampled;
  };

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

///\brief a station's identity, one row per MarketID
///\detail rebuildable from journals - the Docked and Market events - so it lives in the main database
struct station_t
  {
  uint64_t market_id;
  uint64_t system_address;
  std::string name;
  std::string station_type;
  ///\brief the economy and government of the place - they say what to look for there, the settlement's name does not
  std::string economy;
  std::string government;
  ///\brief the faction holding the place - its influence is what missions handed in here raise
  std::string controlling_faction;
  };

///\brief when we last read this station's market
///\detail the contents themselves come from Market.json, which cannot be rebuilt, so the time of
/// the reading belongs to the database gathered live
struct market_info_t
  {
  uint64_t market_id;
  std::chrono::sys_seconds updated;
  };

///\brief the commodity dictionary; mean_price is the galactic average, a constant of the commodity
struct commodity_t
  {
  uint64_t id;
  std::string name;
  std::string category;
  uint32_t mean_price;
  };

///\brief the newest market reading, one row per commodity
struct market_item_t
  {
  int64_t oid{-1};
  uint64_t market_id;
  uint64_t commodity_id;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t stock;
  uint32_t demand;
  ///\brief whether the station really produces and buys this commodity - zero stock may be a passing gap
  bool producer;
  bool consumer;
  };

///\brief a market row already joined with the commodity dictionary, ready for the window
struct market_entry_t
  {
  std::string name;
  std::string category;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t mean_price;
  uint32_t stock;
  uint32_t demand;
  ///\brief whether the station really produces and buys this commodity
  bool producer;
  bool consumer;
  };

///\brief a light projection of star_system for pick lists; field names must match the columns
struct system_ref_t
  {
  uint64_t system_address;
  std::string name;
  };

[[nodiscard]]
auto to_influence(
  int64_t faction_oid,
  uint64_t system_address,
  std::chrono::sys_seconds timestamp,
  events::faction_info_t const & faction
) -> faction_influence_t;

enum struct mission_status_e : uint8_t
  {
  accepted,
  redirected,  // done but not delivered and completed
  completed,
  failed,
  abandoned,
  ///\brief the game stopped listing it as open and we never saw it close
  expired
  };

consteval auto adl_enum_bounds(mission_status_e)
  {
  using enum mission_status_e;
  return simple_enum::adl_info{accepted, expired};
  }

struct mission_t
  {
  uint64_t mission_id;
  mission_status_e status;
  std::chrono::sys_seconds expiry;
  std::string faction;
  std::string type;
  std::string description;
  uint64_t reward;
  ///\brief the station the mission was taken at, zero when unknown
  uint64_t market_id;
  ///\brief when the mission closed - without it weekly statistics cannot be counted
  std::chrono::sys_seconds closed;

  std::string target;
  std::string target_type;
  std::string target_faction;

  std::string destination_system;   //": "Anana",
  std::string destination_station;  //": "Yamazaki Base",
  std::string destination_settlement;

  std::string redirected_system;   //": "Anana",
  std::string redirected_station;  //": "Yamazaki Base",
  std::string redirected_settlement;

  uint32_t count;
  uint16_t kill_count;
  uint16_t passenger_count;

  [[nodiscard]]
  auto mission_count() const noexcept
    {
    return std::max<uint32_t>(std::max<uint32_t>(count, kill_count), passenger_count);
    }
  };

using space_location_t = std::array<double, 3>;

struct route_item_t
  {
  std::string system;
  uint64_t system_address;
  /// star position in light years
  space_location_t star_location;
  std::string star_class;
  double distance;
  bool visited;
  };


struct fcmaterial_t
{
  int64_t oid;
  int64_t carrier_id;
  int64_t timestamp;
  uint64_t material_id;
  uint32_t price;
  uint32_t stock;
  uint32_t demand;
};

struct carrier_t
{
  int64_t oid;
  uint64_t market_id;
  std::string carrier_name;
  std::string carrier_id;
  ///\brief a carrier that concerns me - a stranger's bartender is visible too, but that is only background
  bool tracked;

  ///\brief the state from the last CarrierStats - empty until we have seen one
  ///
  /// The event arrives on docking and on managing the carrier, so these numbers always come
  /// from the last such moment rather than from now - hence the timestamp beside them
  std::string carrier_type;
  std::string docking_access;
  uint32_t fuel_level;
  double jump_range_curr;
  double jump_range_max;
  uint32_t total_capacity;
  uint32_t free_space;
  uint32_t cargo;
  uint64_t balance;
  uint64_t available_balance;
  std::chrono::sys_seconds stats_seen;
};

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
struct micro_sale_t
{
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t market_id;
  uint64_t price;
  uint32_t total_count;
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

///\brief a commodity a mission requires - a separate table, so the mission schema stays untouched
struct mission_cargo_t
  {
  uint64_t mission_id;
  ///\brief the readable name, the same as in the commodity dictionary
  std::string commodity;
  uint32_t count;
  };

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

///\brief a waypoint of a stored neutron route
///
/// A route plotted outside the game (spansh) and loaded from a file - the game stores it nowhere, and
/// NavRoute is overwritten with every course set, so neutron jumps plotted one at a time
/// would wipe it constantly. A route flown regularly should be remembered, and since it cannot
/// be rebuilt from journals, it lives in the database gathered live
struct neutron_waypoint_t
  {
  int64_t oid{-1};
  ///\brief the route's name, the same on all its waypoints - one is remembered, the one flown
  /// regularly; one-off routes live only until the window closes and reach nowhere
  std::string route_name;
  ///\brief flight order - after any reversal, so zero is the first hop
  uint32_t position;
  std::string system;
  uint64_t system_address;
  double loc_x;
  double loc_y;
  double loc_z;
  ///\brief whether this is a neutron star, that is a stop for a supercharge
  bool neutron;
  ///\brief the distance from the previous waypoint in light years
  double distance;
  };

///\brief a ship in transit between ports
///
/// The game gives the delivery time once, at the order, and never mentions it again - arrival has
/// no event of its own. Unless that one moment is stored, the information is lost
struct ship_transfer_t
  {
  int64_t oid{-1};
  uint64_t ship_id;
  std::string ship_type;
  std::string from_system;
  uint64_t to_market_id;
  double distance;
  uint64_t price;
  std::chrono::sys_seconds ordered;
  ///\brief worked out at the order: the moment of ordering plus the delivery time
  std::chrono::sys_seconds arrives;
  };

///\brief a port we stood at with a ship
///
/// Only a place one docks at with a ship counts - on-foot settlements and carriers are not
/// ports in the sense of the escape pod, which returns you to the last port, not to the last
/// place. The station type is stored, because it is what decides
struct port_visit_t
  {
  uint64_t market_id;
  std::string name;
  std::string system;
  std::string station_type;
  std::chrono::sys_seconds visited;
  };

///\brief how much of what has to be brought in total, once open missions are summed
struct cargo_need_t
  {
  std::string commodity;
  uint32_t count;
  };

///\brief a place where what a mission requires can be bought
struct supply_option_t
  {
  uint64_t market_id;
  std::string station;
  std::string station_type;
  std::string system;
  std::string commodity;
  uint32_t needed;
  uint32_t stock;
  uint32_t buy_price;
  };

///\brief whether the commodity cannot be bought, only mined
///
/// a station can pay very well for such a resource, but for a trader it is a dead end -
/// nobody will sell it to him. The list is fixed knowledge about the game, so it sits in code, not in the database
[[nodiscard]]
auto is_mining_only(std::string_view commodity) noexcept -> bool;

///\brief a trade rate: buy there, sell here
struct trade_option_t
  {
  uint64_t market_id;
  std::string station;
  std::string system;
  std::string commodity;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t stock;
  uint32_t demand;
  };

///\brief how many missions, and of what kind, were done for a faction in a given period
struct mission_stat_t
  {
  std::string faction;
  uint32_t missions;
  uint64_t rewards;
  ///\brief the most frequent kind, Mission_Massacre for instance
  std::string top_type;
  };

///\brief a bartender shelf row after joining with the dictionary
struct carrier_stock_t
{
  std::string name;
  std::string localised;
  std::string category;
  uint32_t price;
  uint32_t stock;
  uint32_t demand;
  std::chrono::sys_seconds timestamp;
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
};

constexpr double light_speed_mps = 299'792'458.0;

///\returns distance in Ly
[[nodiscard]]
auto distance(space_location_t const & loc1, space_location_t const & loc2) -> double;

[[nodiscard]]
auto transform_mission_name(std::string_view input) -> std::string;
  }  // namespace info
