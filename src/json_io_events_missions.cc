///\brief journal events read through json_io.h: missions, fighting and micro resources - each type once
#include "json_io_impl.h"
#include <events/missions.h>
#include <events/combat.h>
#include <events/micro_resources.h>

EHT_JSON_READ(events::sell_micro_resources_t)
EHT_JSON_READ(events::buy_micro_resources_t)
EHT_JSON_READ(events::trade_micro_resources_t)
EHT_JSON_READ(events::commit_crime_t)
EHT_JSON_READ(events::backpack_change_t)
EHT_JSON_READ(events::mission_accepted_t)
EHT_JSON_READ(events::mission_abandoned_t)
EHT_JSON_READ(events::mission_completed_t)
EHT_JSON_READ(events::mission_failed_t)
EHT_JSON_READ(events::mission_redirected_t)
EHT_JSON_READ(events::missions_t)
EHT_JSON_READ(events::ship_targeted_t)
EHT_JSON_READ(events::bounty_t)
EHT_JSON_READ(events::launch_fighter_t)
EHT_JSON_READ(events::dock_fighter_t)
EHT_JSON_READ(events::fighter_destroyed_t)
EHT_JSON_READ(events::fighter_rebuilt_t)
EHT_JSON_READ(events::crew_assign_t)
EHT_JSON_READ(events::npc_crew_rank_t)
EHT_JSON_READ(events::faction_kill_bond_t)
EHT_JSON_READ(events::book_dropship_t)
EHT_JSON_READ(events::dropship_deploy_t)
EHT_JSON_READ(events::resurrect_t)
