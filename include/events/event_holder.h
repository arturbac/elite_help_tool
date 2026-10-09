#pragma once
#include <events/common.h>
#include <events/event_kind.h>
#include <events/system_info.h>
#include <events/navigation.h>
#include <events/exploration.h>
#include <events/station.h>
#include <events/missions.h>
#include <events/ships.h>
#include <events/carrier.h>
#include <events/combat.h>
#include <events/micro_resources.h>
#include <events/colonisation.h>
#include <events/community_goal.h>
#include <variant>

///\brief journal events: every event the tool reads, as one variant - only what dispatches on events needs it
namespace events
  {

using event_holder_t = std::variant<
  fsd_jump_t,
  fsd_target_t,
  start_jump_t,
  fss_discovery_scan_t,
  fss_body_signals_t,
  fss_signal_discovered_t,
  codex_entry_t,
  market_t,
  undocked_t,
  docked_t,
  shipyard_transfer_t,
  sell_micro_resources_t,
  buy_micro_resources_t,
  trade_micro_resources_t,
  commit_crime_t,
  approach_settlement_t,
  disembark_t,
  supercruise_entry_t,
  supercruise_exit_t,
  supercruise_destination_drop_t,
  jet_cone_boost_t,
  backpack_change_t,
  ship_locker_t,
  backpack_t,
  fss_all_bodies_found_t,
  scan_bary_centre_t,
  scan_detailed_scan_t,
  saa_scan_complete_t,
  dss_body_signals_t,
  scan_organic_t,
  fuel_scoop_t,
  loadout_t,
  afmu_repairs_t,
  repair_all_t,
  repair_t,
  location_t,
  mission_accepted_t,
  mission_abandoned_t,
  mission_completed_t,
  mission_failed_t,
  mission_redirected_t,
  missions_t,
  nav_route_t,
  nav_route_clear_t,
  cargo_t,
  carrier_stats_t,
  fcmaterials_t,
  ship_targeted_t,
  bounty_t,
  launch_fighter_t,
  dock_fighter_t,
  fighter_destroyed_t,
  fighter_rebuilt_t,
  crew_assign_t,
  npc_crew_rank_t,
  commander_t,
  faction_kill_bond_t,
  book_dropship_t,
  dropship_deploy_t,
  embark_t,
  died_t,
  colonisation_construction_depot_t,
  colonisation_contribution_t,
  colonisation_system_claim_t,
  colonisation_system_claim_release_t,
  community_goal_t,
  carrier_jump_request_t,
  carrier_location_t,
  carrier_jump_cancelled_t,
  carrier_jump_t,
  shipyard_swap_t,
  resurrect_t,
  stored_ships_t,
  shipyard_buy_t,
  shipyard_new_t,
  shipyard_sell_t,
  sell_ship_on_rebuy_t,
  set_user_ship_name_t>;

  }  // namespace events
