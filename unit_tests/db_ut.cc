#include <databse_storage.h>
#include <biology.h>
#include <territory.h>
#include <data/bgs.h>
#include <data/carrier.h>
#include <data/micro_resources.h>
#include <data/station.h>
#include <star_system.h>
#include <json_glaze.h>
#include <print>
#include <filesystem>
#include <boost/ut.hpp>
#include <spdlog/spdlog.h>
namespace ut = boost::ut;
// struct foo_t
//   {
//   std::string a;
//   int b;
//   double c;
//   };
namespace fs = std::filesystem;

int main()
  {
  //   glz::reflect<foo_t> def;
  //
  //   glz::for_each_field(foo_t{}, []<typename T>(T & field)
  //     {
  //       std::println("{}", sqlite::reflection_type_name<T>());
  //     });
  // std::string buffer;
  // buffer.resize(64);
  // glz::context ctx{};
  // std::string value{"AAA"};

  // auto const parse_res = glz::write<glz::opts{.error_on_unknown_keys = false}>(value, buffer);

  // glz::to<glz::JSON, int>::template op<glz::opts{}>(43,  ctx, buffer, 0);
  spdlog::set_level(spdlog::level::debug);
  for(char const * file: {"elite.sqlite", "galaxy.sqlite", "live.sqlite"})
    if(fs::exists(file))
      fs::remove(file);
  database_storage_t dbs{"elite.sqlite"};

  star_system_t system {
    .system_address= 3384199352978,
    .name = "Oochosy LW-C d14",
    .star_type = "K",
    .bary_centre = {
      bary_centre_t{
        .body_id = 1,
        .semi_major_axis = 0.15,
        .eccentricity = 0.31,
        .orbital_inclination = 5.12,
        .periapsis = 890123.,
        .orbital_period = 12356.,
        .ascending_node = 15,
        .mean_anomaly = 0.11
      }
    },
    .bodies = {
      //{ "BodyName":"Col 173 Sector JP-L c9-4", "BodyID":0, "StarSystem":"Col 173 Sector JP-L c9-4",
      // "SystemAddress":1184974770842, "DistanceFromArrivalLS":0.000000, "StarType":"K", 
      // "Subclass":9, "StellarMass":0.597656, "Radius":480193792.000000, "AbsoluteMagnitude":7.442474,
      // "Age_MY":1938, "SurfaceTemperature":3811.000000, "Luminosity":"Va", "RotationPeriod":322650.250188,
      //"AxialTilt":0.000000, "Rings":[ { "Name":"Col 173 Sector JP-L c9-4 A Belt", "RingClass":"eRingClass_Rocky",
      //"MassMT":1.0175e+14, "InnerRad":7.3454e+08, "OuterRad":2.0326e+09 } ], "WasDiscovered":true, "WasMapped":false,
      //"WasFootfalled":false }
      body_t{
        .value = 1000000,
        .body_id= 7,
        .name = "1",
        
        .details = planet_details_t{
          .parent_star= 1,
          .parent_barycenter = 0,
          .terraform_state = events::terraform_state_e::Terraformable,
          .planet_class = "High metal content body",
          .atmosphere = "thick argon rich atmosphere",
          .atmosphere_type = "ArgonRich",
          .genuses_ = {events::genus_t{.Genus_Localised = "Bacterium", .Species_Localised = "", .Sampled = false}},
          .volcanism = "",
          .mass_em = 0.006929,
          .surface_gravity = 1.763689,
          .surface_temperature = 956.597717,
          .surface_pressure = 0.000000,
        },
        .orbital_period = 341110.241413
      }
/*
{
  "BodyName": "Col 173 Sector IJ-N c8-12 A 1",
  "Parents": [
    {
      "Star": 1
    },
    {
      "Null": 0
    }
  ],
  "DistanceFromArrivalLS": 23.417667,
  "TidalLock": true,
  "Volcanism": "",
  "MassEM": ,
  "Radius": 1251308.875000,
  "SurfaceGravity": ,
  "SurfaceTemperature": ,
  "SurfacePressure": ,
  "Landable": true,
  "Materials": [
    {
      "Name": "iron",
      "Percent": 21.732067
    },
    {
      "Name": "nickel",
      "Percent": 16.437223
    },
    {
      "Name": "sulphur",
      "Percent": 15.291922
    },
    {
      "Name": "carbon",
      "Percent": 12.858923
    },
    {
      "Name": "chromium",
      "Percent": 9.773632
    },
    {
      "Name": "manganese",
      "Percent": 8.975122
    },
    {
      "Name": "phosphorus",
      "Percent": 8.232503
    },
    {
      "Name": "zirconium",
      "Percent": 2.523541
    },
    {
      "Name": "molybdenum",
      "Percent": 1.419091
    },
    {
      "Name": "tin",
      "Percent": 1.413885
    },
    {
      "Name": "ruthenium",
      "Percent": 1.342094
    }
  ],
  "Composition": {
    "Ice": 0.000000,
    "Rock": 0.669066,
    "Metal": 0.330934
  },
  "SemiMajorAxis": 7026505589.485168,
  "Eccentricity": 0.000985,
  "OrbitalInclination": -0.013204,
  "Periapsis": 338.519857,
  "OrbitalPeriod": ,
  "AscendingNode": 82.022338,
  "MeanAnomaly": 28.719750,
  "RotationPeriod": 341110.368400,
  "AxialTilt": 0.084878,
  "WasDiscovered": true,
  "WasMapped": false,
  "WasFootfalled": false
}
*/
    }
  };
  using namespace std::string_view_literals;
  constexpr uint64_t address{3384199352978};
  constexpr uint32_t body{7};

  auto res{dbs.open()};
  ut::expect(bool(res));
  res = dbs.store(system);
  ut::expect(bool(res));
  auto r2{dbs.load_system(address)};
  ut::expect(bool(r2));
  ut::expect(r2->has_value());
  star_system_t loaded{std::move(**r2)};
  ut::expect(loaded.system_address == address);
  ut::expect(loaded.bodies.size() == 1);
  ut::expect(loaded.bodies[0].name == "1"sv);
  ut::expect(std::get<planet_details_t>(loaded.bodies[0].details).planet_class == "High metal content body"sv);

  using namespace ut;

  "a character's progress comes back from the separate database"_test = [&]
  {
    expect(bool(dbs.store_fss_complete(address)));
    expect(bool(dbs.store_dss_complete(address, body)));
    expect(bool(dbs.store_footfall_complete(address, body)));
    expect(bool(dbs.store_genus_species(address, body, "Bacterium", "Bacterium Aurasus", true, true)));
    // a later Log of the same genus does not take the finished sample back
    expect(bool(dbs.store_genus_species(address, body, "Bacterium", "Bacterium Aurasus", true, false)));
    expect(bool(dbs.update_faction_info(info::faction_info_t{.name = "Crew of Pethes", .reputation = 87.5})));

    auto reloaded{dbs.load_system(address)};
    expect(bool(reloaded)) << "the system did not load";
    expect(reloaded->has_value());
    star_system_t const & got{**reloaded};
    expect(got.fss_complete) << "fss_complete lost";
    expect(std::get<planet_details_t>(got.bodies[0].details).mapped) << "mapped lost";
    expect(std::get<planet_details_t>(got.bodies[0].details).footfalled) << "footfalled lost";

    auto const & genuses{std::get<planet_details_t>(got.bodies[0].details).genuses_};
    expect(genuses.size() == 1u);
    expect(genuses[0].Sampled) << "the sample lost";
    expect(genuses[0].Species_Localised == "Bacterium Aurasus"sv) << "the species lost";

    // the codex lists what this commander logged; a species learnt from another account's journals
    // stays in the shared history, not in the codex
    expect(bool(dbs.store_genus_species(address, body, "Stratum", "Stratum Tectonicas", false, true)));
    auto finds{dbs.load_codex_finds()};
    expect(bool(finds));
    expect(finds->size() == 1u) << "only the commander's own find belongs in the codex";
    expect(finds->front().species == "Bacterium Aurasus"sv);
    expect(finds->front().sampled);
    auto history{dbs.load_species_history()};
    expect(bool(history));
    expect(history->size() == 2u) << "the history knows both";

    auto faction{dbs.load_faction("Crew of Pethes")};
    expect(bool(faction));
    expect(faction->has_value());
    expect((*faction)->reputation > 87.4 and (*faction)->reputation < 87.6) << "the reputation lost";
  };

  "a replayed body_signals event replaces the genus list rather than duplicating it"_test = [&]
  {
    std::vector<::events::genus_t> const first_pass{
      ::events::genus_t{.Genus_Localised = "Bacterium", .Species_Localised = "", .Sampled = false},
      ::events::genus_t{.Genus_Localised = "Stratum", .Species_Localised = "", .Sampled = false}
    };
    expect(bool(dbs.store(address, body, std::span{first_pass})));

    // a second reading of the same body's signals - as a journal replay after a restart would do -
    // must not stack a second copy of the same two genuses on top of the first
    expect(bool(dbs.store(address, body, std::span{first_pass})));

    auto reloaded{dbs.load_system(address)};
    expect(bool(reloaded));
    expect(reloaded->has_value());
    auto const & genuses{std::get<planet_details_t>((**reloaded).bodies[0].details).genuses_};
    expect(genuses.size() == 2u) << "genus rows duplicated on replay, got:" << genuses.size();
  };

  "mapping after a sample keeps the species and drops a genus no longer listed"_test = [&]
  {
    expect(bool(dbs.store_genus_species(address, body, "Bacterium", "Bacterium Aurasus", true, true)));
    std::vector<::events::genus_t> const mapped{
      ::events::genus_t{.Genus_Localised = "Bacterium", .Species_Localised = "", .Sampled = false},
      ::events::genus_t{.Genus_Localised = "Tussock", .Species_Localised = "", .Sampled = false}
    };
    expect(bool(dbs.store(address, body, std::span{mapped})));

    auto reloaded{dbs.load_system(address)};
    expect(bool(reloaded) and reloaded->has_value());
    auto const & genuses{std::get<planet_details_t>((**reloaded).bodies[0].details).genuses_};
    expect(genuses.size() == 2u) << "got:" << genuses.size();
    auto const bacterium{std::ranges::find(genuses, "Bacterium"sv, &::events::genus_t::Genus_Localised)};
    expect(bacterium != genuses.end() and bacterium->Species_Localised == "Bacterium Aurasus"sv) << "species lost";
    expect(std::ranges::find(genuses, "Stratum"sv, &::events::genus_t::Genus_Localised) == genuses.end());
  };

  // this is the whole point of the split: the shared knowledge of the galaxy can be rebuilt by the second
  // account, and this character's progress has to survive that - hence natural keys rather than oids
  // the rule Artur pointed out: when a faction's retreat completes it leaves the system, and every
  // asset it held there passes to whoever controls the system. The danger is the obvious reading of
  // it - engineer bases and megaships are held by names that never stand in a faction list at all,
  // and taking their stations from them would be wrong far more often than the rule is right
  "a completed retreat hands the settlements to whoever controls the system"_test = [&]
  {
    using namespace std::chrono;
    constexpr uint64_t reach{4242424242ull};

    star_system_t system_row{.system_address = reach, .name = "Test Reach"};
    expect(bool(dbs.store(system_row)));
    system_row.controlling_faction = "Holders";
    expect(bool(dbs.update_system_info(system_row)));

    expect(bool(dbs.update_faction_info(info::faction_info_t{.name = "Holders"})));
    expect(bool(dbs.update_faction_info(info::faction_info_t{.name = "Leavers"})));

    auto holders{dbs.faction_oid("Holders")};
    auto leavers{dbs.faction_oid("Leavers")};
    expect(holders and *holders);
    expect(leavers and *leavers);

    sys_seconds const before{sys_days{2026y / 9 / 20}};
    sys_seconds const after{sys_days{2026y / 9 / 25}};

    // both played the background simulation here, which is what tells them from an engineer
    for(auto const oid: {int64_t(**holders), int64_t(**leavers)})
      expect(bool(dbs.store(
        info::faction_influence_t{.faction_oid = oid, .system_address = reach, .timestamp = before, .influence = 0.2}
      )));

    // the newest reading of the system saw only the holders - the leavers are gone
    expect(bool(dbs.store_faction_seen(int64_t(**leavers), reach, before)));
    expect(bool(dbs.store_faction_seen(int64_t(**holders), reach, after)));

    expect(bool(dbs.store(
      info::station_t{
        .market_id = 91001u,
        .system_address = reach,
        .name = "Left Behind",
        .station_type = "OnFootSettlement",
        .controlling_faction = "Leavers"
      }
    )));
    expect(bool(dbs.store(
      info::station_t{
        .market_id = 91002u,
        .system_address = reach,
        .name = "Tinkerer's Workshop",
        .station_type = "Outpost",
        .controlling_faction = "Hilda Tinkerer"
      }
    )));

    auto abandoned{dbs.load_place_owner("Test Reach", "Left Behind")};
    expect(abandoned and abandoned->has_value());
    expect(**abandoned == "Holders") << "a settlement left behind by a retreat kept its old owner";

    auto engineer{dbs.load_place_owner("Test Reach", "Tinkerer's Workshop")};
    expect(engineer and engineer->has_value());
    expect(**engineer == "Hilda Tinkerer") << "an engineer's base was taken from them by the retreat rule";

    // the settlement list of the system and a single read show the same owner as the mission lookup
    auto listed{dbs.load_stations(reach)};
    expect(listed and listed->size() == 2u);
    if(listed and listed->size() == 2u)
      {
      expect((*listed)[0].controlling_faction == "Holders") << "the system's list kept the retreated owner";
      expect((*listed)[1].controlling_faction == "Hilda Tinkerer");
      }
    auto single{dbs.load_station(91001u)};
    expect(single and single->has_value() and (*single)->controlling_faction == "Holders");
  };


  "a construction finished while docked leaves as the settlement it became"_test = [&]
  {
    constexpr uint64_t site{4393068035ull};
    // Docked at the site, then ApproachSettlement of the finished one - with no type - then Undocked
    expect(bool(dbs.store(info::station_t{
      .market_id = site,
      .system_address = 4242424244ull,
      .name = "Planetary Construction Site: Horwood Military Camp",
      .station_type = "PlanetaryConstructionDepot",
      .dist_from_star_ls = 812.0
    })));
    expect(bool(dbs.store(info::station_t{
      .market_id = site, .system_address = 4242424244ull, .name = "Horwood Military Camp", .station_type = {}
    })));
    expect(bool(dbs.store(info::station_t{
      .market_id = site, .system_address = 0u, .name = "Horwood Military Camp", .station_type = "OnFootSettlement"
    })));

    auto station{dbs.load_station(site)};
    expect(station and station->has_value());
    expect((*station)->name == "Horwood Military Camp");
    expect((*station)->station_type == "OnFootSettlement") << "the type of the finished place came from Undocked";
    expect((*station)->system_address == 4242424244ull) << "Undocked's zero system erased the known one";
    expect((*station)->dist_from_star_ls == 812.0) << "Undocked erased the distance Docked gave";
  };

  "a system is last seen at its newest reading, with its influence unchanged"_test = [&]
  {
    using namespace std::chrono;
    constexpr uint64_t quiet{4242424245ull};

    expect(bool(dbs.update_faction_info(info::faction_info_t{.name = "Quiet Settlers"})));
    auto oid{dbs.faction_oid("Quiet Settlers")};
    expect(oid and *oid);

    auto never{dbs.last_seen(quiet)};
    expect(never and not never->has_value());

    sys_seconds const changed{sys_days{2026y / 9 / 26} + 14h};
    sys_seconds const visit{sys_days{2026y / 9 / 28} + 3h};
    expect(bool(dbs.store(info::faction_influence_t{
      .faction_oid = int64_t(**oid), .system_address = quiet, .timestamp = changed, .influence = 0.57
    })));
    expect(bool(dbs.store_faction_seen(int64_t(**oid), quiet, changed)));
    // the next visits find the same influence: presence moves on, influence writes nothing new
    expect(bool(dbs.store_faction_seen(int64_t(**oid), quiet, visit)));

    auto seen{dbs.last_seen(quiet)};
    expect(seen and seen->has_value() and **seen == visit);
    auto tick{dbs.last_local_tick(quiet, info::tick_kind_e::influence)};
    expect(tick and tick->has_value() and **tick == changed);
  };

  "the last tick in a system is the last change of influence, not of a state"_test = [&]
  {
    using namespace std::chrono;
    constexpr uint64_t flicker{4242424243ull};

    expect(bool(dbs.update_faction_info(info::faction_info_t{.name = "Flickerers"})));
    auto oid{dbs.faction_oid("Flickerers")};
    expect(oid and *oid);

    sys_seconds const tick{sys_days{2026y / 9 / 26} + 14h};
    // FSDJump says Retreat, Location says None - the same influence each time
    expect(bool(dbs.store(info::faction_influence_t{
      .faction_oid = int64_t(**oid), .system_address = flicker, .timestamp = tick - 24h, .influence = 0.12
    })));
    expect(bool(dbs.store(info::faction_influence_t{
      .faction_oid = int64_t(**oid), .system_address = flicker, .timestamp = tick, .influence = 0.105
    })));
    for(auto const [offset, state]: {std::pair{4h, "Retreat"}, std::pair{5h, "None"}, std::pair{6h, "Retreat"}})
      expect(bool(dbs.store(info::faction_influence_t{
        .faction_oid = int64_t(**oid),
        .system_address = flicker,
        .timestamp = tick + offset,
        .influence = 0.105,
        .faction_state = state
      })));

    auto last{dbs.last_local_tick(flicker, info::tick_kind_e::influence)};
    expect(last and last->has_value());
    expect(last and last->has_value() and **last == tick) << "a state named anew was taken for the tick";
  };

  "the territory is every system one's factions are in at its newest reading, the ones gone left out"_test = [&]
  {
    using namespace std::chrono;
    constexpr uint64_t home{4242424246ull};
    constexpr uint64_t lost{4242424247ull};
    sys_seconds const before{sys_days{2026y / 9 / 20}};
    sys_seconds const after{sys_days{2026y / 9 / 25}};

    star_system_t home_row{.system_address = home, .name = "Test Home"};
    expect(bool(dbs.store(home_row)));
    home_row.controlling_faction = "Homers";
    home_row.population = 76117u;
    expect(bool(dbs.update_system_info(home_row)));
    expect(bool(dbs.store(star_system_t{.system_address = lost, .name = "Test Lost"})));

    for(std::string_view const name: {"Homers", "Guests", "Gone Guests"})
      expect(bool(dbs.update_faction_info(info::faction_info_t{.name = std::string{name}})));
    auto homers{dbs.faction_oid("Homers")};
    auto guests{dbs.faction_oid("Guests")};
    auto gone{dbs.faction_oid("Gone Guests")};
    expect(homers and *homers and guests and *guests and gone and *gone);

    for(auto const [oid, influence]:
        {std::pair{int64_t(**homers), 0.6}, std::pair{int64_t(**guests), 0.35}, std::pair{int64_t(**gone), 0.05}})
      expect(bool(dbs.store(
        info::faction_influence_t{.faction_oid = oid, .system_address = home, .timestamp = before, .influence = influence}
      )));
    expect(bool(dbs.store_faction_seen(int64_t(**gone), home, before)));
    expect(bool(dbs.store_faction_seen(int64_t(**homers), home, after)));
    expect(bool(dbs.store_faction_seen(int64_t(**guests), home, after)));
    // the homers retreated from the other system - its newest reading has only the guests
    expect(bool(dbs.store_faction_seen(int64_t(**homers), lost, before)));
    expect(bool(dbs.store_faction_seen(int64_t(**guests), lost, after)));

    std::vector<std::string> const own{"Homers"};
    auto territory{dbs.load_territory(own)};
    expect(territory and territory->size() == 1u) << "a system the faction has left is still in its territory";
    if(not territory or territory->size() != 1u)
      return;
    territory::system_t const & system{territory->front()};
    expect(system.name == "Test Home" and system.controlling == "Homers" and system.population == 76117u);
    expect(system.factions.size() == 2u) << "a faction gone from the newest reading is still listed";
    expect(system.factions.size() == 2u and system.factions[0].name == "Homers");
    expect(system.factions.size() == 2u and std::abs(system.factions[0].influence - 60.0) < 1e-9);
    expect(system.seen.has_value() and *system.seen == after);
  };

  "a carrier's shelf readings keep their time"_test = [&]
  {
    expect(bool(dbs.update_carrier(info::carrier_t{.oid = -1, .market_id = 42u, .carrier_name = "Maria", .carrier_id = "W1V-NXM", .tracked = true})));
    auto carrier{dbs.load_carrier("W1V-NXM")};
    expect(bool(carrier) and carrier->has_value());
    expect(bool(dbs.store(info::micro_resource_t{.name = "weaponschematic", .id = 7u, .localised = "Weapon Schematic", .category = "Item"})));
    // the time is kept in seconds - the sales are told apart by the gaps between readings, so a lost time loses them all
    for(int64_t const t: {1'790'000'000, 1'790'003'600})
      expect(bool(dbs.store(info::fcmaterial_t{
        .oid = -1, .carrier_id = (*carrier)->oid, .timestamp = t, .material_id = 7u, .price = 3'325'000u, .stock = 4u, .demand = 0u
      })));
    auto history{dbs.load_carrier_history("W1V-NXM", {})};
    expect(bool(history) and history->size() == 2_u);
    expect(history->back().timestamp.time_since_epoch().count() == 1'790'003'600_ll);
    expect(history->back().name == "weaponschematic" and history->back().category == "Item");
    auto stock{dbs.load_carrier_stock("W1V-NXM")};
    expect(bool(stock) and stock->size() == 1_u and stock->front().timestamp.time_since_epoch().count() == 1'790'003'600_ll);
  };

  "a galaxy rebuild loses no progress"_test = [&]
  {
    dbs.close();
    fs::remove("galaxy.sqlite");

    database_storage_t fresh{"elite.sqlite"};
    expect(bool(fresh.open()));
    expect(bool(fresh.store(system)));

    auto reloaded{fresh.load_system(address)};
    expect(bool(reloaded));
    expect(reloaded->has_value());
    star_system_t const & got{**reloaded};
    expect(got.fss_complete) << "fss_complete did not survive the galaxy rebuild";
    expect(std::get<planet_details_t>(got.bodies[0].details).mapped) << "mapped did not survive the galaxy rebuild";
    expect(std::get<planet_details_t>(got.bodies[0].details).genuses_[0].Sampled)
      << "the sample did not survive the galaxy rebuild";

    // the faction's identity went with galaxy and comes back from journals with a new oid; the reputation
    // survived in the personal database and is to attach itself to it by name, despite the different oid
    expect(bool(fresh.update_faction_info(info::faction_info_t{.name = "Crew of Pethes", .reputation = 87.5})));

    auto faction{fresh.load_faction("Crew of Pethes")};
    expect(bool(faction));
    expect(faction->has_value());
    expect((*faction)->reputation > 87.4) << "the reputation did not survive the galaxy rebuild";
  };

  return {};
  }