///\brief journal events read through json_io.h: scans of bodies and signals - each type once
#include "json_io_impl.h"
#include <events/exploration.h>

EHT_JSON_READ(events::fss_discovery_scan_t)
EHT_JSON_READ(events::fss_body_signals_t)
EHT_JSON_READ(events::fss_signal_discovered_t)
EHT_JSON_READ(events::codex_entry_t)
EHT_JSON_READ(events::fss_all_bodies_found_t)
EHT_JSON_READ(events::scan_bary_centre_t)
EHT_JSON_READ(events::scan_detailed_scan_t)
EHT_JSON_READ(events::saa_scan_complete_t)
EHT_JSON_READ(events::dss_body_signals_t)
EHT_JSON_READ(events::scan_organic_t)
