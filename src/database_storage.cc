// database_storage_t: opening the databases, their schema, whose they are and how far the journal is read; the network incidents
#include "database_storage_impl.h"

database_storage_t::database_storage_t(std::string_view db_path) :
    db_path_{db_path},
    db_{std::make_unique<sqlite3_handle_t>()}
  {
  // the side files lie next to the main database and each lives its own life
  std::filesystem::path sibling{db_path};
  sibling.replace_filename("live.sqlite");
  live_db_path_ = sibling.string();
  sibling.replace_filename("galaxy.sqlite");
  galaxy_db_path_ = sibling.string();
  }

database_storage_t::~database_storage_t() { close(); }

auto database_storage_t::open(storage_mode_e mode) -> expected_ec<void>
  {
  int const rc = sqlite3_open(db_path_.c_str(), &db_->db);

  if(rc != SQLITE_OK)
    return cxx23::unexpected(std::make_error_code(std::errc::io_error));

  // reading from the gui and writing from the following thread are separate connections; we wait rather than take SQLITE_BUSY
  sqlite3_busy_timeout(db_->db, 3000);

  // data gathered live and knowledge of the galaxy in separate files - ATTACH creates them when they do not exist
  for(auto const & [schema, path]:
      {std::pair{"live"sv, std::cref(live_db_path_)}, std::pair{"galaxy"sv, std::cref(galaxy_db_path_)}})
    if(
      auto res{sqlite::execute_query_no_result(
        db_->db, std::format("ATTACH DATABASE '{}' AS {};", sqlite::escape_sql_quotes(path.get()), schema)
      )};
      not res
    ) [[unlikely]]
      return res;

  if(mode == storage_mode_e::bulk_import)
    {
    // every insert is a transaction of its own, and importing the whole of the logs makes hundreds of
    // thousands of them - without an fsync per row and with the journal in memory the import runs many
    // times faster. A crash ends in a damaged database, but the import builds it from scratch anyway.
    //
    // the schema prefix is no ornament: an unqualified journal_mode and synchronous reach EVERY attached
    // database, so they would take these safeguards off live.sqlite as well - and that file cannot be
    // rebuilt and is sometimes shared with a second, running instance. main and galaxy the import builds
    // from scratch, so the shortcuts belong there - and are necessary, because most rows go to galaxy;
    // leaving it with an fsync per row slows the whole import down more than tenfold.
    // temp_store belongs to the connection, not to a database, so it stays without a prefix
    for(std::string_view pragma:
        {"PRAGMA main.synchronous = OFF;"sv,
         "PRAGMA main.journal_mode = MEMORY;"sv,
         "PRAGMA galaxy.synchronous = OFF;"sv,
         "PRAGMA galaxy.journal_mode = MEMORY;"sv,
         "PRAGMA temp_store = MEMORY;"sv})
      if(auto res{sqlite::execute_query_no_result(db_->db, pragma)}; not res) [[unlikely]]
        return res;
    }
  else
    {
    // the gui, the tool windows and the overlay read from separate connections while the journal thread
    // writes. With a rollback journal such a reader waits for the writer and after busy_timeout gets
    // "database is locked"; under WAL it does not wait at all, reading the last consistent image beside
    // the write in progress
    for(std::string_view pragma:
        {"PRAGMA journal_mode = WAL;"sv, "PRAGMA live.journal_mode = WAL;"sv, "PRAGMA galaxy.journal_mode = WAL;"sv})
      if(auto res{sqlite::execute_query_no_result(db_->db, pragma)}; not res) [[unlikely]]
        return res;
    }

  // migration first, because create_database checks the shape of the tables and would refuse to open an old one
  if(auto res{migrate_live_schema()}; not res) [[unlikely]]
    return res;

  // every CREATE is IF NOT EXISTS, so an existing database gains the tables and indexes it lacks
  return create_database();
  }

auto database_storage_t::migrate_live_schema() -> expected_ec<void>
  {
  // bumped only when a table's shape changes in a way an older build could misread - not for a column
  // simply added in place, which every build already tolerates via the loop below. A file this build
  // does not recognise is left alone rather than risked: better a refusal to start than a silent
  // rewrite of rows in a shape half-understood
  constexpr int current_schema_version{1};

  for(std::string_view const schema: {"main"sv, "galaxy"sv, "live"sv})
    {
    auto stored{sqlite::select_signle_from<int>(db_->db, std::format("PRAGMA {}.user_version", schema))};
    if(not stored) [[unlikely]]
      return cxx23::unexpected{stored.error()};
    if(*stored and **stored > current_schema_version) [[unlikely]]
      {
      spdlog::critical(
        "{}.sqlite was written by a newer EHT (schema {} > {} this build knows) - refusing to touch it",
        schema,
        **stored,
        current_schema_version
      );
      return cxx23::unexpected(std::make_error_code(std::errc::not_supported));
      }
    }

  // a column added in place instead of rebuilding the whole database. for live.sqlite it is the only
  // way, because that file cannot be rebuilt from journals; for galaxy it is a courtesy - the tool starts
  // at once, and a rebuild will fill the column in hindsight whenever one comes along anyway
  struct addition_t
    {
    std::string_view table;
    std::string_view column;
    std::string_view type;
    };

  for(addition_t const & add:
      {addition_t{sql_iface::tables::market_item, "producer"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::market_item, "consumer"sv, "INTEGER DEFAULT 0"sv},
       // the faction running the place when we last read it - rows written before it was kept stay
       // empty, so a takeover since is never flagged for a reading taken before this was added
       addition_t{sql_iface::tables::market, "controlling_faction"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::station, "controlling_faction"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::station, "dist_from_star_ls"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::station, "body_id"sv, "INTEGER"sv},
       // where on that body the settlement stands, from ApproachSettlement - unknown for rows written
       // before it was kept, or where the approach came from orbit rather than on foot
       addition_t{sql_iface::tables::station, "latitude"sv, "REAL"sv},
       addition_t{sql_iface::tables::station, "longitude"sv, "REAL"sv},
       // when the scan behind a body's orbital elements was taken - rows written before it was kept
       // stay at the epoch, as distant in the past as a position "now" could ever be carried forward to
       addition_t{sql_iface::tables::body, "scanned_at"sv, "TEXT DEFAULT '1970-01-01T00:00:00Z'"sv},
       addition_t{sql_iface::tables::bary_centre, "scanned_at"sv, "TEXT DEFAULT '1970-01-01T00:00:00Z'"sv},
       // what a star orbits - stars written before it was kept stay NULL until a rebuild or a rescan
       addition_t{sql_iface::tables::star_details, "parent_star"sv, "INTEGER"sv},
       addition_t{sql_iface::tables::star_details, "parent_barycenter"sv, "INTEGER"sv},
       // which of the parents is the nearest - rows written before it was kept say 0, unknown, and are
       // placed by the nearest kind guessed
       addition_t{sql_iface::tables::star_details, "nearest_parent"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::planet_details, "nearest_parent"sv, "INTEGER DEFAULT 0"sv},
       // a star's own phase on the orbit it is written above - rows written before it was kept stay 0,
       // as if the star sat still at the moment of every scan
       addition_t{sql_iface::tables::star_details, "ascending_node"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::star_details, "mean_anomaly"sv, "REAL DEFAULT 0"sv},
       // what the discovery scan counted - systems honked before it was kept say 0, as if never honked
       addition_t{sql_iface::tables::star_system, "body_count"sv, "INTEGER DEFAULT 0"sv},
       // the carrier's state from CarrierStats - added in place, because live.sqlite is never created anew
       addition_t{sql_iface::tables::carrier, "carrier_type"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::carrier, "docking_access"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::carrier, "fuel_level"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "jump_range_curr"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "jump_range_max"sv, "REAL DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "total_capacity"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "free_space"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "cargo"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "balance"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "available_balance"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::carrier, "stats_seen"sv, "TEXT DEFAULT ''"sv},
       addition_t{sql_iface::tables::commodity, "key"sv, "TEXT DEFAULT ''"sv},
       // ships flown before it was kept stay empty until a rebuild from journals
       addition_t{sql_iface::tables::ship, "flown"sv, "TEXT DEFAULT ''"sv},
       // missions handed in before the bars were kept say 0 until filled in from the journals
       addition_t{sql_iface::tables::mission_influence, "economy"sv, "INTEGER DEFAULT 0"sv},
       addition_t{sql_iface::tables::mission_influence, "security"sv, "INTEGER DEFAULT 0"sv},
       // before purchases and barters were kept every transaction was a sale, and every item went away
       addition_t{sql_iface::tables::micro_sale, "kind"sv, "TEXT DEFAULT 'sold'"sv},
       addition_t{sql_iface::tables::micro_sale_item, "received"sv, "INTEGER DEFAULT 0"sv},
       // what followed a disconnect - rows found before it was read stay NULL until the rescan below fills them
       addition_t{sql_iface::tables::network_incident, "last_rx_s"sv, "REAL"sv},
       addition_t{sql_iface::tables::network_incident, "reconnect_started_s"sv, "REAL"sv},
       addition_t{sql_iface::tables::network_incident, "reconnected_s"sv, "REAL"sv}})
    {
    auto known{sqlite::table_columns(db_->db, add.table)};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};

    // an empty list means the table is not there yet - it will be created in its final shape at once
    if(known->empty() or std::ranges::find(*known, add.column) != known->end())
      continue;

    spdlog::info("adding column {} to {}", add.column, add.table);
    if(
      auto res{sqlite::execute_query_no_result(
        db_->db, std::format("ALTER TABLE {} ADD COLUMN {} {}", add.table, add.column, add.type)
      )};
      not res
    ) [[unlikely]]
      return res;
    // the disconnects already found get their aftermath only from netLog read again, from its start
    if(add.table == sql_iface::tables::network_incident and add.column == "last_rx_s"sv)
      if(auto res{sqlite::execute_query_no_result(
           db_->db, std::format("UPDATE {} SET netlog_through=''", sql_iface::tables::incident_scan_progress)
         )};
         not res) [[unlikely]]
        return res;
    }

  for(std::string_view const schema: {"main"sv, "galaxy"sv, "live"sv})
    if(auto res{sqlite::execute_query_no_result(
         db_->db, std::format("PRAGMA {}.user_version = {}", schema, current_schema_version)
       )};
       not res) [[unlikely]]
      return res;

  return {};
  }

auto database_storage_t::create_database() -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));

  if(auto res{
       sqlite::create_table<sql_iface::star_system_t>(db_->db, "system_address"sv, sql_iface::tables::star_system)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::bary_centre_t>(db_->db, "oid"sv, sql_iface::tables::bary_centre)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::body_t>(db_->db, "oid"sv, sql_iface::tables::body)}; not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::ring_t>(db_->db, "oid"sv, sql_iface::tables::ring)}; not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::planet_details_t>(db_->db, "oid"sv, sql_iface::tables::planet_details)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::signal_t>(db_->db, "oid"sv, sql_iface::tables::signal)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::genus_t>(db_->db, "oid"sv, sql_iface::tables::genus)}; not res)
    [[unlikely]]
    return res;

  if(auto res{
       sqlite::create_table<sql_iface::atmosphere_element_t>(db_->db, "oid"sv, sql_iface::tables::atmosphere_element)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<sql_iface::star_details_t>(db_->db, "oid"sv, sql_iface::tables::star_details)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<sql_iface::faction_info_t>(db_->db, "oid"sv, sql_iface::tables::faction_info)};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::faction_influence_t>(db_->db, "oid"sv, sql_iface::tables::faction_influence)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::conflict_t>(db_->db, "oid"sv, sql_iface::tables::system_conflict)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<system_signal_t>(db_->db, "oid"sv, sql_iface::tables::system_signal)}; not res)
    [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::tick_observation_t>(db_->db, "oid"sv, sql_iface::tables::tick_observation)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::station_t>(db_->db, "market_id"sv, sql_iface::tables::station)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::settlement_owner_t>(db_->db, "oid"sv, sql_iface::tables::settlement_owner)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::ground_bond_t>(db_->db, "oid"sv, sql_iface::tables::ground_bond)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::colony_claim_t>(db_->db, "system_address"sv, sql_iface::tables::colony_claim)};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::carrier_movement_t>(db_->db, "oid"sv, sql_iface::tables::carrier_movement)};
     not res) [[unlikely]]
    return res;
  if(auto res{
       sqlite::create_table<info::construction_depot_t>(db_->db, "market_id"sv, sql_iface::tables::construction_depot)
     };
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::construction_need_t>(db_->db, "oid"sv, sql_iface::tables::construction_need)};
     not res) [[unlikely]]
    return res;
  if(auto res{
       sqlite::create_table<info::construction_delivery_t>(db_->db, "oid"sv, sql_iface::tables::construction_delivery)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::market_info_t>(db_->db, "market_id"sv, sql_iface::tables::market)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::commodity_t>(db_->db, "id"sv, sql_iface::tables::commodity)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::market_item_t>(db_->db, "oid"sv, sql_iface::tables::market_item)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::mission_t>(db_->db, "mission_id"sv, sql_iface::tables::mission)}; not res)
    [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::mission_cargo_t>(db_->db, "mission_id"sv, sql_iface::tables::mission_cargo)};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{
      sqlite::create_table<info::mission_influence_t>(db_->db, "oid"sv, sql_iface::tables::mission_influence)
    };
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::ship_transfer_t>(db_->db, "oid"sv, sql_iface::tables::ship_transfer)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::port_visit_t>(db_->db, "market_id"sv, sql_iface::tables::port_visit)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::ship_t>(db_->db, "ship_id"sv, sql_iface::tables::ship)}; not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_resource_t>(db_->db, "name"sv, sql_iface::tables::micro_resource)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_sale_t>(db_->db, "oid"sv, sql_iface::tables::micro_sale)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_sale_item_t>(db_->db, "oid"sv, sql_iface::tables::micro_sale_item)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::consumable_use_t>(db_->db, "oid"sv, sql_iface::tables::consumable_use)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::foot_kill_t>(db_->db, "oid"sv, sql_iface::tables::foot_kill)}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::micro_acquisition_t>(db_->db, "oid"sv, sql_iface::tables::micro_acquisition)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::neutron_waypoint_t>(db_->db, "oid"sv, sql_iface::tables::neutron_route)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::construction_abandoned_t>(
       db_->db, "market_id"sv, sql_iface::tables::construction_abandoned
     )};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::network_incident_t>(db_->db, "oid"sv, sql_iface::tables::network_incident)};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::incident_scan_progress_t>(
       db_->db, "id"sv, sql_iface::tables::incident_scan_progress
     )};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_table<info::carrier_cargo_t>(db_->db, "oid"sv, sql_iface::tables::carrier_cargo)};
     not res) [[unlikely]]
    return res;
  if(auto res{
       sqlite::create_table<info::carrier_cargo_change_t>(db_->db, "oid"sv, sql_iface::tables::carrier_cargo_change)
     };
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::carrier_t>(db_->db, "oid"sv, sql_iface::tables::carrier)}; not res)
    [[unlikely]]
    return res;
    
  if(auto res{sqlite::create_table<info::fcmaterial_t>(db_->db, "oid"sv, sql_iface::tables::carrier_materials)}; not res)
    [[unlikely]]
    return res;

  // looking a faction up by name and finding its last influence row happens at every system visited
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::faction_info, "name", "name")}; not res) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::faction_influence, "faction_oid, system_address, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::faction_presence_t>(db_->db, "oid"sv, sql_iface::tables::faction_presence)};
    not res
  ) [[unlikely]]
    return res;

  // the key has to be unique, because recording presence upserts through ON CONFLICT
  if(
    auto res{
      sqlite::create_index(db_->db, sql_iface::tables::faction_presence, "faction_oid, system_address", "key", true)
    };
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::system_conflict, "system_address, faction1, faction2, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  // the same signal comes back with every fss scan, so each one meets a check first
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::system_signal, "system_address, name")}; not res)
    [[unlikely]]
    return res;

  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::market_item, "market_id")}; not res) [[unlikely]]
    return res;
  // and by commodity, for where to get what the missions need
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::market_item, "commodity_id", "commodity")}; not res)
    [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::carrier_materials, "carrier_id, material_id, timestamp"
    )};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_table<info::journal_progress_t>(db_->db, "id"sv, sql_iface::tables::journal_progress)};
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::db_owner_t>(db_->db, "fid"sv, sql_iface::tables::db_owner)}; not res)
    [[unlikely]]
    return res;

  // this character's progress - apart from knowledge of the galaxy, so in the main database
  if(
    auto res{
      sqlite::create_table<info::system_progress_t>(db_->db, "system_address"sv, sql_iface::tables::system_progress)
    };
    not res
  ) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::body_progress_t>(db_->db, "oid"sv, sql_iface::tables::body_progress)};
     not res) [[unlikely]]
    return res;

  if(auto res{sqlite::create_table<info::genus_progress_t>(db_->db, "oid"sv, sql_iface::tables::genus_progress)};
     not res) [[unlikely]]
    return res;

  if(
    auto res{
      sqlite::create_table<info::faction_reputation_t>(db_->db, "faction"sv, sql_iface::tables::faction_reputation)
    };
    not res
  ) [[unlikely]]
    return res;

  // progress upserts through ON CONFLICT, so the keys have to be unique
  if(
    auto res{sqlite::create_index(db_->db, sql_iface::tables::body_progress, "system_address, body_id", "key", true)};
    not res
  ) [[unlikely]]
    return res;

  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::genus_progress, "system_address, body_id, genus", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // a rescan of netLog's whole history checks every incident it finds against what is already stored
  if(
    auto res{sqlite::create_index(db_->db, sql_iface::tables::network_incident, "occurred, category")};
    not res
  ) [[unlikely]]
    return res;

  // every find checks whether we know it already, and there are a hundred and some thousand of them
  if(
    auto res{sqlite::create_index(db_->db, sql_iface::tables::micro_acquisition, "timestamp, market_id, name")};
    not res
  ) [[unlikely]]
    return res;

  // a mission moves several factions at once, so there are many times more rows than missions. The same
  // key serves both to sift out repeats during a rebuild and to search by system
  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::mission_influence, "mission_id, faction, system_address", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // the same window falls once per system, and a rebuild repeats it from the beginning
  if(
    auto res{sqlite::create_index(
      db_->db, sql_iface::tables::tick_observation, "kind, system_address, window_begin, window_end", "key", true
    )};
    not res
  ) [[unlikely]]
    return res;

  // the search for the last ticks goes by the end of the window
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::tick_observation, "kind, window_end", "recent")};
     not res) [[unlikely]]
    return res;

  // a settlement's owners are looked up one settlement at a time, and so are the kills made at it
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::settlement_owner, "market_id, first_seen", "place")};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::ground_bond, "market_id, timestamp", "place")};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::construction_need, "market_id", "site")}; not res)
    [[unlikely]]
    return res;

  // a system is read with its bodies and each body with its details, so every one of these is looked up by
  // its owner - without an index each look is a scan of the whole table, some twenty-five thousand rows,
  // a dozen times for every system read
  for(auto const & [table, column]:
      {std::pair{sql_iface::tables::body, "ref_system_address"sv},
       std::pair{sql_iface::tables::bary_centre, "ref_system_address"sv},
       std::pair{sql_iface::tables::ring, "ref_system_address"sv},
       std::pair{sql_iface::tables::planet_details, "ref_body_oid"sv},
       std::pair{sql_iface::tables::star_details, "ref_body_oid"sv},
       std::pair{sql_iface::tables::atmosphere_element, "ref_body_oid"sv},
       std::pair{sql_iface::tables::signal, "ref_body_oid"sv},
       std::pair{sql_iface::tables::genus, "ref_body_oid"sv},
       std::pair{sql_iface::tables::station, "system_address"sv}})
    if(auto res{sqlite::create_index(db_->db, table, column, "owner")}; not res) [[unlikely]]
      return res;

  // a system's influence and presence are asked for by the system, while the keys above lead with the
  // faction - the first reading of a system, asked once for every tick observation, walked the whole key
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::faction_influence, "system_address, timestamp", "system")};
     not res) [[unlikely]]
    return res;
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::faction_presence, "system_address, last_seen", "system")};
     not res) [[unlikely]]
    return res;
  // the positions of the fleet's and the carriers' systems are looked up by name, a few times a second
  if(auto res{sqlite::create_index(db_->db, sql_iface::tables::star_system, "name", "name")}; not res) [[unlikely]]
    return res;

  return {};
  }

auto database_storage_t::store_journal_progress(std::chrono::sys_seconds last_event) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT INTO {0} (id, last_event) VALUES (1, '{1:%Y-%m-%dT%H:%M:%SZ}')"
      " ON CONFLICT(id) DO UPDATE SET last_event = excluded.last_event",
      sql_iface::tables::journal_progress,
      last_event
    )
  );
  }

auto database_storage_t::load_journal_progress() -> expected_ec<std::optional<std::chrono::sys_seconds>>
  {
  return sqlite::select_signle_from<std::chrono::sys_seconds>(
    db_->db, std::format("SELECT last_event FROM {} WHERE id=1", sql_iface::tables::journal_progress)
  );
  }

auto database_storage_t::store_owner(info::db_owner_t const & owner) -> expected_ec<void>
  {
  std::string query{std::format(
    "INSERT INTO {0} (fid, name) VALUES ('{1}', '{2}') ON CONFLICT(fid) DO UPDATE SET name = excluded.name",
    sql_iface::tables::db_owner,
    sqlite::escape_sql_quotes(owner.fid),
    sqlite::escape_sql_quotes(owner.name)
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::load_owner() -> expected_ec<std::optional<info::db_owner_t>>
  {
  auto res{sqlite::select_from<info::db_owner_t>(db_->db, sql_iface::tables::db_owner, {})};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};
  if(res->empty())
    return std::optional<info::db_owner_t>{};
  return std::optional<info::db_owner_t>{std::move((*res)[0])};
  }

auto database_storage_t::close() -> void
  {
  if(db_->db)
    db_->close();
  }

auto database_storage_t::network_incident_known(std::chrono::sys_seconds occurred, std::string_view category)
  -> expected_ec<bool>
  {
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE occurred='{:%Y-%m-%dT%H:%M:%SZ}' AND category='{}'",
      sql_iface::tables::network_incident,
      occurred,
      sqlite::escape_sql_quotes(category)
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  return *known and **known != 0;
  }

auto database_storage_t::fill_network_incident_times(info::network_incident_t const & value) -> expected_ec<void>
  {
  if(not value.last_rx_s and not value.reconnect_started_s and not value.reconnected_s)
    return {};
  auto const number = [](std::optional<double> v) { return v ? std::format("{}", *v) : std::string{"NULL"}; };
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET last_rx_s=COALESCE(last_rx_s,{}), reconnect_started_s=COALESCE(reconnect_started_s,{}), "
      "reconnected_s=COALESCE(reconnected_s,{}) WHERE occurred='{:%Y-%m-%dT%H:%M:%SZ}' AND category='{}'",
      sql_iface::tables::network_incident,
      number(value.last_rx_s),
      number(value.reconnect_started_s),
      number(value.reconnected_s),
      value.occurred,
      sqlite::escape_sql_quotes(value.category)
    )
  );
  }

auto database_storage_t::store(info::network_incident_t const & value) -> expected_ec<void>
  {
  // rescanning the same netLog/journal files must not duplicate what they already gave
  auto known{network_incident_known(value.occurred, value.category)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(*known)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::network_incident, value);
  }

auto database_storage_t::load_network_incidents(uint32_t limit) -> expected_ec<std::vector<info::network_incident_t>>
  {
  return sqlite::select_from<info::network_incident_t>(
    db_->db, sql_iface::tables::network_incident, std::format("ORDER BY occurred DESC LIMIT {}", limit)
  );
  }

auto database_storage_t::load_latest_incident_oid() -> expected_ec<int64_t>
  {
  // a plain count, not max(oid) - rows are only ever inserted, never updated, so a rising count is exactly
  // as good a sign of something new as the newest id would be, and count(*) is never NULL on an empty table
  auto res{sqlite::select_signle_from<int64_t>(
    db_->db, std::format("SELECT count(*) FROM {}", sql_iface::tables::network_incident)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};
  return *res ? **res : int64_t{0};
  }

auto database_storage_t::load_incident_scan_progress() -> expected_ec<info::incident_scan_progress_t>
  {
  auto rows{sqlite::select_from<info::incident_scan_progress_t>(
    db_->db, sql_iface::tables::incident_scan_progress, "WHERE id=1"
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  return rows->empty() ? info::incident_scan_progress_t{} : std::move((*rows)[0]);
  }

auto database_storage_t::store_incident_scan_progress(
  std::string_view netlog_through, std::string_view journal_through
) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT INTO {0} (id, netlog_through, journal_through) VALUES (1, '{1}', '{2}')"
      " ON CONFLICT(id) DO UPDATE SET netlog_through = excluded.netlog_through,"
      " journal_through = excluded.journal_through",
      sql_iface::tables::incident_scan_progress,
      sqlite::escape_sql_quotes(netlog_through),
      sqlite::escape_sql_quotes(journal_through)
    )
  );
  }
