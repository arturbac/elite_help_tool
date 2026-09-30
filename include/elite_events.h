#pragma once
///\brief everything read from the journal and what the tool makes of it, at once
///\detail the events live in events/<domain>.h, the tool's own model beside them; a file that needs only
/// part of it includes that part
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
#include <events/companion_files.h>
#include <events/event_holder.h>
#include <star_system.h>
#include <orbit.h>
#include <generic_state.h>
#include <exploration_value.h>
#include <format_credits.h>
