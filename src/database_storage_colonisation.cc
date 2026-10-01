// database_storage_t: colonisation claims and construction sites
#include "database_storage_impl.h"

auto database_storage_t::store(info::colony_claim_t const & value) -> expected_ec<void>
  {
  auto known{sqlite::select_from<info::colony_claim_t>(
    db_->db, sql_iface::tables::colony_claim, std::format(" WHERE system_address={}", value.system_address)
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  if(known->empty())
    return sqlite::insert_into<info::colony_claim_t, true>(
      db_->db, "system_address"sv, sql_iface::tables::colony_claim, value
    );
  // a release keeps who claimed it and when; a claim again takes the system back
  info::colony_claim_t row{known->front()};
  if(value.released)
    row.released = true;
  else
    row = value;
  return sqlite::update_pk(db_->db, "system_address"sv, sql_iface::tables::colony_claim, row, row.system_address);
  }

auto database_storage_t::store_construction(
  info::construction_depot_t const & depot, std::span<info::construction_need_t const> needs
) -> expected_ec<void>
  {
  auto known{sqlite::select_from<info::construction_depot_t>(
    db_->db, sql_iface::tables::construction_depot, std::format(" WHERE market_id={}", depot.market_id)
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};
  // an older reading - a journal read again - never overwrites a newer one
  if(not known->empty() and depot.updated < known->front().updated)
    return {};
  info::construction_depot_t row{depot};
  // the event does not name the system; the station row, from the docking, does
  if(row.system_address == 0u and not known->empty())
    row.system_address = known->front().system_address;
  auto stored{
    known->empty()
      ? sqlite::insert_into<info::construction_depot_t, true>(
          db_->db, "market_id"sv, sql_iface::tables::construction_depot, row
        )
      : sqlite::update_pk(db_->db, "market_id"sv, sql_iface::tables::construction_depot, row, row.market_id)
  };
  if(not stored) [[unlikely]]
    return stored;

  // the site writes its state every little while, mostly the same - rows are rewritten only on a change
  auto current{sqlite::select_from<info::construction_need_t>(
    db_->db, sql_iface::tables::construction_need, std::format(" WHERE market_id={} ORDER BY key", depot.market_id)
  )};
  if(not current) [[unlikely]]
    return cxx23::unexpected{current.error()};
  std::vector<info::construction_need_t> incoming{needs.begin(), needs.end()};
  std::ranges::sort(incoming, {}, &info::construction_need_t::key);
  bool const same{std::ranges::equal(
    *current,
    incoming,
    [](info::construction_need_t const & a, info::construction_need_t const & b)
    { return a.key == b.key and a.required == b.required and a.provided == b.provided; }
  )};
  if(same)
    return {};

  if(auto res{sqlite::execute_query_no_result(
       db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::construction_need, depot.market_id)
     )};
     not res) [[unlikely]]
    return res;
  for(info::construction_need_t const & need: incoming)
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::construction_need, need)}; not res)
      [[unlikely]]
      return res;
  return {};
  }

auto database_storage_t::store_delivery(info::construction_delivery_t const & value) -> expected_ec<void>
  {
  if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::construction_delivery, value)}; not res)
    [[unlikely]]
    return res;
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "UPDATE {} SET provided = min(required, provided + {}) WHERE market_id={} AND key='{}'",
      sql_iface::tables::construction_need,
      value.amount,
      value.market_id,
      sqlite::escape_sql_quotes(value.key)
    )
  );
  }

auto database_storage_t::mark_construction_abandoned(uint64_t market_id, bool abandoned, std::chrono::sys_seconds when)
  -> expected_ec<void>
  {
  if(not abandoned)
    return sqlite::execute_query_no_result(
      db_->db, std::format("DELETE FROM {} WHERE market_id={}", sql_iface::tables::construction_abandoned, market_id)
    );
  return sqlite::execute_query_no_result(
    db_->db,
    std::format(
      "INSERT OR REPLACE INTO {} (market_id, marked) VALUES ({}, '{:%Y-%m-%dT%H:%M:%SZ}')",
      sql_iface::tables::construction_abandoned,
      market_id,
      when
    )
  );
  }

auto database_storage_t::load_construction_sites(bool with_abandoned)
  -> expected_ec<std::vector<info::construction_site_t>>
  {
  auto depots{sqlite::select_from<info::construction_depot_t>(
    db_->db,
    sql_iface::tables::construction_depot,
    std::format(
      " WHERE complete=0 AND failed=0 AND system_address IN (SELECT system_address FROM {} WHERE released=0)"
      " ORDER BY system_address, market_id",
      sql_iface::tables::colony_claim
    )
  )};
  if(not depots) [[unlikely]]
    return cxx23::unexpected{depots.error()};

  std::vector<info::construction_site_t> sites;
  for(info::construction_depot_t const & depot: *depots)
    {
    auto marked{sqlite::select_signle_from<uint64_t>(
      db_->db,
      std::format(
        "SELECT market_id FROM {} WHERE market_id={}", sql_iface::tables::construction_abandoned, depot.market_id
      )
    )};
    bool const abandoned{marked and *marked};
    if(abandoned and not with_abandoned)
      continue;
    info::construction_site_t site{.depot = depot, .name = {}, .system = {}, .needs = {}, .abandoned = abandoned};
    if(auto station{load_station(depot.market_id)}; station and *station)
      site.name = (*station)->name;
    // the colonisation ship's site goes by a name the game never localised
    if(constexpr std::string_view ship{"$EXT_PANEL_ColonisationShip;"}; site.name.starts_with(ship))
      {
      std::string_view rest{std::string_view{site.name}.substr(ship.size())};
      while(rest.starts_with(' '))
        rest.remove_prefix(1u);
      site.name = std::format("Colonisation Ship: {}", rest);
      }
    // the name alone - the whole system with its bodies is a dozen queries, read here for every site
    if(auto name{sqlite::select_signle_from<std::string>(
         db_->db,
         std::format("SELECT name FROM {} WHERE system_address={}", sql_iface::tables::star_system, depot.system_address)
       )};
       name and *name)
      site.system = **name;
    auto needs{sqlite::select_from<info::construction_need_t>(
      db_->db,
      sql_iface::tables::construction_need,
      std::format(" WHERE market_id={} ORDER BY (required - provided) DESC", depot.market_id)
    )};
    if(needs)
      site.needs = std::move(*needs);
    sites.push_back(std::move(site));
    }
  return sites;
  }
