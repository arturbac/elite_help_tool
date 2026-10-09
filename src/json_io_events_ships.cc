///\brief journal events read through json_io.h: the ships and the carriers - each type once
#include "json_io_impl.h"
#include <events/ships.h>
#include <events/carrier.h>

EHT_JSON_READ(events::fcmaterials_t)
EHT_JSON_READ_FILE(events::fcmaterials_t)
EHT_JSON_READ(events::shipyard_transfer_t)
EHT_JSON_READ(events::fuel_scoop_t)
EHT_JSON_READ(events::loadout_t)
EHT_JSON_READ(events::afmu_repairs_t)
EHT_JSON_READ(events::repair_all_t)
EHT_JSON_READ(events::repair_t)
EHT_JSON_READ(events::cargo_t)
EHT_JSON_READ(events::carrier_stats_t)
EHT_JSON_READ(events::carrier_jump_request_t)
EHT_JSON_READ(events::carrier_location_t)
EHT_JSON_READ(events::carrier_jump_cancelled_t)
EHT_JSON_READ(events::carrier_jump_t)
EHT_JSON_READ(events::shipyard_swap_t)
EHT_JSON_READ(events::stored_ships_t)
EHT_JSON_READ(events::shipyard_buy_t)
EHT_JSON_READ(events::shipyard_new_t)
EHT_JSON_READ(events::shipyard_sell_t)
EHT_JSON_READ(events::sell_ship_on_rebuy_t)
EHT_JSON_READ(events::set_user_ship_name_t)
