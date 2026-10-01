// database_storage_t: fleet carriers, their cargo and bar, and micro resources
#include "database_storage_impl.h"
#include <bar_sales.h>

auto database_storage_t::load_carriers() -> expected_ec<std::vector<info::carrier_t>>
  {
  // wlasny flotowiec na gorze, reszta to tlo z odwiedzin
  return sqlite::select_from<info::carrier_t>(
    db_->db, sql_iface::tables::carrier, " ORDER BY tracked DESC, carrier_name"
  );
  }

namespace
  {
  ///\brief the readings of a carrier's shelf joined with the dictionary - a reading keeps its time in seconds,
  /// the reading row takes it as the text every other time is kept in
  auto shelf_readings(std::string_view where) -> std::string
    {
    return std::format(
      "(SELECT r.name AS name, r.localised AS localised, r.category AS category, m.price AS price, m.stock AS stock,"
      " m.demand AS demand, strftime('%Y-%m-%dT%H:%M:%SZ', m.timestamp, 'unixepoch') AS timestamp"
      " FROM {} m JOIN {} c ON c.oid = m.carrier_id LEFT JOIN {} r ON r.id = m.material_id{})",
      sql_iface::tables::carrier_materials,
      sql_iface::tables::carrier,
      sql_iface::tables::micro_resource,
      where
    );
    }
  }  // namespace

auto database_storage_t::load_carrier_stock(std::string_view carrier_id)
  -> expected_ec<std::vector<info::carrier_stock_t>>
  {
  // what counts is the last reading, the earlier ones are the history of sales
  return sqlite::select_from<info::carrier_stock_t>(
    db_->db,
    shelf_readings(std::format(
      " WHERE c.carrier_id = '{}' AND m.timestamp = (SELECT max(timestamp) FROM {} WHERE carrier_id = c.oid)"
      " ORDER BY r.category, r.localised",
      sqlite::escape_sql_quotes(carrier_id),
      sql_iface::tables::carrier_materials
    )),
    ""
  );
  }

auto database_storage_t::load_port_sale_rows() -> expected_ec<std::vector<bar::port_sale_row_t>>
  {
  // a carrier's bar pays what its owner set, so only the ports' sales say what a kind is worth
  return sqlite::select_from<bar::port_sale_row_t>(
    db_->db,
    std::format(
      "(SELECT s.oid AS sale_oid, s.price AS price, i.name AS name, i.count AS count"
      " FROM {0} s JOIN {1} i ON i.sale_oid = s.oid"
      " WHERE s.kind = 'sold' AND s.market_id NOT IN (SELECT market_id FROM {2})"
      " AND s.market_id NOT IN (SELECT market_id FROM {3} WHERE station_type = 'FleetCarrier'))",
      sql_iface::tables::micro_sale,
      sql_iface::tables::micro_sale_item,
      sql_iface::tables::carrier,
      sql_iface::tables::station
    ),
    ""
  );
  }

auto database_storage_t::load_micro_resource_commodities() -> expected_ec<std::vector<std::string>>
  {
  // Data, Goods or Assets alike - none of the three ever sits in a station's commodity market, only in
  // a bartender's shelf or a backpack, so a mission asking for any of them never has a market "source"
  // to point at in the first place
  auto rows{sqlite::select_from<info::micro_resource_t>(db_->db, sql_iface::tables::micro_resource, "")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::vector<std::string> names;
  names.reserve(rows->size());
  for(info::micro_resource_t & row: *rows)
    names.push_back(std::move(row.localised));
  return names;
  }

auto database_storage_t::load_micro_resource_names() -> expected_ec<std::map<std::string, std::string>>
  {
  auto rows{sqlite::select_from<info::micro_resource_t>(db_->db, sql_iface::tables::micro_resource, "")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::map<std::string, std::string> names;
  for(info::micro_resource_t & row: *rows)
    names.emplace(std::move(row.name), std::move(row.localised));
  return names;
  }

auto database_storage_t::load_carrier_history(std::string_view carrier_id, std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::carrier_stock_t>>
  {
  // the reading before the period is taken too - without it the first fall in the period has nothing to fall from
  return sqlite::select_from<info::carrier_stock_t>(
    db_->db,
    shelf_readings(std::format(
      " WHERE c.carrier_id = '{0}' AND m.timestamp >= coalesce((SELECT max(timestamp) FROM {1}"
      " WHERE carrier_id = c.oid AND timestamp < {2}), 0)"
      " ORDER BY r.name, m.timestamp",
      sqlite::escape_sql_quotes(carrier_id),
      sql_iface::tables::carrier_materials,
      since.time_since_epoch().count()
    )),
    ""
  );
  }

auto database_storage_t::load_acquisition_summary(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::acquisition_summary_t>>
  {
  // a place's economy comes from the station, so single finds gain it in hindsight.
  // it is counted once, in a single pass over the history - the same per row cost a second
  // "Mostly from" follows the chosen period - otherwise it would speak of a place from half a year ago
  std::string const window{std::format(" WHERE b.timestamp >= '{:%Y-%m-%dT%H:%M:%SZ}'", since)};

  return sqlite::select_from<info::acquisition_summary_t>(
    db_->db,
    std::format(
      "(SELECT a.name AS name, r.localised AS localised, r.category AS category,"
      " sum(CASE WHEN a.source = 'collected' THEN a.count ELSE 0 END) AS collected,"
      " sum(CASE WHEN a.source = 'mission_reward' THEN a.count ELSE 0 END) AS from_missions,"
      " coalesce(e.economy, '?') AS top_economy,"
      " max(a.timestamp) AS last_seen"
      " FROM {0} a"
      " LEFT JOIN {1} r ON r.name = a.name"
      " LEFT JOIN (SELECT name, economy FROM"
      "   (SELECT b.name AS name, coalesce(nullif(st.economy, ''), '?') AS economy,"
      "           row_number() OVER (PARTITION BY b.name ORDER BY sum(b.count) DESC) AS pick"
      "    FROM {0} b LEFT JOIN {2} st ON st.market_id = b.market_id{3}"
      "    GROUP BY b.name, st.economy)"
      "   WHERE pick = 1) e ON e.name = a.name"
      " WHERE a.timestamp >= '{4:%Y-%m-%dT%H:%M:%SZ}'"
      " GROUP BY a.name ORDER BY collected + from_missions DESC)",
      sql_iface::tables::micro_acquisition,
      sql_iface::tables::micro_resource,
      sql_iface::tables::station,
      window,
      since
    ),
    ""
  );
  }

auto database_storage_t::store(info::micro_acquisition_t const & value) -> expected_ec<void>
  {
  // replaying the journal repeats the finds; time, place and material tell them apart
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND market_id={} AND name='{}'",
    sql_iface::tables::micro_acquisition,
    value.timestamp,
    value.market_id,
    sqlite::escape_sql_quotes(value.name)
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::micro_acquisition, value);
  }

auto database_storage_t::store(info::carrier_movement_t const & value) -> expected_ec<void>
  {
  // the same move read twice - a journal read again - is kept once
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE carrier_id={} AND kind='{}' AND timestamp='{:%Y-%m-%dT%H:%M:%SZ}'",
      sql_iface::tables::carrier_movement,
      value.carrier_id,
      value.kind,
      value.timestamp
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(*known and **known != 0u)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier_movement, value);
  }

auto database_storage_t::load_carrier_cargo(uint64_t carrier_id) -> expected_ec<std::vector<info::carrier_cargo_t>>
  {
  return sqlite::select_from<info::carrier_cargo_t>(
    db_->db,
    sql_iface::tables::carrier_cargo,
    std::format(" WHERE carrier_id={} AND count>0 ORDER BY count DESC", carrier_id)
  );
  }

auto database_storage_t::load_carrier_cargo_totals() -> expected_ec<std::map<std::string, int64_t>>
  {
  auto rows{sqlite::select_from<info::carrier_cargo_t>(db_->db, sql_iface::tables::carrier_cargo, " WHERE count>0")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::map<std::string, int64_t> totals;
  for(info::carrier_cargo_t const & row: *rows)
    totals[row.key] += row.count;
  return totals;
  }

auto database_storage_t::change_carrier_cargo(info::carrier_cargo_change_t const & given) -> expected_ec<void>
  {
  if(given.delta == 0)
    return {};
  // the hold names a commodity by its internal name, "steel" or "cmmcomposite" - the market's dictionary
  // has the name the game shows
  info::carrier_cargo_change_t change{given};
  if(std::ranges::none_of(change.commodity, [](char c) { return c >= 'A' and c <= 'Z'; }))
    if(auto names{sqlite::select_from<info::commodity_t>(db_->db, sql_iface::tables::commodity, "")}; names)
      for(info::commodity_t const & c: *names)
        if((c.key.empty() ? info::commodity_key(c.name) : c.key) == change.key)
          {
          change.commodity = c.name;
          break;
          }
  auto known{sqlite::select_from<info::carrier_cargo_t>(
    db_->db,
    sql_iface::tables::carrier_cargo,
    std::format(
      " WHERE carrier_id={} AND key='{}' LIMIT 1", change.carrier_id, sqlite::escape_sql_quotes(change.key)
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier_cargo_change, change)}; not res)
    [[unlikely]]
    return res;
  if(known->empty())
    return sqlite::insert_into(
      db_->db,
      "oid"sv,
      sql_iface::tables::carrier_cargo,
      info::carrier_cargo_t{
        .carrier_id = change.carrier_id, .key = change.key, .commodity = change.commodity,
        .count = std::max<int64_t>(change.delta, 0)
      }
    );
  info::carrier_cargo_t row{known->front()};
  // taken off more than was known to be there: the count was wrong - it stops at nothing
  row.count = std::max<int64_t>(row.count + change.delta, 0);
  if(not change.commodity.empty())
    row.commodity = change.commodity;
  return sqlite::update_pk(db_->db, "oid"sv, sql_iface::tables::carrier_cargo, row, row.oid);
  }

auto database_storage_t::set_carrier_cargo(
  uint64_t carrier_id, std::string_view given_key, std::string_view commodity, int64_t count, std::chrono::sys_seconds when
) -> expected_ec<void>
  {
  std::string const key{given_key};
  if(key.empty())
    return {};
  int64_t now_count{};
  if(auto known{sqlite::select_from<info::carrier_cargo_t>(
       db_->db,
       sql_iface::tables::carrier_cargo,
       std::format(" WHERE carrier_id={} AND key='{}' LIMIT 1", carrier_id, sqlite::escape_sql_quotes(key))
     )};
     known and not known->empty())
    now_count = known->front().count;
  return change_carrier_cargo(
    info::carrier_cargo_change_t{
      .timestamp = when, .carrier_id = carrier_id, .key = key, .commodity = std::string{commodity},
      .delta = std::max<int64_t>(count, 0) - now_count, .source = "edit"
    }
  );
  }

auto database_storage_t::load_carrier_states(std::chrono::sys_seconds now, std::chrono::minutes cooldown)
  -> expected_ec<std::vector<info::carrier_state_t>>
  {
  auto moves{sqlite::select_from<info::carrier_movement_t>(
    db_->db, sql_iface::tables::carrier_movement, " ORDER BY carrier_id, timestamp"
  )};
  if(not moves) [[unlikely]]
    return cxx23::unexpected{moves.error()};

  // a jump takes about a minute - the position comes that long after the departure
  constexpr std::chrono::minutes jump_takes{1};

  struct order_t
    {
    info::carrier_movement_t request;
    std::string from;
    ///\brief the position read after the departure - the arrival - once there is one
    std::optional<std::chrono::sys_seconds> arrived;
    };
  std::map<uint64_t, info::carrier_state_t> states;
  std::map<uint64_t, std::optional<order_t>> orders;
  for(info::carrier_movement_t const & move: *moves)
    {
    info::carrier_state_t & state{states[move.carrier_id]};
    state.carrier_id = move.carrier_id;
    if(not move.carrier_type.empty())
      state.carrier_type = move.carrier_type;
    auto & order{orders[move.carrier_id]};
    if(move.kind == "request")
      order = order_t{.request = move, .from = state.system, .arrived = std::nullopt};
    else if(move.kind == "cancel")
      order.reset();
    else
      {
      // a position read after the departure is the arrival - where the carrier went, and when
      if(order and move.timestamp >= order->request.departure and not order->arrived)
        order->arrived = move.timestamp;
      state.system = move.system;
      state.since = move.timestamp;
      }
    }

  std::vector<info::carrier_state_t> result;
  for(auto & [id, state]: states)
    {
    if(auto const & order{orders[id]}; order)
      {
      auto const arrival{order->arrived.value_or(order->request.departure + jump_takes)};
      if(now < arrival + cooldown)
        {
        state.jumping = true;
        state.from = order->from;
        state.to = order->request.system;
        state.to_body = order->request.body;
        state.departure = order->request.departure;
        state.arrival = arrival;
        state.arrived = order->arrived.has_value();
        }
      else if(not order->arrived)
        {
        // not in the game at the arrival, so no position came - it went where it was sent
        state.system = order->request.system;
        state.since = arrival;
        }
      }
    // the name and callsign come from the carrier's own statistics, when we have seen them
    if(auto row{sqlite::select_from<info::carrier_t>(
         db_->db, sql_iface::tables::carrier, std::format(" WHERE market_id={} LIMIT 1", id)
       )};
       row and not row->empty())
      {
      state.name = row->front().carrier_name;
      state.callsign = row->front().carrier_id;
      }
    result.push_back(std::move(state));
    }
  return result;
  }

auto database_storage_t::load_carrier(std::string_view carrier_id) -> expected_ec<std::optional<info::carrier_t>>
  {
  auto res{sqlite::select_from<info::carrier_t>(
    db_->db, sql_iface::tables::carrier, std::format(" WHERE carrier_id='{}'", sqlite::escape_sql_quotes(carrier_id))
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::carrier_t>{};

  return std::optional<info::carrier_t>{std::move((*res)[0])};
  }

auto database_storage_t::store(info::micro_resource_t const & value) -> expected_ec<void>
  {
  auto known{sqlite::select_from<info::micro_resource_t>(
    db_->db,
    sql_iface::tables::micro_resource,
    std::format(" WHERE name='{}'", sqlite::escape_sql_quotes(value.name))
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(known->empty())
    return sqlite::insert_into<info::micro_resource_t, true>(
      db_->db, "name"sv, sql_iface::tables::micro_resource, value
    );

  // each source knows a different part - the id and the readable name from the bartender, the category from a sale
  info::micro_resource_t merged{std::move((*known)[0])};
  bool changed{};
  if(merged.id == 0 and value.id != 0)
    {
    merged.id = value.id;
    changed = true;
    }
  if(merged.localised.empty() and not value.localised.empty())
    {
    merged.localised = value.localised;
    changed = true;
    }
  if(merged.category.empty() and not value.category.empty())
    {
    merged.category = value.category;
    changed = true;
    }

  if(not changed)
    return {};

  std::string query{std::format(
    "UPDATE {} SET id={}, localised='{}', category='{}' WHERE name='{}'",
    sql_iface::tables::micro_resource,
    merged.id,
    sqlite::escape_sql_quotes(merged.localised),
    sqlite::escape_sql_quotes(merged.category),
    sqlite::escape_sql_quotes(merged.name)
  )};
  return sqlite::execute_query_no_result(db_->db, query);
  }

auto database_storage_t::store(info::consumable_use_t const & value) -> expected_ec<void>
  {
  // replaying the journal repeats the uses; time and the place in that second tell them apart
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND seq={}",
      sql_iface::tables::consumable_use,
      value.timestamp,
      value.seq
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(*known and **known != 0)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::consumable_use, value);
  }

auto database_storage_t::store(info::foot_kill_t const & value) -> expected_ec<void>
  {
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND seq={}",
      sql_iface::tables::foot_kill,
      value.timestamp,
      value.seq
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(*known and **known != 0)
    return {};
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::foot_kill, value);
  }

auto database_storage_t::load_bartender_summary(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::bartender_summary_t>>
  {
  return sqlite::select_from<info::bartender_summary_t>(
    db_->db,
    std::format(
      "(SELECT i.name AS name, coalesce(r.localised, '') AS localised, coalesce(r.category, '') AS category,"
      " sum(CASE WHEN s.kind = 'sold' THEN i.count ELSE 0 END) AS sold,"
      " sum(CASE WHEN s.kind = 'bought' THEN i.count ELSE 0 END) AS bought,"
      " sum(CASE WHEN s.kind = 'bartered' AND i.received = 0 THEN i.count ELSE 0 END) AS bartered_away,"
      " sum(CASE WHEN s.kind = 'bartered' AND i.received != 0 THEN i.count ELSE 0 END) AS bartered_for"
      " FROM {0} i JOIN {1} s ON s.oid = i.sale_oid LEFT JOIN {2} r ON r.name = i.name"
      " WHERE s.timestamp >= '{3:%Y-%m-%dT%H:%M:%SZ}' GROUP BY i.name)",
      sql_iface::tables::micro_sale_item,
      sql_iface::tables::micro_sale,
      sql_iface::tables::micro_resource,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_bartender_totals(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::bartender_total_t>>
  {
  return sqlite::select_from<info::bartender_total_t>(
    db_->db,
    std::format(
      "(SELECT kind, count(*) AS transactions, sum(price) AS credits FROM {} WHERE timestamp >= "
      "'{:%Y-%m-%dT%H:%M:%SZ}' GROUP BY kind)",
      sql_iface::tables::micro_sale,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_consumable_summary(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::consumable_summary_t>>
  {
  return sqlite::select_from<info::consumable_summary_t>(
    db_->db,
    std::format(
      "(SELECT u.name AS name, coalesce(r.localised, '') AS localised, sum(u.count) AS used"
      " FROM {0} u LEFT JOIN {1} r ON r.name = u.name WHERE u.timestamp >= '{2:%Y-%m-%dT%H:%M:%SZ}'"
      " GROUP BY u.name ORDER BY used DESC)",
      sql_iface::tables::consumable_use,
      sql_iface::tables::micro_resource,
      since
    ),
    ""
  );
  }

auto database_storage_t::load_foot_kills(std::chrono::sys_seconds since)
  -> expected_ec<std::vector<info::foot_kill_summary_t>>
  {
  return sqlite::select_from<info::foot_kill_summary_t>(
    db_->db,
    std::format(
      "(SELECT kind, grenade, weapon, count(*) AS kills FROM {} WHERE timestamp >= '{:%Y-%m-%dT%H:%M:%SZ}'"
      " GROUP BY kind, grenade, weapon ORDER BY kills DESC)",
      sql_iface::tables::foot_kill,
      since
    ),
    ""
  );
  }

auto database_storage_t::store(info::micro_sale_t const & sale, std::span<info::micro_sale_item_t const> items)
  -> expected_ec<void>
  {
  // replaying the journal repeats the same transactions; time, market and kind tell them apart
  std::string known_query{std::format(
    "SELECT count(*) FROM {} WHERE market_id={} AND timestamp='{:%Y-%m-%dT%H:%M:%SZ}' AND kind='{}'",
    sql_iface::tables::micro_sale,
    sale.market_id,
    sale.timestamp,
    simple_enum::enum_name(sale.kind)
  )};
  auto known{sqlite::select_signle_from<uint64_t>(db_->db, known_query)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::micro_sale, sale)}; not res) [[unlikely]]
    return res;

  auto sale_oid{sqlite::select_signle_from<int64_t>(
    db_->db, std::format("SELECT max(oid) FROM {}", sql_iface::tables::micro_sale)
  )};
  if(not sale_oid or not *sale_oid) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::bad_message));

  for(info::micro_sale_item_t const & item: items)
    {
    info::micro_sale_item_t row{item};
    row.sale_oid = **sale_oid;
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::micro_sale_item, row)}; not res) [[unlikely]]
      return res;
    }

  return {};
  }

auto database_storage_t::carrier_oid( std::string_view name ) -> expected_ec<std::optional<int64_t>>
  {
  std::string query{
    std::format("SELECT oid FROM {} WHERE carrier_id='{}'", sql_iface::tables::carrier, sqlite::escape_sql_quotes(name))
  };
  return sqlite::select_signle_from<uint64_t>(db_->db, query);
  }

[[nodiscard]]
auto database_storage_t::set_carrier_tracked(std::string_view carrier_id, bool tracked) -> expected_ec<void>
  {
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET tracked={} WHERE carrier_id='{}'",
      sql_iface::tables::carrier,
      tracked ? 1 : 0,
      sqlite::escape_sql_quotes(carrier_id)
    )
  );
  }

auto database_storage_t::update_carrier(info::carrier_t const & carrier) -> expected_ec<void>
  {
  if(not db_->db)
    return cxx23::unexpected(std::make_error_code(std::errc::not_connected));
  
  if(carrier.oid != -1)
    return sqlite::update_pk(db_->db, "oid"sv, sql_iface::tables::carrier, carrier, carrier.oid);
  else
  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier, carrier)};
     not res) [[unlikely]]
    return res;
    
  return {};
  }

auto database_storage_t::store(info::fcmaterial_t const & value) -> expected_ec<void>
  {
  std::string query{
    std::format("SELECT count(*) FROM {} WHERE carrier_id={} and material_id={} and timestamp={}",
                sql_iface::tables::carrier_materials, value.carrier_id, value.material_id, value.timestamp)
  };
  auto cntres{sqlite::select_signle_from<uint64_t>(db_->db, query)};
  if(not cntres) [[unlikely]]
    return cxx23::unexpected{cntres.error()};
  auto cnt { *cntres};
  if( *cnt == 0)
    return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::carrier_materials, value);
  return {};
  }
