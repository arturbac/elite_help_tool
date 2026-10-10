// database_storage_t: missions and what they need, pay and move
#include "database_storage_impl.h"
#include <bar_sales.h>

auto database_storage_t::store(info::mission_t const & value) -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));

  return sqlite::insert_into<info::mission_t, true>(db_->db, "mission_id"sv, sql_iface::tables::mission, value);
  }

auto database_storage_t::load_missions() -> expected_ec<std::vector<info::mission_t>>
  {
  return sqlite::select_from<info::mission_t>(
    db_->db,
    sql_iface::tables::mission,
    // redirected means done and waiting to be handed in - past its deadline it is closed just as an
    // unhanded one is, because either it was lost or the handing-in never reached the journal
    std::format(
      " WHERE expiry > '{:%Y-%m-%dT%H:%M:%SZ}' AND (status='accepted' OR status='redirected')",
      std::chrono::system_clock::now()
    )
  );
  }

auto database_storage_t::mission_exists(uint64_t mission_id) ->expected_ec<bool>
{
  if( auto res{sqlite::select_signle_from<uint32_t>(db_->db,
    std::format("select count(*) from {} where mission_id={}",sql_iface::tables::mission,mission_id) )}; not res)
    return cxx23::unexpected{res.error()};
  else
    return *res != 0;
}

auto database_storage_t::change_mission_status(
  uint64_t mission_id, info::mission_status_e const status, std::chrono::sys_seconds when
) -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='{:%Y-%m-%dT%H:%M:%SZ}' WHERE mission_id={}",
    sql_iface::tables::mission,
    status,
    when,
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(info::mission_cargo_t const & value) -> expected_ec<void>
  {
  // replaying the journal meets the same mission again, so there is to be one row only
  return sqlite::insert_into<info::mission_cargo_t, true>(
    db_->db, "mission_id"sv, sql_iface::tables::mission_cargo, value
  );
  }

auto database_storage_t::load_cargo_needs() -> expected_ec<std::vector<info::cargo_need_t>>
  {
  return sqlite::select_from<info::cargo_need_t>(
    db_->db,
    std::format(
      "(SELECT mc.commodity AS commodity, sum(mc.count) AS count"
      " FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      " WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      " GROUP BY mc.commodity ORDER BY count DESC)",
      sql_iface::tables::mission_cargo,
      sql_iface::tables::mission,
      info::mission_status_e::accepted,
      info::mission_status_e::redirected
    ),
    ""
  );
  }

auto database_storage_t::load_producers() -> expected_ec<std::vector<info::supply_option_t>>
  {
  // the same as load_supply_options, but without the condition on stock - what matters is the bare fact
  // that the market trades in it, so as to tell a momentary emptiness from having no source at all
  return sqlite::select_from<info::supply_option_t>(
    db_->db,
    std::format(
      // the missions' needs worked out once and the markets reached from them - as a subquery in FROM the
      // planner walked every market item first and summed the needs again for each, seconds for an empty answer
      "(WITH n AS MATERIALIZED (SELECT mc.commodity AS commodity, sum(mc.count) AS needed"
      "       FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      "       WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      "       GROUP BY mc.commodity)"
      " SELECT i.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(st.station_type,'') AS station_type,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " n.needed AS needed,"
      " i.stock AS stock,"
      " i.buy_price AS buy_price"
      " FROM n"
      " JOIN {4} c ON lower(c.name) = lower(n.commodity)"
      // CROSS JOIN keeps this order: the planner left to itself walks every market item, since lower()
      // in the join above hides the commodity's id from it - 21 ms against 1.5 with a dozen missions
      " CROSS JOIN {5} i ON i.commodity_id = c.id AND i.producer <> 0"
      " LEFT JOIN {6} st ON st.market_id = i.market_id"
      " LEFT JOIN {7} ss ON ss.system_address = st.system_address)",
      sql_iface::tables::mission_cargo,
      sql_iface::tables::mission,
      info::mission_status_e::accepted,
      info::mission_status_e::redirected,
      sql_iface::tables::commodity,
      sql_iface::tables::market_item,
      sql_iface::tables::station,
      sql_iface::tables::star_system
    ),
    ""
  );
  }

auto database_storage_t::load_supply_options() -> expected_ec<std::vector<info::supply_option_t>>
  {
  // the commodity dictionary and the stock sit in the live database, the missions in the main one - hence one query across both
  return sqlite::select_from<info::supply_option_t>(
    db_->db,
    std::format(
      // the missions' needs worked out once and the markets reached from them - as a subquery in FROM the
      // planner walked every market item first and summed the needs again for each, seconds for an empty answer
      "(WITH n AS MATERIALIZED (SELECT mc.commodity AS commodity, sum(mc.count) AS needed"
      "       FROM {0} mc JOIN {1} m ON m.mission_id = mc.mission_id"
      "       WHERE m.status IN ('{2}','{3}') AND mc.commodity <> ''"
      "       GROUP BY mc.commodity)"
      " SELECT i.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(st.station_type,'') AS station_type,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " n.needed AS needed,"
      " i.stock AS stock,"
      " i.buy_price AS buy_price"
      " FROM n"
      " JOIN {4} c ON lower(c.name) = lower(n.commodity)"
      // CROSS JOIN keeps this order: the planner left to itself walks every market item, since lower()
      // in the join above hides the commodity's id from it - 21 ms against 1.5 with a dozen missions
      " CROSS JOIN {5} i ON i.commodity_id = c.id AND i.stock >= n.needed AND i.buy_price > 0"
      " LEFT JOIN {6} st ON st.market_id = i.market_id"
      " LEFT JOIN {7} ss ON ss.system_address = st.system_address)",
      sql_iface::tables::mission_cargo,
      sql_iface::tables::mission,
      info::mission_status_e::accepted,
      info::mission_status_e::redirected,
      sql_iface::tables::commodity,
      sql_iface::tables::market_item,
      sql_iface::tables::station,
      sql_iface::tables::star_system
    ),
    ""
  );
  }

auto database_storage_t::reopen_mission(uint64_t mission_id, std::chrono::sys_seconds expiry) -> expected_ec<void>
  {
  // the Missions event is a snapshot from the moment the game started; replayed later it closes missions
  // taken after it, whereas MissionAccepted is the stronger witness - it says outright that at that
  // moment the mission was open
  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='', expiry='{:%Y-%m-%dT%H:%M:%SZ}' WHERE mission_id={}",
    sql_iface::tables::mission,
    info::mission_status_e::accepted,
    expiry,
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::complete_mission(uint64_t mission_id, std::chrono::sys_seconds when, uint64_t reward)
  -> expected_ec<void>
  {
  // the sum in MissionAccepted is the offer; only MissionCompleted says what actually came in
  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='{:%Y-%m-%dT%H:%M:%SZ}', reward={} WHERE mission_id={}",
    sql_iface::tables::mission,
    info::mission_status_e::completed,
    when,
    reward,
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::expire_missions_outside(std::span<uint64_t const> active, std::chrono::sys_seconds when)
  -> expected_ec<void>
  {
  std::string listed;
  for(uint64_t const mission_id: active)
    {
    if(not listed.empty())
      listed += ',';
    listed += std::to_string(mission_id);
    }

  // an empty list carries information too - it means the game has no open mission left
  std::string const exclusion{listed.empty() ? std::string{} : std::format(" AND mission_id NOT IN ({})", listed)};

  std::string query{std::format(
    "UPDATE {} SET status='{}', closed='{:%Y-%m-%dT%H:%M:%SZ}' WHERE status IN ('{}','{}'){}",
    sql_iface::tables::mission,
    info::mission_status_e::expired,
    when,
    info::mission_status_e::accepted,
    info::mission_status_e::redirected,
    exclusion
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::redirect_mission(
  uint64_t mission_id, std::string_view system, std::string_view station, std::string_view settlement
) -> expected_ec<void>
  {
  std::string query{std::format(
    "UPDATE {} SET status='{}', redirected_system='{}', redirected_station='{}', redirected_settlement='{}' WHERE "
    "mission_id={}",
    sql_iface::tables::mission,
    info::mission_status_e::redirected,
    sqlite::escape_sql_quotes(system),
    sqlite::escape_sql_quotes(station),
    sqlite::escape_sql_quotes(settlement),
    mission_id
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::load_mission_rewards(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<bar::mission_reward_row_t>>
  {
  // a reward is written in the same event that completes the mission, so the second of it is the link
  return sqlite::select_from<bar::mission_reward_row_t>(
    db_->db,
    std::format(
      "(SELECT m.mission_id AS mission_id, m.type AS type, coalesce(a.name, '') AS name,"
      " coalesce(r.category, '') AS category, coalesce(a.count, 0) AS count"
      " FROM {0} m LEFT JOIN {1} a ON a.timestamp = m.closed AND a.source = 'mission_reward'"
      " LEFT JOIN {3} r ON r.name = a.name"
      " WHERE m.status = 'completed' AND m.closed >= '{2:%Y-%m-%dT%H:%M:%SZ}')",
      sql_iface::tables::mission,
      sql_iface::tables::micro_acquisition,
      since,
      sql_iface::tables::micro_resource
    ),
    ""
  );
  }

auto database_storage_t::load_mission_stats(std::chrono::sys_seconds since, uint64_t system_address)
  -> expected_ec<std::vector<info::mission_stat_t>>
  {
  // narrowing to a system goes through the station the mission was taken at,
  // and each source has its own station alias, so the condition is built separately for each
  auto const scope_for{
    [system_address](std::string_view alias) -> std::string
    {
      if(system_address == 0)
        return {};
      return std::format(" AND {}.system_address = {}", alias, system_address);
    }
  };
  std::string const scope{scope_for("st")};
  std::string const inner_scope{scope_for("bs")};

  return sqlite::select_from<info::mission_stat_t>(
    db_->db,
    std::format(
      "(SELECT m.faction AS faction, count(*) AS missions, sum(m.reward) AS rewards,"
      " (SELECT b.type FROM {0} b LEFT JOIN {1} bs ON bs.market_id = b.market_id"
      "  WHERE b.faction = m.faction AND b.status = 'completed' AND b.closed >= '{2:%Y-%m-%dT%H:%M:%SZ}'{4}"
      "  GROUP BY b.type ORDER BY count(*) DESC LIMIT 1) AS top_type"
      " FROM {0} m LEFT JOIN {1} st ON st.market_id = m.market_id"
      " WHERE m.status = 'completed' AND m.closed >= '{2:%Y-%m-%dT%H:%M:%SZ}'{3}"
      " GROUP BY m.faction ORDER BY missions DESC)",
      sql_iface::tables::mission,
      sql_iface::tables::station,
      since,
      scope,
      inner_scope
    ),
    ""
  );
  }

auto database_storage_t::store(info::mission_influence_t const & value) -> expected_ec<void>
  {
  // a rebuild from journals repeats every mission - mission, faction and system tell them apart
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE mission_id={} AND faction='{}' AND system_address={}",
    sql_iface::tables::mission_influence,
    value.mission_id,
    sqlite::escape_sql_quotes(value.faction),
    value.system_address
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::mission_influence, value);
  }
