// database_storage_t: the ships of the fleet and the neutron route
#include "database_storage_impl.h"

auto database_storage_t::store_neutron_route(std::span<info::neutron_waypoint_t const> route) -> expected_ec<void>
  {
  // one remembered route - loading a new one replaces the previous one entirely
  if(auto res{sqlite::execute_query_no_result(db_->db, std::format("DELETE FROM {}", sql_iface::tables::neutron_route))};
     not res) [[unlikely]]
    return res;

  for(info::neutron_waypoint_t const & waypoint: route)
    if(auto res{sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::neutron_route, waypoint)}; not res)
      [[unlikely]]
      return res;

  return {};
  }

auto database_storage_t::load_neutron_route() -> expected_ec<std::vector<info::neutron_waypoint_t>>
  {
  return sqlite::select_from<info::neutron_waypoint_t>(
    db_->db, sql_iface::tables::neutron_route, " ORDER BY position"
  );
  }

auto database_storage_t::load_neutron_progress(std::string_view fid, std::string_view route_key) -> expected_ec<uint32_t>
  {
  auto rows{sqlite::select_from<info::neutron_progress_t>(
    db_->db,
    sql_iface::tables::neutron_progress,
    std::format(
      " WHERE fid='{}' AND route_key='{}'", sqlite::escape_sql_quotes(fid), sqlite::escape_sql_quotes(route_key)
    )
  )};
  if(not rows) [[unlikely]]
    return cxx23::unexpected{rows.error()};
  return rows->empty() ? 0u : rows->front().reached;
  }

auto database_storage_t::store_neutron_progress(info::neutron_progress_t const & value) -> expected_ec<void>
  {
  // the row of this account and route replaced - the unique key on both keeps it one
  if(auto res{sqlite::execute_query_no_result(
       db_->db,
       std::format(
         "DELETE FROM {} WHERE fid='{}' AND route_key='{}'",
         sql_iface::tables::neutron_progress,
         sqlite::escape_sql_quotes(value.fid),
         sqlite::escape_sql_quotes(value.route_key)
       )
     )};
     not res) [[unlikely]]
    return res;
  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::neutron_progress, value);
  }

auto database_storage_t::store(info::ship_transfer_t const & value) -> expected_ec<void>
  {
  // a rebuild from journals repeats every order - the ship and the moment of ordering tell them apart
  auto known{sqlite::select_signle_from<uint64_t>(
    db_->db,
    std::format(
      "SELECT count(*) FROM {} WHERE ship_id={} AND ordered='{:%Y-%m-%dT%H:%M:%SZ}'",
      sql_iface::tables::ship_transfer,
      value.ship_id,
      value.ordered
    )
  )};
  if(not known) [[unlikely]]
    return cxx23::unexpected{known.error()};

  if(*known and **known != 0)
    return {};

  return sqlite::insert_into(db_->db, "oid"sv, sql_iface::tables::ship_transfer, value);
  }

auto database_storage_t::load_transfers_in_flight(std::chrono::sys_seconds now)
  -> expected_ec<std::vector<info::ship_transfer_t>>
  {
  return sqlite::select_from<info::ship_transfer_t>(
    db_->db,
    sql_iface::tables::ship_transfer,
    std::format(" WHERE arrives > '{:%Y-%m-%dT%H:%M:%SZ}' ORDER BY arrives", now)
  );
  }

auto database_storage_t::load_fleet() -> expected_ec<std::vector<info::ship_t>>
  {
  return sqlite::select_from<info::ship_t>(db_->db, sql_iface::tables::ship, " ORDER BY ship_id");
  }

auto database_storage_t::store_fleet(std::span<info::ship_t const> ships) -> expected_ec<void>
  {
  // a few dozen rows, and every change is worked out on the whole list - so the list replaces the table
  if(auto res{sqlite::execute_query_no_result(db_->db, std::format("DELETE FROM {}", sql_iface::tables::ship))};
     not res) [[unlikely]]
    return res;

  for(info::ship_t const & ship: ships)
    if(auto res{sqlite::insert_into<info::ship_t, true>(db_->db, "ship_id"sv, sql_iface::tables::ship, ship)}; not res)
      [[unlikely]]
      return res;

  return {};
  }
