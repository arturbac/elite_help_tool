#pragma once
#include <cstdint>

///\brief the database row types declared only - enough for a header that passes them by reference or
/// names them in a signature; whoever touches their members includes the domain header under data/
namespace info
  {
// data/bgs.h
enum struct allegiance_e : uint8_t;
struct bgs_effort_t;
struct conflict_t;
struct faction_effect_t;
struct faction_influence_t;
struct faction_info_t;
struct faction_presence_t;
struct faction_ref_t;
struct faction_reputation_t;
struct faction_state_entry_t;
enum struct government_e : uint8_t;
enum struct happiness_e;
struct mission_influence_t;
struct state_effort_t;
struct state_shift_t;
struct tick_fact_t;
enum struct tick_kind_e : uint8_t;
struct tick_observation_t;
struct tick_stats_t;
struct war_countdown_t;
struct war_onset_t;
// data/carrier.h
struct carrier_cargo_change_t;
struct carrier_cargo_t;
struct carrier_movement_t;
struct carrier_state_t;
struct carrier_stock_t;
struct carrier_t;
struct fcmaterial_t;
// data/colonisation.h
struct colony_claim_t;
struct construction_abandoned_t;
struct construction_delivery_t;
struct construction_depot_t;
struct construction_need_t;
struct construction_site_t;
// data/market.h
struct cargo_need_t;
struct commodity_t;
struct market_entry_t;
struct market_info_t;
struct market_item_t;
struct supply_option_t;
struct trade_option_t;
// data/micro_resources.h
enum struct acquisition_source_e : uint8_t;
struct acquisition_summary_t;
struct bartender_summary_t;
struct bartender_total_t;
struct consumable_summary_t;
struct consumable_use_t;
enum struct foot_kill_e : uint8_t;
struct foot_kill_summary_t;
struct foot_kill_t;
struct micro_acquisition_t;
struct micro_resource_t;
struct micro_sale_item_t;
struct micro_sale_t;
enum struct micro_trade_e : uint8_t;
// data/missions.h
struct mission_cargo_t;
struct mission_stat_t;
enum struct mission_status_e : uint8_t;
struct mission_t;
// data/navigation.h
struct neutron_waypoint_t;
struct route_item_t;
// data/network.h
struct incident_scan_progress_t;
struct network_incident_t;
// data/progress.h
struct body_progress_t;
struct db_owner_t;
struct genus_progress_t;
struct journal_progress_t;
struct system_progress_t;
struct system_ref_t;
// data/ships.h
struct port_visit_t;
struct ship_t;
struct ship_transfer_t;
// data/station.h
struct settlement_owner_t;
struct station_t;
// data/war.h
enum struct cz_intensity_e : uint8_t;
struct ground_bond_t;
struct war_settlement_t;
struct war_view_t;
  }  // namespace info
