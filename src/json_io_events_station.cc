///\brief journal events read through json_io.h: stations, settlements, construction sites, and the files beside the journals - each type once
#include "json_io_impl.h"
#include <events/station.h>
#include <events/companion_files.h>
#include <events/colonisation.h>

EHT_JSON_READ_FILE(events::status_file_t)
EHT_JSON_READ_FILE(events::market_file_t)
EHT_JSON_READ_FILE(events::cargo_file_t)
EHT_JSON_READ(events::market_t)
EHT_JSON_READ(events::undocked_t)
EHT_JSON_READ(events::docked_t)
EHT_JSON_READ(events::approach_settlement_t)
EHT_JSON_READ(events::disembark_t)
EHT_JSON_READ(events::embark_t)
EHT_JSON_READ(events::colonisation_construction_depot_t)
EHT_JSON_READ(events::colonisation_contribution_t)
EHT_JSON_READ(events::colonisation_system_claim_t)
EHT_JSON_READ(events::colonisation_system_claim_release_t)
