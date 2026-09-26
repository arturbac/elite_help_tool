#pragma once
#include <string>
#include <memory>
#include <simple_enum/expected.h>
#include <simple_enum/simple_enum.hpp>
#include <elite_events.h>
#include <elite_data.h>
#include <array>
#include <span>

struct sqlite3_handle_t;

template<typename T>
using expected_ec = cxx23::expected<T, std::error_code>;

///\brief the database's working mode; it decides the durability/speed trade-off
enum struct storage_mode_e : uint8_t
  {
  ///\brief working live - every write in a transaction of its own, sqlite's own defaults
  live,
  ///\brief building the database from scratch out of all the logs; on failure we repeat the import anyway
  bulk_import
  };

consteval auto adl_enum_bounds(storage_mode_e)
  {
  using enum storage_mode_e;
  return simple_enum::adl_info{live, bulk_import};
  }

struct database_storage_t
  {
  std::string db_path_;
  ///\brief data gathered live only - station markets and the carrier's bartender
  ///\detail their source is Market.json and FCMaterials.json, overwritten by the game, so they cannot
  /// be rebuilt from journals; a rebuild of the main database leaves them alone, and journal_tailer
  /// creates this file only when it is missing
  std::string live_db_path_;
  ///\brief facts about the galaxy, shared by all commanders - systems, bodies, stations, factions
  ///\detail rebuildable from any commander's journals, because they describe the world and not the player. that lets
  /// two accounts point at the same file and share what is known about the Bubble, while keeping their own missions,
  /// reputation and scanning progress in the main database
  std::string galaxy_db_path_;
  std::unique_ptr<sqlite3_handle_t> db_;

  explicit database_storage_t(std::string_view db_path);
  ~database_storage_t();

  [[nodiscard]]
  auto open(storage_mode_e mode = storage_mode_e::live) -> expected_ec<void>;

  [[nodiscard]]
  auto create_database() -> expected_ec<void>;

  ///\brief adds missing columns to live.sqlite, which cannot be rebuilt from journals
  [[nodiscard]]
  auto migrate_live_schema() -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::mission_t const & value) -> expected_ec<void>;
  
  [[nodiscard]]
  auto mission_exists(uint64_t mission_id) ->expected_ec<bool>;
  
  [[nodiscard]]
  auto load_missions() -> expected_ec<std::vector<info::mission_t>>;
  
  [[nodiscard]]
  auto change_mission_status(
    uint64_t mission_id, info::mission_status_e const status, std::chrono::sys_seconds when
  ) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::mission_cargo_t const & value) -> expected_ec<void>;

  ///\brief how much of what has to be brought in total for the open missions
  [[nodiscard]]
  auto load_cargo_needs() -> expected_ec<std::vector<info::cargo_need_t>>;

  ///\brief markets that produce the commodity, even when they happen to have none in stock
  [[nodiscard]]
  auto load_producers() -> expected_ec<std::vector<info::supply_option_t>>;

  ///\brief where it can be bought among the markets we know, with stock covering the need
  [[nodiscard]]
  auto load_supply_options() -> expected_ec<std::vector<info::supply_option_t>>;

  ///\brief trade rates against this market, worked out over the other markets we know
  ///\param bring_here true - buy elsewhere and sell here; false - buy here and carry away
  [[nodiscard]]
  auto load_trade_options(uint64_t market_id, unsigned limit, bool bring_here)
    -> expected_ec<std::vector<info::trade_option_t>>;

  ///\brief MissionAccepted proves the mission is open, even when the row already exists
  [[nodiscard]]
  auto reopen_mission(uint64_t mission_id, std::chrono::sys_seconds expiry) -> expected_ec<void>;

  ///\brief closing a mission together with the sum the game actually paid
  [[nodiscard]]
  auto complete_mission(uint64_t mission_id, std::chrono::sys_seconds when, uint64_t reward) -> expected_ec<void>;

  ///\brief the Missions event lists everything the game considers open - the rest closed without us
  [[nodiscard]]
  auto expire_missions_outside(std::span<uint64_t const> active, std::chrono::sys_seconds when) -> expected_ec<void>;

  [[nodiscard]]
  auto redirect_mission(uint64_t mission_id, std::string_view system, std::string_view station, std::string_view settlment)-> expected_ec<void>;
  
  [[nodiscard]]
  auto carrier_oid( std::string_view name ) -> expected_ec<std::optional<int64_t>>;
  
  [[nodiscard]]
  auto load_carrier(std::string_view carrier_id) -> expected_ec<std::optional<info::carrier_t>>;

  [[nodiscard]]
  auto update_carrier(info::carrier_t const & value) -> expected_ec<void>;

  ///\brief fills in the micro resource dictionary - each source contributes a different part
  [[nodiscard]]
  auto store(info::micro_resource_t const & value) -> expected_ec<void>;

  ///\brief stores a micro resource sale, skipping the ones already known
  [[nodiscard]]
  auto store(info::micro_sale_t const & sale, std::span<info::micro_sale_item_t const> items) -> expected_ec<void>;

  ///\brief stores a collected micro resource, skipping the ones already known
  [[nodiscard]]
  auto store(info::micro_acquisition_t const & value) -> expected_ec<void>;

  ///\brief the bartender's shelf as of the last reading, with the time of it
  [[nodiscard]]
  auto load_carrier_stock(std::string_view carrier_id) -> expected_ec<std::vector<info::carrier_stock_t>>;

  ///\brief marks a carrier as one's own, or takes that mark off
  ///\detail until now the flag existed in the schema and was carefully preserved on every
  /// price reading, but nothing in the whole program could set it
  [[nodiscard]]
  auto set_carrier_tracked(std::string_view carrier_id, bool tracked) -> expected_ec<void>;

  ///\brief carriers we have seen, one's own first
  [[nodiscard]]
  auto load_carriers() -> expected_ec<std::vector<info::carrier_t>>;

  ///\brief completed missions per faction since the given moment
  ///\detail a system_address other than zero narrows it to the missions taken in that system
  [[nodiscard]]
  auto load_mission_stats(std::chrono::sys_seconds since, uint64_t system_address)
    -> expected_ec<std::vector<info::mission_stat_t>>;

  ///\brief finds summed per material, split by how they were obtained
  [[nodiscard]]
  auto load_acquisition_summary(std::chrono::sys_seconds since)
    -> expected_ec<std::vector<info::acquisition_summary_t>>;

  ///\brief the influence a handed-in mission had on one faction in one system, skipping known rows
  [[nodiscard]]
  auto store(info::mission_influence_t const & value) -> expected_ec<void>;

  ///\brief when we last looked at this system - any faction will do, a reading covers them all
  [[nodiscard]]
  auto last_system_seen(uint64_t system_address) -> expected_ec<std::optional<std::chrono::sys_seconds>>;

  ///\brief a trace of a tick, skipping windows already known
  [[nodiscard]]
  auto store(info::tick_observation_t const & value) -> expected_ec<void>;

  ///\brief the last observed ticks of the given kind, newest first
  ///\detail windows from one day are intersected - every system visited narrows the result
  [[nodiscard]]
  auto load_recent_ticks(info::tick_kind_e kind, uint32_t within_days)
    -> expected_ec<std::vector<info::tick_fact_t>>;

  ///\brief when what the tick recalculates last changed in this system
  ///
  /// The question "has it recalculated here" is answered by the change itself, with no bracketing: influence
  /// moves at the tick and nowhere else, so the date of the last change IS the date of the last recalculation
  /// seen in this system. Windows are needed only to pin down the HOUR of the galaxy-wide wave,
  /// where the quantity wanted is a moment rather than a fact
  [[nodiscard]]
  auto last_local_tick(uint64_t system_address, info::tick_kind_e kind)
    -> expected_ec<std::optional<std::chrono::sys_seconds>>;

  ///\brief how regularly the recalculation comes - from the same waves as load_recent_ticks
  [[nodiscard]]
  auto load_tick_stats(info::tick_kind_e kind, uint32_t within_days) -> expected_ec<info::tick_stats_t>;

  ///\brief effort in pluses set against the movement of influence, BGS day by day
  ///\detail days are separated by detected recalculation waves, not by a fixed hour - that moves every few
  /// days and at a weekend may not come at all. a system_address other than zero narrows it to one system
  [[nodiscard]]
  auto load_bgs_effort(uint32_t within_days, uint64_t system_address)
    -> expected_ec<std::vector<info::bgs_effort_t>>;

  ///\brief stores a neutron route, replacing the previous one - we keep one, the one being flown
  [[nodiscard]]
  auto store_neutron_route(std::span<info::neutron_waypoint_t const> route) -> expected_ec<void>;

  ///\brief the remembered neutron route in flight order
  [[nodiscard]]
  auto load_neutron_route() -> expected_ec<std::vector<info::neutron_waypoint_t>>;

  ///\brief stores an ordered ship transfer, skipping the ones already known
  [[nodiscard]]
  auto store(info::ship_transfer_t const & value) -> expected_ec<void>;

  ///\brief transfers that have not arrived as of the given moment
  [[nodiscard]]
  auto load_transfers_in_flight(std::chrono::sys_seconds now) -> expected_ec<std::vector<info::ship_transfer_t>>;

  ///\brief records a stop at a port, moving the mark forward on a repeat visit
  [[nodiscard]]
  auto store(info::port_visit_t const & value) -> expected_ec<void>;

  ///\brief the last port we stood at with a ship
  [[nodiscard]]
  auto load_last_port() -> expected_ec<std::optional<info::port_visit_t>>;

  ///\brief systems we really worked in - the ones with recorded mission influence
  [[nodiscard]]
  auto load_bgs_systems() -> expected_ec<std::vector<info::system_ref_t>>;

  ///\brief announced wars together with the moment we first saw them running
  [[nodiscard]]
  auto load_war_onsets() -> expected_ec<std::vector<info::war_onset_t>>;

  ///\brief how many war recalculations are left before each running conflict is settled
  [[nodiscard]]
  auto load_war_countdown(uint64_t system_address) -> expected_ec<std::vector<info::war_countdown_t>>;

  [[nodiscard]]
  auto store(info::fcmaterial_t const & value) -> expected_ec<void>;
  
  [[nodiscard]]
  auto store(star_system_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store_fss_complete(uint64_t system_address) -> expected_ec<void>;

  [[nodiscard]]
  auto store_system_location(uint64_t system_address, std::array<double, 3> const & loc) -> expected_ec<void>;

  /// the system described - economy, government, allegiance, security, population, controlling faction
  [[nodiscard]]
  auto update_system_info(star_system_t const & system) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, bary_centre_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, body_t const & value) -> expected_ec<uint64_t>;

  [[nodiscard]]
  auto store_dss_complete(uint64_t system_address, events::body_id_t body_id) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::signal_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::genus_t const & value) -> expected_ec<void>;

  /// gatunek dopisywany do rodzaju znanego z mapowania, po pobraniu probki
  [[nodiscard]]
  auto store_genus_species(
    uint64_t system_address,
    events::body_id_t body_id,
    std::string_view genus,
    std::string_view species,
    bool sampled
  ) -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, ring_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto oid_for_body(uint64_t system_address, events::body_id_t body_id) -> expected_ec<std::optional<uint64_t>>;

  [[nodiscard]]
  auto store(uint64_t system_address, events::body_id_t body_id, std::span<events::signal_t const> value)
    -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, events::body_id_t body_id, std::span<events::genus_t const> value)
    -> expected_ec<void>;

  [[nodiscard]]
  auto store(uint64_t system_address, std::span<ring_t const> value) -> expected_ec<void>;

  [[nodiscard]]
  auto store_ring_body_id(
    uint64_t system_address,
    events::body_id_t parent_body_id,
    std::string_view ring_name,
    events::body_id_t ring_body_id
  ) -> expected_ec<void>;
  [[nodiscard]]
  auto store(uint64_t ref_body_oid, events::atmosphere_element_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto faction_oid( std::string_view name ) -> expected_ec<std::optional<uint64_t>>;
  
  [[nodiscard]]
  auto load_faction( std::string_view name )-> expected_ec<std::optional<info::faction_info_t>>;
  
  [[nodiscard]]
  auto load_factions() -> expected_ec<std::vector<info::faction_info_t>>;

  [[nodiscard]]
  ///\brief a faction's identity goes to the shared galaxy, its reputation to the personal database
  ///\detail with_reputation=false when importing somebody else's journal: we take the world, not the reputation
  [[nodiscard]]
  auto update_faction_info(info::faction_info_t const & faction, bool with_reputation = true) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::faction_influence_t const & value) -> expected_ec<void>;

  /// the last recorded influence entry for a faction/system pair
  [[nodiscard]]
  auto last_influence(int64_t faction_oid, uint64_t system_address)
    -> expected_ec<std::optional<info::faction_influence_t>>;

  ///\brief records that a faction was in the system at this reading, even when nothing changed
  [[nodiscard]]
  auto store_faction_seen(int64_t faction_oid, uint64_t system_address, std::chrono::sys_seconds when)
    -> expected_ec<void>;

  ///\brief factions present at the newest reading of the system - the rest have left it
  [[nodiscard]]
  auto load_present_factions(uint64_t system_address) -> expected_ec<std::vector<info::faction_ref_t>>;

  /// the whole influence history in a system, every faction, ascending by time
  [[nodiscard]]
  auto load_influence_history(uint64_t system_address) -> expected_ec<std::vector<info::faction_influence_t>>;

  /// systems for which we have a recorded influence history
  [[nodiscard]]
  auto load_systems_with_influence() -> expected_ec<std::vector<info::system_ref_t>>;

  ///\brief a system signal; repeats of the same name in the same system are skipped
  [[nodiscard]]
  auto store(system_signal_t const & value) -> expected_ec<void>;

  [[nodiscard]]
  auto load_system_signals(uint64_t system_address) -> expected_ec<std::vector<system_signal_t>>;

  ///\brief a station's identity, rebuildable from journals
  [[nodiscard]]
  auto store(info::station_t const & value) -> expected_ec<void>;

  ///\brief the time of the last market reading, from the database gathered live
  [[nodiscard]]
  auto load_market_info(uint64_t market_id) -> expected_ec<std::optional<info::market_info_t>>;

  [[nodiscard]]
  auto load_station(uint64_t market_id) -> expected_ec<std::optional<info::station_t>>;

  ///\brief a station by the name seen in a system signal
  [[nodiscard]]
  auto load_station(uint64_t system_address, std::string_view name) -> expected_ec<std::optional<info::station_t>>;

  ///\brief the account this personal database belongs to
  [[nodiscard]]
  auto store_owner(info::db_owner_t const & owner) -> expected_ec<void>;

  [[nodiscard]]
  auto load_owner() -> expected_ec<std::optional<info::db_owner_t>>;

  ///\brief a system's stations as known from journals - dockings, markets, mission targets
  ///\detail a scanner signal can fail to mention a settlement, or a port only just built,
  /// and a station we actually stood in is stronger evidence than a missing signal
  [[nodiscard]]
  auto load_stations(uint64_t system_address) -> expected_ec<std::vector<info::station_t>>;

  ///\brief a market's contents joined with the commodity dictionary
  [[nodiscard]]
  auto load_market_entries(uint64_t market_id) -> expected_ec<std::vector<info::market_entry_t>>;

  ///\brief replaces a market's entire contents with a fresh reading and marks the update time
  [[nodiscard]]
  auto replace_market(
    uint64_t market_id,
    std::chrono::sys_seconds updated,
    std::span<info::commodity_t const> commodities,
    std::span<info::market_item_t const> items
  ) -> expected_ec<void>;

  [[nodiscard]]
  auto store(info::conflict_t const & value) -> expected_ec<void>;

  /// the last recorded conflict state of these two factions in the system
  [[nodiscard]]
  auto last_conflict(uint64_t system_address, std::string_view faction1, std::string_view faction2)
    -> expected_ec<std::optional<info::conflict_t>>;

  /// conflicts in a system, ascending by time
  [[nodiscard]]
  auto load_conflicts(uint64_t system_address) -> expected_ec<std::vector<info::conflict_t>>;

  [[nodiscard]]
  auto load_system(uint64_t system_address) -> expected_ec<std::optional<star_system_t>>;
  auto close() -> void;
  };
