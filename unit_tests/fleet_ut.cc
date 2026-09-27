#include <boost/ut.hpp>
#include <fleet.h>

namespace
  {
using namespace std::chrono_literals;

constexpr std::chrono::sys_seconds morning{std::chrono::sys_days{std::chrono::September / 27 / 2026} + 8h};

[[nodiscard]]
auto by_id(std::vector<info::ship_t> const & ships, uint64_t ship_id) -> info::ship_t const *
  {
  auto it{std::ranges::find(ships, ship_id, &info::ship_t::ship_id)};
  return it != ships.end() ? &*it : nullptr;
  }

[[nodiscard]]
auto shipyard() -> ::events::stored_ships_t
  {
  return ::events::stored_ships_t{
    .StationName = "PRNH",
    .MarketID = 3715010304u,
    .StarSystem = "Bleia Eohn PW-D b32-1",
    .ShipsHere = {::events::stored_ship_here_t{
      .ShipID = 26u,
      .ShipType = "lakonminer",
      .ShipType_Localised = "Type-11 Prospector",
      .Name = "T11",
      .Value = 77174133u,
      .Hot = false
    }},
    .ShipsRemote = {
      ::events::stored_ship_remote_t{
        .ShipID = 34u,
        .ShipType = "PantherMkII",
        .ShipType_Localised = "Panther Clipper Mk II",
        .Name = "pc2",
        .StarSystem = "Bleia Eohn LQ-F b31-7",
        .ShipMarketID = 4379304195u,
        .TransferPrice = 541810u,
        .TransferTime = 447u,
        .Value = 447935501u,
        .Hot = false,
        .InTransit = false
      },
      ::events::stored_ship_remote_t{
        .ShipID = 40u,
        .ShipType = "SmallCombat01_NX",
        .ShipType_Localised = "Kestrel Mk II",
        .Name = "",
        .StarSystem = "",
        .ShipMarketID = 0u,
        .TransferPrice = 0u,
        .TransferTime = 0u,
        .Value = 56560989u,
        .Hot = false,
        .InTransit = true
      }
    }
  };
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  "the shipyard's list replaces the fleet but keeps the ship flown"_test = []
  {
    std::vector<info::ship_t> ships{
      info::ship_t{.ship_id = 54u, .ship_type = "explorer_nx", .type_name = "Caspian Explorer", .current = true},
      // sold somewhere unseen - not on the list any more
      info::ship_t{.ship_id = 62u, .ship_type = "sidewinder", .type_name = "sidewinder"}
    };
    fleet::apply(ships, morning, shipyard());

    expect(ships.size() == 4u);
    expect(by_id(ships, 62u) == nullptr);
    expect(by_id(ships, 54u) != nullptr and by_id(ships, 54u)->current);

    info::ship_t const * here{by_id(ships, 26u)};
    expect(here != nullptr and here->station == std::string{"PRNH"} and here->market_id == 3715010304u);
    expect(here != nullptr and here->system == std::string{"Bleia Eohn PW-D b32-1"});
    info::ship_t const * remote{by_id(ships, 34u)};
    expect(remote != nullptr and remote->ship_type == std::string{"panthermkii"});
    expect(remote != nullptr and remote->type_name == std::string{"Panther Clipper Mk II"});
    expect(remote != nullptr and remote->system == std::string{"Bleia Eohn LQ-F b31-7"} and not remote->in_transit);
  };

  "a transfer seen ordered keeps its destination while the list says only in transit"_test = []
  {
    std::vector<info::ship_t> ships;
    fleet::apply(
      ships,
      morning,
      ::events::shipyard_transfer_t{
        .ShipType = "SmallCombat01_NX",
        .ShipType_Localised = "Kestrel Mk II",
        .ShipID = 40u,
        .System = "Bleia Eohn BD-I a64-1",
        .ShipMarketID = 1u,
        .Distance = 14.9,
        .TransferPrice = 1000u,
        .TransferTime = 449u,
        .MarketID = 4379304195u
      },
      fleet::here_t{.system = "Bleia Eohn LQ-F b31-7", .station = "Nowhere Base", .market_id = 4379304195u}
    );
    fleet::apply(ships, morning + 1min, shipyard());

    info::ship_t const * ship{by_id(ships, 40u)};
    expect(ship != nullptr and ship->in_transit);
    expect(ship != nullptr and ship->system == std::string{"Bleia Eohn LQ-F b31-7"});
    expect(ship != nullptr and ship->arrives == morning + 449s);
  };

  "a swap leaves one ship here and takes the other"_test = []
  {
    std::vector<info::ship_t> ships{info::ship_t{.ship_id = 2u, .ship_type = "viper_mkiv", .current = true}};
    fleet::apply(
      ships,
      morning,
      ::events::shipyard_swap_t{
        .ShipType = "smallcombat01_nx",
        .ShipType_Localised = "Kestrel Mk II",
        .ShipID = 40u,
        .StoreOldShip = "Viper_MkIV",
        .StoreShipID = 2u,
        .SellShipID = std::nullopt,
        .MarketID = 4379304195u
      },
      fleet::here_t{.system = "Bleia Eohn LQ-F b31-7", .station = "Carrier", .market_id = 0u}
    );
    expect(ships.size() == 2u);
    expect(not by_id(ships, 2u)->current and by_id(ships, 2u)->market_id == 4379304195u);
    expect(by_id(ships, 40u)->current);
  };

  "an escape pod leaves the ship on the carrier, and a swap far away does not move it"_test = []
  {
    std::vector<info::ship_t> ships{info::ship_t{.ship_id = 54u, .ship_type = "explorer_nx", .current = true}};
    fleet::apply(
      ships,
      morning,
      ::events::resurrect_t{.Option = "escape"},
      fleet::here_t{.system = "Kusauts", .station = "W1V-NXM", .market_id = 3706381824u}
    );
    expect(not by_id(ships, 54u)->current and by_id(ships, 54u)->market_id == 3706381824u);

    // the game writes the ship left on the carrier as stored at the port the pod brought us to
    fleet::apply(
      ships,
      morning + 2min,
      ::events::shipyard_swap_t{
        .ShipType = "sidewinder",
        .ShipType_Localised = {},
        .ShipID = 68u,
        .StoreOldShip = "Explorer_NX",
        .StoreShipID = 54u,
        .SellShipID = std::nullopt,
        .MarketID = 4391602179u
      },
      fleet::here_t{.system = "Bleia Eohn PW-D b32-1", .station = "Arkush City", .market_id = 4391602179u}
    );
    expect(by_id(ships, 54u)->system == std::string{"Kusauts"} and by_id(ships, 54u)->market_id == 3706381824u);
    expect(by_id(ships, 68u)->current and by_id(ships, 68u)->market_id == 4391602179u);
  };

  "a rebuy leaves the ship flown as it was"_test = []
  {
    std::vector<info::ship_t> ships{info::ship_t{.ship_id = 54u, .ship_type = "explorer_nx", .current = true}};
    fleet::apply(
      ships,
      morning,
      ::events::resurrect_t{.Option = "rebuy"},
      fleet::here_t{.system = "Kusauts", .station = "W1V-NXM", .market_id = 3706381824u}
    );
    expect(by_id(ships, 54u)->current and by_id(ships, 54u)->market_id == 0u);
  };

  "a purchase stores the old ship, the new one is flown, a sale removes one"_test = []
  {
    std::vector<info::ship_t> ships{info::ship_t{.ship_id = 54u, .ship_type = "explorer_nx", .current = true}};
    fleet::here_t const here{.system = "Kusauts", .station = "Somewhere", .market_id = 7u};
    fleet::apply(
      ships,
      morning,
      ::events::shipyard_buy_t{
        .ShipType = "sidewinder", .ShipType_Localised = {}, .StoreShipID = 54u, .SellShipID = {}, .MarketID = 7u
      },
      here
    );
    fleet::apply(
      ships,
      morning,
      ::events::shipyard_new_t{.ShipType = "sidewinder", .ShipType_Localised = {}, .NewShipID = 66u},
      here
    );
    expect(ships.size() == 2u);
    expect(by_id(ships, 66u)->current and not by_id(ships, 54u)->current);
    expect(by_id(ships, 54u)->system == std::string{"Kusauts"});
    expect(by_id(ships, 66u)->type_name == std::string{"Sidewinder"});

    fleet::apply(
      ships,
      ::events::set_user_ship_name_t{
        .Ship = "sidewinder", .ShipID = 66u, .UserShipName = "pudelko", .UserShipId = "SJ-01"
      }
    );
    expect(fleet::shown_name(*by_id(ships, 66u)) == std::string{"pudelko"});

    fleet::apply(ships, ::events::shipyard_sell_t{.ShipType = "explorer_nx", .SellShipID = 54u, .MarketID = 7u});
    expect(ships.size() == 1u);
  };

  "ships are placed by distance, one on a carrier follows the carrier"_test = []
  {
    std::vector<info::ship_t> const ships{
      info::ship_t{.ship_id = 1u, .type_name = "far", .system = "Colonia", .seen = morning},
      info::ship_t{.ship_id = 2u, .type_name = "carried", .system = "Kusauts", .market_id = 99u, .seen = morning},
      info::ship_t{.ship_id = 3u, .type_name = "flown", .current = true},
      info::ship_t{.ship_id = 4u, .type_name = "lost", .system = "Unheard Of", .seen = morning}
    };
    std::vector<info::carrier_state_t> const carriers{info::carrier_state_t{
      .carrier_id = 99u, .callsign = "W1V-NXM", .system = "Bleia Eohn LQ-F b31-7", .since = morning + 1h
    }};
    std::map<std::string, std::array<double, 3>> const positions{
      {"Colonia", {-9530.5, -910.28125, 19808.125}},
      {"Bleia Eohn LQ-F b31-7", {-230.75, -53.90625, 2168.5625}},
      {"Kusauts", {-18.3125, -9.71875, 121.84375}}
    };
    std::array<double, 3> const here{-232.3125, -51.34375, 2183.03125};
    auto const placed{fleet::place(ships, carriers, positions, "Bleia Eohn PW-D b32-1", here, morning + 2h)};

    expect(placed.size() == 4u);
    expect(placed[0].ship.ship_id == 3u and placed[0].distance_ly == 0.0);
    expect(placed[1].ship.ship_id == 2u and placed[1].on_carrier);
    expect(placed[1].system == std::string{"Bleia Eohn LQ-F b31-7"} and placed[1].station == std::string{"W1V-NXM"});
    expect(placed[1].distance_ly.has_value() and *placed[1].distance_ly < 15.0);
    expect(placed[2].ship.ship_id == 1u);
    expect(placed[3].ship.ship_id == 4u and not placed[3].distance_ly.has_value());
  };
  }
