///\brief every journal event and every file beside the journals, read through json_io.h - each type once
#include "json_io_impl.h"
#include <elite_events.h>

namespace eht::json
  {
#define EHT_JSON_READ(type) template auto read_lenient(type & value, std::string const & text) -> error_t;
#define EHT_JSON_READ_FILE(type) template auto read_file_lenient(type & value, std::string const & path) -> error_t;

// the line's head, read first to know which event it is
EHT_JSON_READ(events::generic_event_t)

// the files the game keeps beside the journals
EHT_JSON_READ_FILE(events::status_file_t)
EHT_JSON_READ_FILE(events::market_file_t)
EHT_JSON_READ_FILE(events::cargo_file_t)
EHT_JSON_READ_FILE(events::nav_route_t)
EHT_JSON_READ(events::fcmaterials_t)
EHT_JSON_READ_FILE(events::fcmaterials_t)

// the events of event_holder_t
EHT_JSON_READ(events::fsd_jump_t)
EHT_JSON_READ(events::fsd_target_t)
EHT_JSON_READ(events::start_jump_t)
EHT_JSON_READ(events::fss_discovery_scan_t)
EHT_JSON_READ(events::fss_body_signals_t)
EHT_JSON_READ(events::fss_signal_discovered_t)
EHT_JSON_READ(events::market_t)
EHT_JSON_READ(events::undocked_t)
EHT_JSON_READ(events::docked_t)
EHT_JSON_READ(events::shipyard_transfer_t)
EHT_JSON_READ(events::sell_micro_resources_t)
EHT_JSON_READ(events::buy_micro_resources_t)
EHT_JSON_READ(events::trade_micro_resources_t)
EHT_JSON_READ(events::commit_crime_t)
EHT_JSON_READ(events::approach_settlement_t)
EHT_JSON_READ(events::disembark_t)
EHT_JSON_READ(events::supercruise_entry_t)
EHT_JSON_READ(events::jet_cone_boost_t)
EHT_JSON_READ(events::backpack_change_t)
EHT_JSON_READ(events::fss_all_bodies_found_t)
EHT_JSON_READ(events::scan_bary_centre_t)
EHT_JSON_READ(events::scan_detailed_scan_t)
EHT_JSON_READ(events::saa_scan_complete_t)
EHT_JSON_READ(events::dss_body_signals_t)
EHT_JSON_READ(events::scan_organic_t)
EHT_JSON_READ(events::fuel_scoop_t)
EHT_JSON_READ(events::loadout_t)
EHT_JSON_READ(events::location_t)
EHT_JSON_READ(events::mission_accepted_t)
EHT_JSON_READ(events::mission_abandoned_t)
EHT_JSON_READ(events::mission_completed_t)
EHT_JSON_READ(events::mission_failed_t)
EHT_JSON_READ(events::mission_redirected_t)
EHT_JSON_READ(events::missions_t)
EHT_JSON_READ(events::cargo_t)
EHT_JSON_READ(events::carrier_stats_t)
EHT_JSON_READ(events::ship_targeted_t)
EHT_JSON_READ(events::bounty_t)
EHT_JSON_READ(events::launch_fighter_t)
EHT_JSON_READ(events::dock_fighter_t)
EHT_JSON_READ(events::fighter_destroyed_t)
EHT_JSON_READ(events::fighter_rebuilt_t)
EHT_JSON_READ(events::crew_assign_t)
EHT_JSON_READ(events::npc_crew_rank_t)
EHT_JSON_READ(events::commander_t)
EHT_JSON_READ(events::faction_kill_bond_t)
EHT_JSON_READ(events::book_dropship_t)
EHT_JSON_READ(events::dropship_deploy_t)
EHT_JSON_READ(events::embark_t)
EHT_JSON_READ(events::colonisation_construction_depot_t)
EHT_JSON_READ(events::colonisation_contribution_t)
EHT_JSON_READ(events::colonisation_system_claim_t)
EHT_JSON_READ(events::colonisation_system_claim_release_t)
EHT_JSON_READ(events::carrier_jump_request_t)
EHT_JSON_READ(events::carrier_location_t)
EHT_JSON_READ(events::carrier_jump_cancelled_t)
EHT_JSON_READ(events::carrier_jump_t)
EHT_JSON_READ(events::shipyard_swap_t)
EHT_JSON_READ(events::resurrect_t)
EHT_JSON_READ(events::stored_ships_t)
EHT_JSON_READ(events::shipyard_buy_t)
EHT_JSON_READ(events::shipyard_new_t)
EHT_JSON_READ(events::shipyard_sell_t)
EHT_JSON_READ(events::sell_ship_on_rebuy_t)
EHT_JSON_READ(events::set_user_ship_name_t)

#undef EHT_JSON_READ_FILE
#undef EHT_JSON_READ
  }  // namespace eht::json
