///\brief journal events read through json_io.h: the line's head, and jumps, routes and where the ship is - each type once
#include "json_io_impl.h"
#include <events/common.h>
#include <events/event_kind.h>
#include <events/system_info.h>
#include <events/navigation.h>

EHT_JSON_READ(events::generic_event_t)
EHT_JSON_READ_FILE(events::nav_route_t)
EHT_JSON_READ(events::fsd_jump_t)
EHT_JSON_READ(events::fsd_target_t)
EHT_JSON_READ(events::start_jump_t)
EHT_JSON_READ(events::supercruise_entry_t)
EHT_JSON_READ(events::jet_cone_boost_t)
EHT_JSON_READ(events::location_t)
EHT_JSON_READ(events::commander_t)
