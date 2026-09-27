#include <fleet.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <spdlog/spdlog.h>

namespace fleet
  {
namespace
  {
  ///\brief the journal writes the same type as "Type8" in one place and "type8" in another
  [[nodiscard]]
  auto lower(std::string_view text) -> std::string
    {
    std::string result{text};
    std::ranges::transform(
      result, result.begin(), [](char c) { return char(std::tolower(static_cast<unsigned char>(c))); }
    );
    return result;
    }

  [[nodiscard]]
  auto find(std::vector<info::ship_t> & ships, uint64_t ship_id) -> info::ship_t *
    {
    auto it{std::ranges::find(ships, ship_id, &info::ship_t::ship_id)};
    return it != ships.end() ? &*it : nullptr;
    }

  ///\brief the ship by its id, added when not known yet
  auto obtain(std::vector<info::ship_t> & ships, uint64_t ship_id) -> info::ship_t &
    {
    if(info::ship_t * ship{find(ships, ship_id)}; ship != nullptr)
      return *ship;
    return ships.emplace_back(info::ship_t{.ship_id = ship_id});
    }

  ///\brief the type, and the shown name only when it came with it or the type changed under the id
  auto set_type(info::ship_t & ship, std::string_view type, std::string_view localised) -> void
    {
    if(type.empty())
      return;
    std::string const internal{lower(type)};
    // a name the game gave is kept over the internal one, and "SideWinder" over "sidewinder"
    if(not localised.empty())
      ship.type_name = std::string{localised};
    else if(ship.ship_type != internal or ship.type_name.empty() or ship.type_name == internal)
      ship.type_name = std::string{type};
    ship.ship_type = internal;
    if(not ship.type_name.empty())
      ship.type_name.front() = char(std::toupper(static_cast<unsigned char>(ship.type_name.front())));
    }

  auto put(info::ship_t & ship, std::chrono::sys_seconds when, here_t const & here, uint64_t market_id) -> void
    {
    ship.system = here.system;
    ship.station = here.station;
    ship.market_id = market_id;
    ship.in_transit = false;
    ship.arrives = {};
    ship.seen = when;
    }

  auto fly(std::vector<info::ship_t> & ships, uint64_t ship_id) -> info::ship_t &
    {
    for(info::ship_t & other: ships)
      other.current = false;
    info::ship_t & ship{obtain(ships, ship_id)};
    ship.current = true;
    ship.in_transit = false;
    return ship;
    }

  // the game names the ship given up at a shipyard as stored there, even one an escape pod left on a carrier
  // far away - only the one the commander came in, or one never placed, really stands here
  auto stays_with_commander(info::ship_t const & ship) noexcept -> bool
    { return ship.current or ship.system.empty(); }

  auto remove(std::vector<info::ship_t> & ships, uint64_t ship_id) -> void
    {
    std::erase_if(ships, [ship_id](info::ship_t const & ship) { return ship.ship_id == ship_id; });
    }
  }  // namespace

auto apply(std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::stored_ships_t const & event)
  -> void
  {
  std::vector<info::ship_t> known{std::move(ships)};
  ships.clear();

  // the ship flown is not on the list - it is the one the commander sits in
  for(info::ship_t & ship: known)
    if(ship.current)
      {
      bool const listed{
        std::ranges::contains(event.ShipsHere, ship.ship_id, &events::stored_ship_here_t::ShipID)
        or std::ranges::contains(event.ShipsRemote, ship.ship_id, &events::stored_ship_remote_t::ShipID)
      };
      if(not listed)
        ships.push_back(ship);
      }

  auto const previous = [&known](uint64_t ship_id) -> info::ship_t
  {
    auto it{std::ranges::find(known, ship_id, &info::ship_t::ship_id)};
    return it != known.end() ? *it : info::ship_t{.ship_id = ship_id};
  };

  for(events::stored_ship_here_t const & stored: event.ShipsHere)
    {
    info::ship_t ship{previous(stored.ShipID)};
    set_type(ship, stored.ShipType, stored.ShipType_Localised);
    ship.name = stored.Name;
    ship.value = stored.Value;
    ship.hot = stored.Hot;
    ship.current = false;
    put(ship, when, here_t{.system = event.StarSystem, .station = event.StationName}, event.MarketID);
    ships.push_back(std::move(ship));
    }

  for(events::stored_ship_remote_t const & stored: event.ShipsRemote)
    {
    info::ship_t ship{previous(stored.ShipID)};
    set_type(ship, stored.ShipType, stored.ShipType_Localised);
    ship.name = stored.Name;
    ship.value = stored.Value;
    ship.hot = stored.Hot;
    ship.current = false;
    if(stored.InTransit)
      {
      // on the way, and the list does not say where to - only the order did, if it was seen
      if(not ship.in_transit)
        {
        ship.system.clear();
        ship.station.clear();
        ship.market_id = 0u;
        ship.arrives = {};
        }
      ship.in_transit = true;
      ship.seen = when;
      }
    else
      {
      // the station is named only for the ships standing here; a remote one keeps the name it had
      // when its market is still the same
      std::string station{ship.market_id == stored.ShipMarketID ? ship.station : std::string{}};
      put(ship, when, here_t{.system = stored.StarSystem, .station = std::move(station)}, stored.ShipMarketID);
      }
    ships.push_back(std::move(ship));
    }

  std::ranges::sort(ships, {}, &info::ship_t::ship_id);
  }

auto apply(
  std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::loadout_t const & event, here_t const & here
) -> void
  {
  info::ship_t & ship{fly(ships, event.ShipID)};
  set_type(ship, event.Ship, {});
  ship.name = event.ShipName;
  ship.ident = event.ShipIdent;
  ship.value = uint64_t{event.HullValue} + uint64_t{event.ModulesValue};
  if(not here.system.empty())
    put(ship, when, here, here.market_id);
  }

auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_swap_t const & event,
  here_t const & here
) -> void
  {
  if(event.SellShipID)
    remove(ships, *event.SellShipID);
  if(event.StoreShipID)
    {
    info::ship_t & left{obtain(ships, *event.StoreShipID)};
    set_type(left, event.StoreOldShip, {});
    if(stays_with_commander(left))
      put(left, when, here, event.MarketID);
    }
  info::ship_t & taken{fly(ships, event.ShipID)};
  set_type(taken, event.ShipType, event.ShipType_Localised);
  put(taken, when, here, event.MarketID);
  }

auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_buy_t const & event,
  here_t const & here
) -> void
  {
  if(event.SellShipID)
    remove(ships, *event.SellShipID);
  if(event.StoreShipID)
    {
    info::ship_t & left{obtain(ships, *event.StoreShipID)};
    if(stays_with_commander(left))
      put(left, when, here, event.MarketID);
    left.current = false;
    }
  }

auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_new_t const & event,
  here_t const & here
) -> void
  {
  info::ship_t & ship{fly(ships, event.NewShipID)};
  set_type(ship, event.ShipType, event.ShipType_Localised);
  put(ship, when, here, here.market_id);
  }

auto apply(
  std::vector<info::ship_t> & ships,
  std::chrono::sys_seconds when,
  events::shipyard_transfer_t const & event,
  here_t const & here
) -> void
  {
  // ordered to where we stand - from now on that is its place, and the order says when it gets there
  info::ship_t & ship{obtain(ships, event.ShipID)};
  set_type(ship, event.ShipType, event.ShipType_Localised);
  put(ship, when, here, event.MarketID);
  // one left behind by an escape pod may still count as flown - but a ship on its way is flown by nobody
  ship.current = false;
  ship.in_transit = true;
  ship.arrives = when + std::chrono::seconds{event.TransferTime};
  }

auto apply(
  std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::resurrect_t const & event, here_t const & here
) -> void
  {
  // the pod leaves the ship at the pad - the game still calls it the active one, but it no longer goes where
  // the commander goes; a rebuy gives the ship back where the commander wakes, and its Loadout says so
  if(event.Option != "escape" or here.market_id == 0u)
    return;
  for(info::ship_t & ship: ships)
    if(ship.current)
      {
      ship.current = false;
      put(ship, when, here, here.market_id);
      }
  }

auto apply(std::vector<info::ship_t> & ships, std::chrono::sys_seconds when, events::undocked_t const & event) -> void
  {
  // a ship counts as flown once it has left a pad with us at the controls - boarding it at a shipyard
  // to take it somewhere is not yet flying it
  if(event.Taxi or event.Multicrew)
    return;
  for(info::ship_t & ship: ships)
    if(ship.current)
      ship.flown = when;
  }

auto apply(std::vector<info::ship_t> & ships, events::shipyard_sell_t const & event) -> void
  { remove(ships, event.SellShipID); }

auto apply(std::vector<info::ship_t> & ships, events::sell_ship_on_rebuy_t const & event) -> void
  { remove(ships, event.SellShipId); }

auto apply(std::vector<info::ship_t> & ships, events::set_user_ship_name_t const & event) -> void
  {
  info::ship_t & ship{obtain(ships, event.ShipID)};
  set_type(ship, event.Ship, {});
  ship.name = event.UserShipName;
  ship.ident = event.UserShipId;
  }

template<typename event_t>
auto record(
  database_storage_t & db,
  std::chrono::sys_seconds when,
  event_t const & event,
  std::string_view system,
  uint64_t market_id
) -> void
  {
  auto ships{db.load_fleet()};
  if(not ships) [[unlikely]]
    {
    spdlog::error("failed to load the fleet");
    return;
    }

  here_t here{.system = std::string{system}, .station = {}, .market_id = market_id};
  if constexpr(requires { event.MarketID; })
    here.market_id = event.MarketID;
  if(here.market_id != 0u)
    if(auto station{db.load_station(here.market_id)}; station and *station)
      here.station = (*station)->name;

  if constexpr(requires { apply(*ships, when, event, here); })
    apply(*ships, when, event, here);
  else if constexpr(requires { apply(*ships, when, event); })
    apply(*ships, when, event);
  else
    apply(*ships, event);

  if(auto res{db.store_fleet(*ships)}; not res) [[unlikely]]
    spdlog::error("failed to store the fleet");
  }

template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::stored_ships_t const &, std::string_view, uint64_t)
    -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::loadout_t const &, std::string_view, uint64_t) -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::shipyard_swap_t const &, std::string_view, uint64_t)
    -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::shipyard_buy_t const &, std::string_view, uint64_t)
    -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::shipyard_new_t const &, std::string_view, uint64_t)
    -> void;
template auto record(
  database_storage_t &, std::chrono::sys_seconds, events::shipyard_transfer_t const &, std::string_view, uint64_t
) -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::shipyard_sell_t const &, std::string_view, uint64_t)
    -> void;
template auto record(
  database_storage_t &, std::chrono::sys_seconds, events::sell_ship_on_rebuy_t const &, std::string_view, uint64_t
) -> void;
template auto record(
  database_storage_t &, std::chrono::sys_seconds, events::set_user_ship_name_t const &, std::string_view, uint64_t
) -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::resurrect_t const &, std::string_view, uint64_t)
    -> void;
template auto
  record(database_storage_t &, std::chrono::sys_seconds, events::undocked_t const &, std::string_view, uint64_t)
    -> void;

auto distance_ly(std::array<double, 3> const & a, std::array<double, 3> const & b) noexcept -> double
  {
  double const dx{a[0] - b[0]};
  double const dy{a[1] - b[1]};
  double const dz{a[2] - b[2]};
  return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

auto place(
  std::span<info::ship_t const> ships,
  std::span<info::carrier_state_t const> carriers,
  std::map<std::string, std::array<double, 3>> const & positions,
  std::string_view here_system,
  std::array<double, 3> const & here_position,
  std::chrono::sys_seconds now
) -> std::vector<placed_ship_t>
  {
  std::vector<placed_ship_t> result;
  result.reserve(ships.size());
  for(info::ship_t const & ship: ships)
    {
    placed_ship_t placed{.ship = ship, .system = ship.system, .station = ship.station, .distance_ly = std::nullopt};
    if(ship.current)
      {
      placed.system = std::string{here_system};
      placed.station.clear();
      placed.distance_ly = 0.0;
      result.push_back(std::move(placed));
      continue;
      }

    placed.travelling = ship.in_transit and (ship.arrives == std::chrono::sys_seconds{} or ship.arrives > now);

    // a carrier jumps with every ship on it - its later position is the ship's
    if(ship.market_id != 0u)
      if(auto it{std::ranges::find(carriers, ship.market_id, &info::carrier_state_t::carrier_id)}; it != carriers.end())
        {
        placed.on_carrier = true;
        if(placed.station.empty())
          placed.station = it->callsign.empty() ? it->name : it->callsign;
        if(not it->system.empty() and it->since >= ship.seen)
          placed.system = it->system;
        }

    if(placed.system == here_system and not here_system.empty())
      placed.distance_ly = 0.0;
    else if(auto pos{positions.find(placed.system)}; pos != positions.end())
      placed.distance_ly = distance_ly(pos->second, here_position);
    result.push_back(std::move(placed));
    }

  std::ranges::stable_sort(
    result,
    [](placed_ship_t const & a, placed_ship_t const & b)
    {
      if(a.ship.current != b.ship.current)
        return a.ship.current;
      if(a.distance_ly.has_value() != b.distance_ly.has_value())
        return a.distance_ly.has_value();
      if(a.distance_ly and *a.distance_ly != *b.distance_ly)
        return *a.distance_ly < *b.distance_ly;
      return a.ship.value > b.ship.value;
    }
  );
  return result;
  }

auto locate(
  database_storage_t & db,
  std::string_view here_system,
  std::array<double, 3> const & here_position,
  std::chrono::sys_seconds now
) -> expected_ec<std::vector<placed_ship_t>>
  {
  auto ships{db.load_fleet()};
  if(not ships) [[unlikely]]
    return cxx23::unexpected{ships.error()};

  // a ship elsewhere comes with its market alone - the port's name is known if we ever docked there
  for(info::ship_t & ship: *ships)
    if(ship.station.empty() and ship.market_id != 0u)
      if(auto station{db.load_station(ship.market_id)}; station and *station)
        ship.station = (*station)->name;

  auto carriers{db.load_carrier_states(now, std::chrono::minutes{0})};
  std::vector<info::carrier_state_t> const no_carriers;
  std::span<info::carrier_state_t const> const known_carriers{carriers ? *carriers : no_carriers};

  std::vector<std::string> names;
  for(info::ship_t const & ship: *ships)
    names.push_back(ship.system);
  for(info::carrier_state_t const & carrier: known_carriers)
    names.push_back(carrier.system);
  std::ranges::sort(names);
  auto const [first, last]{std::ranges::unique(names)};
  names.erase(first, last);
  std::erase(names, std::string{});

  auto positions{db.load_system_positions(names)};
  if(not positions) [[unlikely]]
    return cxx23::unexpected{positions.error()};

  return place(*ships, known_carriers, *positions, here_system, here_position, now);
  }

auto shown_name(info::ship_t const & ship) -> std::string { return ship.name.empty() ? ship.type_name : ship.name; }
  }  // namespace fleet
