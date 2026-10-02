// database_storage_t: stations, their markets, port visits and trade options
#include "database_storage_impl.h"

auto database_storage_t::load_trade_options(uint64_t market_id, unsigned limit, bool bring_here, uint64_t in_system)
  -> expected_ec<std::vector<info::trade_option_t>>
  {
  // the "here" market is the one we stand in, "other" is any other one we have ever seen.
  // the direction decides nothing but which side buys and which sells
  std::string_view const buy_side{bring_here ? "other" : "here"};
  std::string_view const sell_side{bring_here ? "here" : "other"};
  // a rate on a single tonne is no rate - below this threshold the hint only litters the screen
  unsigned const minimum_quantity{eht::settings()->trade.minimum_quantity};
  // the game names only the system a route ends in, never the station, so the whole system is the narrowest
  std::string const system_filter{in_system != 0u ? std::format(" AND st.system_address = {}", in_system) : ""};

  return sqlite::select_from<info::trade_option_t>(
    db_->db,
    std::format(
      "(SELECT other.market_id AS market_id,"
      " coalesce(st.name,'') AS station,"
      " coalesce(ss.name,'') AS system,"
      " c.name AS commodity,"
      " {6}.buy_price AS buy_price,"
      " {7}.sell_price AS sell_price,"
      " {6}.stock AS stock,"
      " {7}.demand AS demand"
      " FROM {0} here"
      " JOIN {0} other ON other.commodity_id = here.commodity_id AND other.market_id <> here.market_id"
      " JOIN {1} c ON c.id = here.commodity_id"
      " LEFT JOIN {2} st ON st.market_id = other.market_id"
      " LEFT JOIN {3} ss ON ss.system_address = st.system_address"
      " WHERE here.market_id = {4}"
      "   AND {6}.stock >= {8} AND {6}.buy_price > 0"
      "   AND {7}.demand >= {8} AND {7}.sell_price > {6}.buy_price{9}"
      // the hold has a finite capacity, so what decides the earnings is the margin per tonne, not the percentage
      " ORDER BY ({7}.sell_price - {6}.buy_price) DESC"
      " LIMIT {5})",
      sql_iface::tables::market_item,
      sql_iface::tables::commodity,
      sql_iface::tables::station,
      sql_iface::tables::star_system,
      market_id,
      limit,
      buy_side,
      sell_side,
      minimum_quantity,
      system_filter
    ),
    ""
  );
  }

auto database_storage_t::store(info::station_t const & value) -> expected_ec<void>
  {
  auto known{load_station(value.market_id)};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(not *known)
    return sqlite::insert_into<info::station_t, true>(db_->db, "market_id"sv, sql_iface::tables::station, value);

  // the sources describe a place differently - Docked knows the station type, ApproachSettlement the
  // settlement's economy. An empty field does not erase what we know, a non-empty one overwrites: a
  // finished construction changes the name and the journal is the only source of truth on what it is called now
  info::station_t merged{**known};
  auto const fill = [](std::string & target, std::string const & source)
  {
    if(not source.empty())
      target = source;
  };
  fill(merged.name, value.name);
  fill(merged.station_type, value.station_type);
  fill(merged.economy, value.economy);
  fill(merged.government, value.government);
  fill(merged.controlling_faction, value.controlling_faction);
  if(value.dist_from_star_ls > 0.0)
    merged.dist_from_star_ls = value.dist_from_star_ls;
  if(value.body_id)
    merged.body_id = value.body_id;
  if(value.latitude)
    merged.latitude = value.latitude;
  if(value.longitude)
    merged.longitude = value.longitude;
  if(merged.system_address == 0)
    merged.system_address = value.system_address;

  return sqlite::update_pk(db_->db, "market_id"sv, sql_iface::tables::station, merged, merged.market_id);
  }

auto database_storage_t::store(info::port_visit_t const & value) -> expected_ec<void>
  {
  // One row per port, with the marker moved at every further stop - and stops do repeat, so a plain
  // INSERT would fail on the key and leave the date of the first visit behind
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT INTO {0} (market_id, name, system, station_type, visited)"
      " VALUES ({1}, '{2}', '{3}', '{4}', '{5:%Y-%m-%dT%H:%M:%SZ}')"
      " ON CONFLICT(market_id) DO UPDATE SET name = excluded.name, system = excluded.system,"
      " station_type = excluded.station_type, visited = excluded.visited",
      sql_iface::tables::port_visit,
      value.market_id,
      sqlite::escape_sql_quotes(value.name),
      sqlite::escape_sql_quotes(value.system),
      sqlite::escape_sql_quotes(value.station_type),
      value.visited
    )
  );
  }

auto database_storage_t::load_last_port() -> expected_ec<std::optional<info::port_visit_t>>
  {
  auto res{sqlite::select_from<info::port_visit_t>(
    db_->db, sql_iface::tables::port_visit, " ORDER BY visited DESC LIMIT 100"
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  // visits stored before construction sites were known not to be safe ports are still here - so the rule
  // is applied once more on the way out
  for(info::port_visit_t & visit: *res)
    if(info::is_escape_pod_port(visit.station_type, visit.name))
      return std::optional<info::port_visit_t>{std::move(visit)};
  return std::optional<info::port_visit_t>{};
  }

auto database_storage_t::load_station(uint64_t market_id) -> expected_ec<std::optional<info::station_t>>
  {
  auto res{sqlite::select_from<info::station_t>(
    db_->db, sql_iface::tables::station, std::format(" WHERE market_id={}", market_id)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::station_t>{};

  if(auto overruled{overrule_retreated_owner(res->front())}; not overruled) [[unlikely]]
    return cxx23::unexpected{overruled.error()};
  return std::optional<info::station_t>{std::move((*res)[0])};
  }

auto database_storage_t::load_commodity_categories() -> expected_ec<std::map<std::string, std::string>>
  {
  auto rows{sqlite::select_from<info::commodity_t>(db_->db, sql_iface::tables::commodity, "")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::map<std::string, std::string> categories;
  for(info::commodity_t const & c: *rows)
    {
    // the dictionary writes "Consumer items" - every word capitalised reads as the game shows it
    std::string category{c.category};
    for(size_t i{}; i != category.size(); ++i)
      if((i == 0u or category[i - 1u] == ' ') and category[i] >= 'a' and category[i] <= 'z')
        category[i] = char(category[i] - 'a' + 'A');
    categories[c.key.empty() ? info::commodity_key(c.name) : c.key] = std::move(category);
    }
  return categories;
  }

auto database_storage_t::load_commodity_names() -> expected_ec<std::vector<std::pair<std::string, std::string>>>
  {
  auto rows{sqlite::select_from<info::commodity_t>(db_->db, sql_iface::tables::commodity, " ORDER BY name")};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  std::vector<std::pair<std::string, std::string>> names;
  names.reserve(rows->size());
  for(info::commodity_t & c: *rows)
    names.emplace_back(c.key.empty() ? info::commodity_key(c.name) : std::move(c.key), std::move(c.name));
  return names;
  }

auto database_storage_t::load_station(uint64_t system_address, std::string_view name)
  -> expected_ec<std::optional<info::station_t>>
  {
  auto res{sqlite::select_from<info::station_t>(
    db_->db,
    sql_iface::tables::station,
    std::format(" WHERE system_address={} AND name='{}'", system_address, sqlite::escape_sql_quotes(name))
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::station_t>{};

  if(auto overruled{overrule_retreated_owner(res->front())}; not overruled) [[unlikely]]
    return cxx23::unexpected{overruled.error()};
  return std::optional<info::station_t>{std::move((*res)[0])};
  }

auto database_storage_t::load_stations(uint64_t system_address) -> expected_ec<std::vector<info::station_t>>
  {
  auto res{sqlite::select_from<info::station_t>(
    db_->db, sql_iface::tables::station, std::format(" WHERE system_address={} ORDER BY name", system_address)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  // the stations of one system share a handful of owners, so each is asked about once
  std::map<std::string, std::string> owners;
  for(info::station_t & station: *res)
    {
    auto const [it, fresh]{owners.try_emplace(station.controlling_faction)};
    if(fresh)
      {
      info::station_t probe{.system_address = station.system_address, .controlling_faction = station.controlling_faction};
      if(auto overruled{overrule_retreated_owner(probe)}; not overruled) [[unlikely]]
        return cxx23::unexpected{overruled.error()};
      it->second = std::move(probe.controlling_faction);
      }
    station.controlling_faction = it->second;
    }
  return res;
  }

auto database_storage_t::load_market_entries(uint64_t market_id) -> expected_ec<std::vector<info::market_entry_t>>
  {
  // the field names of market_entry_t coincide with the columns of both tables, so the join goes
  // through the same query generator as an ordinary read
  return sqlite::select_from<info::market_entry_t>(
    db_->db,
    std::format("{} JOIN {} ON id = commodity_id", sql_iface::tables::market_item, sql_iface::tables::commodity),
    std::format(" WHERE market_id={} ORDER BY category, name", market_id)
  );
  }

auto database_storage_t::replace_market(
  uint64_t market_id,
  std::chrono::sys_seconds updated,
  std::span<info::commodity_t const> commodities,
  std::span<info::market_item_t const> items,
  std::string_view controlling_faction
) -> expected_ec<void>
  {
  // the commodity dictionary is shared by every market; only the unknown ones are added
  for(info::commodity_t const & value: commodities)
    {
    auto known{sqlite::select_signle_from<uint64_t>(
      db_->db, std::format("SELECT count(*) FROM {} WHERE id={}", sql_iface::tables::commodity, value.id)
    )};
    if(not known) [[unlikely]]
      return cxx23::unexpected{known.error()};

    if(*known and **known != 0)
      {
      // a row from before the internal name was kept learns it now
      if(not value.key.empty())
        if(
          auto res{sqlite::execute_query_no_result(
            db_->db,
            std::format(
              "UPDATE {} SET key='{}' WHERE id={} AND key=''",
              sql_iface::tables::commodity,
              sqlite::escape_sql_quotes(value.key),
              value.id
            )
          )};
          not res
        ) [[unlikely]]
          return res;
      continue;
      }

    if(auto res{sqlite::insert_into<info::commodity_t, true>(db_->db, "id"sv, sql_iface::tables::commodity, value)};
       not res) [[unlikely]]
      return res;
    }

  // a market changes all the time, we keep the last reading alone
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::market_item, market_id)
    )};
    not res
  ) [[unlikely]]
    return res;

  for(info::market_item_t const & item: items)
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::market_item, item)}; not res) [[unlikely]]
      return res;

  // the time of the reading is kept with the market, not with the station - a station is rebuildable, a reading is not
  if(
    auto res{sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::market, market_id)
    )};
    not res
  ) [[unlikely]]
    return res;

  return sqlite::insert_into<info::market_info_t, true>(
    db_->db,
    "market_id"sv,
    sql_iface::tables::market,
    info::market_info_t{
      .market_id = market_id, .updated = updated, .controlling_faction = std::string{controlling_faction}
    }
  );
  }

auto database_storage_t::load_market_info(uint64_t market_id) -> expected_ec<std::optional<info::market_info_t>>
  {
  auto res{sqlite::select_from<info::market_info_t>(
    db_->db, sql_iface::tables::market, std::format(" WHERE market_id={}", market_id)
  )};
  if(not res) [[unlikely]]
    return cxx23::unexpected{res.error()};

  if(res->empty())
    return std::optional<info::market_info_t>{};

  return std::optional<info::market_info_t>{std::move((*res)[0])};
  }
