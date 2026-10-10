// database_storage_t: star systems and their bodies, rings, signals and life
#include "database_storage_impl.h"
#include <biology.h>
#include <exploration_value.h>

auto database_storage_t::store(star_system_t const & system) -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));

  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::star_system, sql_iface::to_db_fromat(system))};
     not res) [[unlikely]]
    return res;

  if(system.fss_complete)
    if(auto res{store_fss_complete(system.system_address)}; not res) [[unlikely]]
      return res;
  // auto const star_system_oid{sqlite3_last_insert_rowid(db_->db)};
  for(bary_centre_t const & bc: system.bary_centre)
    if(auto res{store(system.system_address, bc)}; not res) [[unlikely]]
      return res;

  for(body_t const & b: system.bodies)
    if(auto res{store(system.system_address, b)}; not res) [[unlikely]]
      return cxx23::unexpected{res.error()};
  return {};
  }

auto database_storage_t::store_fss_complete(uint64_t system_address) -> expected_ec<void>
  {
  // a scan belongs to the character, not to the system - hence a table of its own in the main database
  std::string query{std::format(
    "INSERT INTO {0} (system_address, fss_complete) VALUES ({1}, 1)"
    " ON CONFLICT(system_address) DO UPDATE SET fss_complete = 1",
    sql_iface::tables::system_progress,
    system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

namespace
  {
///\brief the query behind the history, over whichever names the tables have on the connection
[[nodiscard]]
auto species_history_source(
  std::string_view genus, std::string_view body, std::string_view planet_details, std::string_view star_details
) -> std::string
  {
  // the planet and the star both have a surface temperature, so the join is wrapped in a query of its own
  // that names each column once - the generator then reads it like a table
  return std::format(
    "(SELECT g.genus AS genus, g.species AS species, pd.planet_class AS planet_class,"
    " pd.atmosphere_type AS atmosphere_type, pd.volcanism AS volcanism,"
    " pd.surface_temperature AS surface_temperature, pd.surface_gravity AS surface_gravity,"
    " pd.surface_pressure AS surface_pressure, sd.star_type AS star_type,"
    " b.ref_system_address AS system_address, b.body_id AS body_id"
    " FROM {0} g JOIN {1} b ON b.oid = g.ref_body_oid JOIN {2} pd ON pd.ref_body_oid = b.oid"
    " LEFT JOIN {1} bs ON bs.ref_system_address = b.ref_system_address AND bs.body_id = pd.parent_star"
    " LEFT JOIN {3} sd ON sd.ref_body_oid = bs.oid"
    " WHERE g.species <> '') AS history",
    genus,
    body,
    planet_details,
    star_details
  );
  }
  }  // namespace

auto database_storage_t::load_species_history() -> expected_ec<std::vector<bio::species_record_t>>
  {
  return sqlite::select_from<bio::species_record_t>(
    db_->db,
    species_history_source(
      sql_iface::tables::genus,
      sql_iface::tables::body,
      sql_iface::tables::planet_details,
      sql_iface::tables::star_details
    ),
    ""
  );
  }

auto database_storage_t::load_species_history_from(std::string const & galaxy_path)
  -> expected_ec<std::vector<bio::species_record_t>>
  {
  sqlite3_handle_t other;
  if(sqlite3_open_v2(galaxy_path.c_str(), &other.db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
    spdlog::warn("the shared galaxy {} could not be opened", galaxy_path);
    return cxx23::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }
  // the other account may be writing it this very moment - a short wait instead of an error
  sqlite3_busy_timeout(other.db, 200);
  return sqlite::select_from<bio::species_record_t>(
    other.db, species_history_source("genus", "body", "planet_details", "star_details"), ""
  );
  }

auto database_storage_t::load_codex_finds() -> expected_ec<std::vector<bio::find_t>>
  {
  return sqlite::select_from<bio::find_t>(
    db_->db,
    std::format(
      "(SELECT g.genus AS genus, g.species AS species, s.name AS system_name, b.name AS body_name,"
      " b.ref_system_address AS system_address, b.body_id AS body_id, pd.planet_class AS planet_class,"
      " pd.atmosphere_type AS atmosphere_type, pd.volcanism AS volcanism,"
      " pd.surface_temperature AS surface_temperature, pd.surface_gravity AS surface_gravity,"
      " pd.surface_pressure AS surface_pressure, sd.star_type AS star_type,"
      " b.distance_from_arrival_ls AS distance_from_arrival_ls, s.loc_x AS loc_x, s.loc_y AS loc_y,"
      " s.loc_z AS loc_z, coalesce(gp.sampled, 0) AS sampled"
      " FROM {0} g JOIN {1} b ON b.oid = g.ref_body_oid JOIN {2} pd ON pd.ref_body_oid = b.oid"
      " JOIN {4} s ON s.system_address = b.ref_system_address"
      " LEFT JOIN {1} bs ON bs.ref_system_address = b.ref_system_address AND bs.body_id = pd.parent_star"
      " LEFT JOIN {3} sd ON sd.ref_body_oid = bs.oid"
      // the codex is this commander's own - a find is theirs when they logged it, whatever else the galaxy knows
      " JOIN {5} gp ON gp.system_address = b.ref_system_address AND gp.body_id = b.body_id"
      " AND gp.genus = g.genus"
      " WHERE g.species <> '') AS finds",
      sql_iface::tables::genus,
      sql_iface::tables::body,
      sql_iface::tables::planet_details,
      sql_iface::tables::star_details,
      sql_iface::tables::star_system,
      sql_iface::tables::genus_progress
    ),
    " ORDER BY genus, species, system_name, body_name"
  );
  }

auto database_storage_t::store_body_count(uint64_t system_address, uint32_t body_count) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET body_count={} WHERE system_address={}", sql_iface::tables::star_system, body_count, system_address
    )
  );
  }

auto database_storage_t::store_system_location(uint64_t system_address, std::array<double, 3> const & loc)
  -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET loc_x={}, loc_y={}, loc_z={} WHERE system_address={}",
    sql_iface::tables::star_system,
    loc[0],
    loc[1],
    loc[2],
    system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(uint64_t system_address, bary_centre_t const & bc) -> expected_ec<void>
  {
  // a rescan repeats the barycentre with the mean anomaly of the moment - same orbit, one row
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db,
      std::format(
        "DELETE FROM {} WHERE ref_system_address={} AND body_id={}",
        sql_iface::tables::bary_centre,
        system_address,
        bc.body_id
      )
    )};
    not res
  ) [[unlikely]]
    return res;

  return sqlite::insert_into(
    db_->db, "oid"sv, sql_iface::tables::bary_centre, sql_iface::to_db_fromat(system_address, bc)
  );
  }

auto database_storage_t::store(uint64_t system_address, body_t const & value) -> expected_ec<uint64_t>
  {
  if(auto res{
       sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::body, sql_iface::to_db_fromat(system_address, value))
     };
     not res)
    return cxx23::unexpected{res.error()};

  auto const body_oid{sqlite3_last_insert_rowid(db_->db)};
  if(value.body_type() == body_type_e::planet)
    {
    planet_details_t const & pd{std::get<planet_details_t>(value.details)};
    if(auto res{
         sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::planet_details, sql_iface::to_db_fromat(body_oid, pd))
       };
       not res)
      return cxx23::unexpected{res.error()};

    // the state held in memory may already carry a mark of mapping - that belongs to the character, not to the body
    if(pd.mapped)
      if(auto res{store_dss_complete(system_address, value.body_id)}; not res)
        return cxx23::unexpected{res.error()};

    for(events::signal_t const & sig: pd.signals_)
      if(auto res{store(body_oid, sig)}; not res)
        return cxx23::unexpected{res.error()};

    for(events::genus_t const & sig: pd.genuses_)
      if(auto res{store(body_oid, sig)}; not res)
        return cxx23::unexpected{res.error()};

    for(events::atmosphere_element_t const & el: pd.atmosphere_composition)
      if(auto res{store(body_oid, el)}; not res)
        return cxx23::unexpected{res.error()};
    }
  else
    {
    if(auto res{sqlite::insert_into(
         db_->db,
         "oid"sv,
         sql_iface::tables::star_details,
         sql_iface::to_db_fromat(body_oid, std::get<star_details_t>(value.details))
       )};
       not res)
      return cxx23::unexpected{res.error()};
    }

  return body_oid;
  }

auto database_storage_t::store_dss_complete(uint64_t system_address, events::body_id_t body_id) -> expected_ec<void>
  {
  // the key is the system and the game's body number, not an oid - an oid changes with every galaxy rebuild
  std::string query{std::format(
    "INSERT INTO {0} (system_address, body_id, mapped, footfalled) VALUES ({1}, {2}, 1, 0)"
    " ON CONFLICT(system_address, body_id) DO UPDATE SET mapped = 1",
    sql_iface::tables::body_progress,
    system_address,
    body_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store_footfall_complete(uint64_t system_address, events::body_id_t body_id) -> expected_ec<void>
  {
  // the key is the system and the game's body number, not an oid - an oid changes with every galaxy rebuild
  std::string query{std::format(
    "INSERT INTO {0} (system_address, body_id, mapped, footfalled) VALUES ({1}, {2}, 0, 1)"
    " ON CONFLICT(system_address, body_id) DO UPDATE SET footfalled = 1",
    sql_iface::tables::body_progress,
    system_address,
    body_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store_ring_body_id(
  uint64_t system_address, events::body_id_t parent_body_id, std::string_view ring_name, events::body_id_t ring_body_id
) -> expected_ec<void>
  {
  std::string query{
    std::format(
      "UPDATE {} SET body_id={}  WHERE ref_system_address={} AND parent_body_id={} AND name='{}'",
      sql_iface::tables::ring,
      ring_body_id,
      system_address,
      parent_body_id,
      sqlite::escape_sql_quotes(ring_name)
    )

  };
  spdlog::warn("{}", query);
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(uint64_t ref_body_oid, events::signal_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::signal, sql_iface::to_db_fromat(ref_body_oid, value));
  }

auto database_storage_t::store(uint64_t ref_body_oid, events::genus_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::genus, sql_iface::to_db_fromat(ref_body_oid, value));
  }

auto database_storage_t::store_genus_species(
  uint64_t system_address,
  events::body_id_t body_id,
  std::string_view genus,
  std::string_view species,
  bool personal,
  bool analysed
) -> expected_ec<void>
  {
  auto body_oid{oid_for_body(system_address, body_id)};
  if(not body_oid) [[unlikely]]
    return cxx23::unexpected{body_oid.error()};

  // a sample can arrive for a body we have not mapped yet
  if(not *body_oid)
    return {};

  // the species grows there whoever sampled it
  std::string query{std::format(
    "UPDATE {} SET species='{}' WHERE ref_body_oid={} AND genus='{}'",
    sql_iface::tables::genus,
    sqlite::escape_sql_quotes(species),
    **body_oid,
    sqlite::escape_sql_quotes(genus)
  )};
  if(auto res{sqlite::execute_query_no_result(db_->db, query)}; not res) [[unlikely]]
    return res;

  // landed without mapping first, the genus never reached the table - and without its row the species
  // would be lost to the history the next guess is made from
  if(not species.empty())
    {
    auto known{sqlite::select_signle_from<uint64_t>(
      db_->db,
      std::format(
        "SELECT count(*) FROM {} WHERE ref_body_oid={} AND genus='{}'",
        sql_iface::tables::genus,
        **body_oid,
        sqlite::escape_sql_quotes(genus)
      )
    )};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};
    if(not *known or **known == 0u)
      if(
        auto res{sqlite::execute_query_no_result(
          db_->db,
          std::format(
            "INSERT INTO {} (ref_body_oid, genus, species) VALUES ({}, '{}', '{}')",
            sql_iface::tables::genus,
            **body_oid,
            sqlite::escape_sql_quotes(genus),
            sqlite::escape_sql_quotes(species)
          )
        )};
        not res
      ) [[unlikely]]
        return res;
    }

  // the species grows there for everyone; that THIS commander logged it is theirs alone, and what makes
  // it a line of their codex rather than a fact learnt from another account's journals
  if(not personal)
    return {};

  // the sampled mark is never taken off - a further Log of the same genus does not undo the taking
  std::string progress{std::format(
    "INSERT INTO {0} (system_address, body_id, genus, sampled) VALUES ({1}, {2}, '{3}', {4})"
    " ON CONFLICT(system_address, body_id, genus) DO UPDATE SET sampled = max(sampled, excluded.sampled)",
    sql_iface::tables::genus_progress,
    system_address,
    body_id,
    sqlite::escape_sql_quotes(genus),
    analysed ? 1 : 0
  )};
  return sqlite::execute_query_no_result(db_->db, progress);
  }

auto database_storage_t::store(uint64_t system_address, ring_t const & value) -> expected_ec<void>
  {
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::ring, sql_iface::to_db_fromat(system_address, value));
  }

auto database_storage_t::oid_for_body(uint64_t system_address, events::body_id_t body_id)
  -> expected_ec<std::optional<uint64_t>>
  {
  std::string query{std::format(
    "SELECT oid from {} WHERE ref_system_address={} AND body_id='{}'", sql_iface::tables::body, system_address, body_id
  )};
  return sqlite::select_signle_from<uint64_t>(db_->db, query);
  }

auto database_storage_t::store(
  uint64_t system_address, events::body_id_t body_id, std::span<events::signal_t const> signals
) -> expected_ec<void>
  {
  auto resoid{oid_for_body(system_address, body_id)};
  if(not resoid)
    return cxx23::unexpected{resoid.error()};

  std::optional<uint64_t> boid_oid{*resoid};
  if(not boid_oid)
    return {};

  // The event carries the body's whole list, so it replaces what was there rather than adding to
  // it. Appending stacked a fresh copy every time a body was scanned again or a journal replayed
  // after the tool was restarted - one body in the archive had ended up with a hundred and forty
  // copies of the same signal, and the count of what is worth landing for grew with them
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE ref_body_oid={}", sql_iface::tables::signal, *boid_oid)
    )};
    not res
  ) [[unlikely]]
    return res;

  for(events::signal_t const & sig: signals)
    if(auto res{store(*boid_oid, sig)}; not res)
      return cxx23::unexpected{res.error()};

  return {};
  }

auto database_storage_t::store(
  uint64_t system_address, events::body_id_t body_id, std::span<events::genus_t const> genuses
) -> expected_ec<void>
  {
  auto resoid{oid_for_body(system_address, body_id)};
  if(not resoid)
    return cxx23::unexpected{resoid.error()};

  std::optional<uint64_t> boid_oid{*resoid};
  if(not boid_oid)
    return {};

  // the event carries the body's whole list, so a genus not on it goes - but a row already there keeps its
  // species, which a sample wrote and the mapping's list never has
  std::string listed;
  for(events::genus_t const & gen: genuses)
    listed += std::format("{}'{}'", listed.empty() ? "" : ",", sqlite::escape_sql_quotes(gen.Genus_Localised));
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db,
      std::format(
        "DELETE FROM {} WHERE ref_body_oid={}{}",
        sql_iface::tables::genus,
        *boid_oid,
        listed.empty() ? std::string{} : std::format(" AND genus NOT IN ({})", listed)
      )
    )};
    not res
  ) [[unlikely]]
    return res;

  for(events::genus_t const & gen: genuses)
    {
    auto known{sqlite::select_signle_from<uint64_t>(
      db_->db,
      std::format(
        "SELECT count(*) FROM {} WHERE ref_body_oid={} AND genus='{}'",
        sql_iface::tables::genus,
        *boid_oid,
        sqlite::escape_sql_quotes(gen.Genus_Localised)
      )
    )};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};
    if(*known != 0u)
      continue;
    if(auto res{store(*boid_oid, gen)}; not res)
      return cxx23::unexpected{res.error()};
    }

  return {};
  }

auto database_storage_t::store(uint64_t system_address, std::span<ring_t const> rings) -> expected_ec<void>
  {
  for(ring_t const & ring: rings)
    if(auto res{store(system_address, ring)}; not res)
      return cxx23::unexpected{res.error()};
  return {};
  }

auto database_storage_t::store(uint64_t ref_body_oid, events::atmosphere_element_t const & value) -> expected_ec<void>
  {
  if(auto res{sqlite::insert_into(
       db_->db, "oid"sv, sql_iface::tables::atmosphere_element, sql_iface::to_db_fromat(ref_body_oid, value)
     )};
     not res)
    return cxx23::unexpected{res.error()};
  return {};
  }

auto database_storage_t::update_system_info(star_system_t const & system) -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET economy='{}', second_economy='{}', government='{}', allegiance='{}', security='{}', "
    "controlling_faction='{}', population={} WHERE system_address={}",
    sql_iface::tables::star_system,
    sqlite::escape_sql_quotes(system.economy),
    sqlite::escape_sql_quotes(system.second_economy),
    sqlite::escape_sql_quotes(system.government),
    sqlite::escape_sql_quotes(system.allegiance),
    sqlite::escape_sql_quotes(system.security),
    sqlite::escape_sql_quotes(system.controlling_faction),
    system.population,
    system.system_address
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

namespace sql_iface
  {
///\brief a system's name and position, without the rest of its row
struct system_position_t
  {
  std::string name;
  double loc_x;
  double loc_y;
  double loc_z;
  };
  }  // namespace sql_iface

auto database_storage_t::load_system_positions(std::span<std::string const> names)
  -> expected_ec<std::map<std::string, std::array<double, 3>>>
  {
  std::map<std::string, std::array<double, 3>> result;
  if(names.empty())
    return result;

  std::string list;
  for(std::string const & name: names)
    list += std::format("{}'{}'", list.empty() ? "" : ",", sqlite::escape_sql_quotes(name));

  auto rows{sqlite::select_from<sql_iface::system_position_t>(
    db_->db,
    sql_iface::tables::star_system,
    std::format(" WHERE name IN ({})", list)
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};

  // a system known only by name - from a signal or a mission, never jumped to - has no position; Sol has
  // one at the origin
  for(sql_iface::system_position_t const & row: *rows)
    if(row.loc_x != 0.0 or row.loc_y != 0.0 or row.loc_z != 0.0 or row.name == "Sol")
      result[row.name] = {row.loc_x, row.loc_y, row.loc_z};
  return result;
  }

auto database_storage_t::store(system_signal_t const & value) -> expected_ec<void>
  {
  // the same signal comes back with every fss scan of the system
  std::string query{std::format(
    "SELECT count(*) FROM {} WHERE system_address={} AND name='{}'",
    sql_iface::tables::system_signal,
    value.system_address,
    sqlite::escape_sql_quotes(value.name)
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    {
    // the signal is known already; what counts is when it was last reported
    std::string update{std::format(
      "UPDATE {} SET last_seen='{:%Y-%m-%dT%H:%M:%SZ}' WHERE system_address={} AND name='{}'",
      sql_iface::tables::system_signal,
      value.last_seen,
      value.system_address,
      sqlite::escape_sql_quotes(value.name)
    )};
    return sqlite::execute_query_no_result(db_->db, update);
    }

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::system_signal, value);
  }

auto database_storage_t::load_system_signals(uint64_t system_address)
  -> expected_ec<std::vector<system_signal_t>>
  {
  auto res{sqlite::select_from<system_signal_t>(
    db_->db, sql_iface::tables::system_signal, std::format(" WHERE system_address={} ORDER BY name", system_address)
  )};
  if(not res) [[unlikely]]
    return res;

  // a construction site or a compromised nav beacon disappears from the system, but the row stays behind
  return filter_current_visit(std::move(*res));
  }

auto database_storage_t::load_unvisited_phenomena() -> expected_ec<std::vector<unvisited_phenomenon_t>>
  {
  // a system known only by its signals has no name of its own in star_system - it keeps its number then
  std::string const source{std::format(
    "(SELECT coalesce(y.name, s.system_address) AS system, max(s.last_seen) AS last_seen"
    " FROM {0} s LEFT JOIN {1} y ON y.system_address = s.system_address"
    " WHERE s.signal_type = '{2}' AND NOT EXISTS"
    " (SELECT 1 FROM {0} v WHERE v.system_address = s.system_address AND v.signal_type = '{3}')"
    " GROUP BY s.system_address)",
    sql_iface::tables::system_signal,
    sql_iface::tables::star_system,
    phenomenon_signal_type,
    phenomenon_visit_type
  )};
  return sqlite::select_from<unvisited_phenomenon_t>(db_->db, source, " ORDER BY last_seen DESC");
  }

auto database_storage_t::load_system(uint64_t system_address)
  -> cxx23::expected<std::optional<star_system_t>, std::error_code>
  {
  auto res{sqlite::select_from<sql_iface::star_system_t>(
    db_->db, sql_iface::tables::star_system, std::format(" WHERE system_address='{}'", system_address)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(not res->empty())
    {
    assert(res->size() == 1);
    star_system_t system{to_native_fromat(std::move((*res)[0]))};

    // this character's progress comes separately - galaxy does not know who scanned what
    if(auto progress{sqlite::select_from<info::system_progress_t>(
         db_->db, sql_iface::tables::system_progress, std::format(" WHERE system_address={}", system_address)
       )};
       progress)
      system.fss_complete = not progress->empty() and progress->front().fss_complete;
    else [[unlikely]]
      return cxx23::unexpected{progress.error()};

    auto mapped{sqlite::select_from<info::body_progress_t>(
      db_->db, sql_iface::tables::body_progress, std::format(" WHERE system_address={}", system_address)
    )};
    if(not mapped) [[unlikely]]
      return cxx23::unexpected{mapped.error()};

    auto sampled{sqlite::select_from<info::genus_progress_t>(
      db_->db, sql_iface::tables::genus_progress, std::format(" WHERE system_address={}", system_address)
    )};
    if(not sampled) [[unlikely]]
      return cxx23::unexpected{sampled.error()};

    if(auto signals{load_system_signals(system_address)}; signals)
      system.system_signals = std::move(*signals);
    else [[unlikely]]
      return cxx23::unexpected{signals.error()};

      {
      auto res2{sqlite::select_from<sql_iface::body_t>(
        db_->db, sql_iface::tables::body, std::format(" WHERE ref_system_address='{}'", system_address)
      )};
      if(not res2) [[unlikely]]
        return cxx23::unexpected{res2.error()};

      std::vector<sql_iface::body_t> bodies{std::move(*res2)};
      for(sql_iface::body_t & body: bodies)
        {
        system.bodies.emplace_back(sql_iface::to_native_fromat(std::move(body)));
        body_t & out_body{system.bodies.back()};

        if(body_type_e(body.details_type) == body_type_e::planet)
          {
          auto res3{sqlite::select_from<sql_iface::planet_details_t>(
            db_->db, sql_iface::tables::planet_details, std::format(" WHERE ref_body_oid='{}'", body.oid)
          )};
          if(not res3) [[unlikely]]
            return cxx23::unexpected{res3.error()};
          // a body without its row of details is a broken database, not a reason to read past the rows
          if(res3->empty()) [[unlikely]]
            {
            spdlog::warn("system {}: body {} has no planet details, left out", system_address, out_body.body_id);
            system.bodies.pop_back();
            continue;
            }
          out_body.details = sql_iface::to_native_fromat((*res3)[0]);
          planet_details_t & details{std::get<planet_details_t>(out_body.details)};

          if(auto it{std::ranges::find(*mapped, out_body.body_id, &info::body_progress_t::body_id)};
             it != mapped->end())
            {
            details.mapped = it->mapped;
            details.footfalled = it->footfalled;
            }

            {
            auto res4{sqlite::select_from<sql_iface::signal_t>(
              db_->db, sql_iface::tables::signal, std::format(" WHERE ref_body_oid='{}'", body.oid)
            )};
            if(not res4) [[unlikely]]
              return cxx23::unexpected{res4.error()};
            if(not res4->empty())
              std::ranges::transform(
                *res4,
                std::back_inserter(details.signals_),
                [](sql_iface::signal_t & sig) -> events::signal_t
                { return sql_iface::to_native_fromat(std::move(sig)); }
              );
            }
            {
            auto res4{sqlite::select_from<sql_iface::genus_t>(
              db_->db, sql_iface::tables::genus, std::format(" WHERE ref_body_oid='{}'", body.oid)
            )};
            if(not res4) [[unlikely]]
              return cxx23::unexpected{res4.error()};
            if(not res4->empty())
              std::ranges::transform(
                *res4,
                std::back_inserter(details.genuses_),
                [&sampled, id = out_body.body_id](sql_iface::genus_t & sig) -> events::genus_t
                {
                  events::genus_t gen{sql_iface::to_native_fromat(std::move(sig))};
                  auto const it{std::ranges::find_if(
                    *sampled,
                    [&](info::genus_progress_t const & pr)
                    { return pr.body_id == id and pr.genus == gen.Genus_Localised; }
                  )};
                  gen.Sampled = it != sampled->end() and it->sampled;
                  return gen;
                }
              );
            }
          }
        else
          {
          auto res3{sqlite::select_from<sql_iface::star_details_t>(
            db_->db, sql_iface::tables::star_details, std::format(" WHERE ref_body_oid='{}'", body.oid)
          )};
          if(not res3) [[unlikely]]
            return cxx23::unexpected{res3.error()};
          // a body without its row of details is a broken database, not a reason to read past the rows
          if(res3->empty()) [[unlikely]]
            {
            spdlog::warn("system {}: body {} has no star details, left out", system_address, out_body.body_id);
            system.bodies.pop_back();
            continue;
            }
          out_body.details = sql_iface::to_native_fromat((*res3)[0]);
          }
        // the stored value is the one reckoned at the scan - reckoned again, so a better formula reaches the
        // bodies already known; how efficiently a body was mapped is not kept, so the efficient outcome is assumed
        out_body.value = exploration::aprox_value(out_body);
        }
      }
      // barycentres - what a body of a shared orbit (two stars, or two planets of one pair) itself
      // orbits; without this a system reopened after a restart has every such body's position collapse
      // to its own small local wobble, missing the barycentre's own, usually much larger, offset
      {
      auto res5{sqlite::select_from<sql_iface::bary_centre_t>(
        db_->db, sql_iface::tables::bary_centre, std::format(" WHERE ref_system_address='{}'", system_address)
      )};
      if(not res5) [[unlikely]]
        return cxx23::unexpected{res5.error()};
      for(sql_iface::bary_centre_t const & bc: *res5)
        system.bary_centre.push_back(sql_iface::to_native_fromat(bc));
      }
      // rings
      {
      auto res4{sqlite::select_from<sql_iface::ring_t>(
        db_->db, sql_iface::tables::ring, std::format(" WHERE ref_system_address='{}'", system_address)
      )};
      if(not res4) [[unlikely]]
        return cxx23::unexpected{res4.error()};
      if(not res4->empty())
        {
        for(sql_iface::ring_t & db_ring: *res4)
          {
          ring_t & ring{system.rings.emplace_back(sql_iface::to_native_fromat(std::move(db_ring)))};
          auto res4{sqlite::select_from<sql_iface::signal_t>(
            db_->db, sql_iface::tables::signal, std::format(" WHERE ref_body_oid='{}'", db_ring.oid)
          )};
          if(not res4) [[unlikely]]
            return cxx23::unexpected{res4.error()};
          if(not res4->empty())
            std::ranges::transform(
              *res4,
              std::back_inserter(ring.signals_),
              [](sql_iface::signal_t & sig) -> events::signal_t { return sql_iface::to_native_fromat(std::move(sig)); }
            );
          }
        }
      }
    return system;
    }
  return {};
  }
