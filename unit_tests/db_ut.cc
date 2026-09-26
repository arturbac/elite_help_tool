#include <databse_storage.h>
#include <glaze/glaze.hpp>
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

  "postep postaci wraca z osobnej bazy"_test = [&]
  {
    expect(bool(dbs.store_fss_complete(address)));
    expect(bool(dbs.store_dss_complete(address, body)));
    expect(bool(dbs.store_genus_species(address, body, "Bacterium", "Bacterium Aurasus", true)));
    expect(bool(dbs.update_faction_info(info::faction_info_t{.name = "Crew of Pethes", .reputation = 87.5})));

    auto reloaded{dbs.load_system(address)};
    expect(bool(reloaded)) << "system sie nie wczytal";
    expect(reloaded->has_value());
    star_system_t const & got{**reloaded};
    expect(got.fss_complete) << "fss_complete zgubione";
    expect(std::get<planet_details_t>(got.bodies[0].details).mapped) << "mapped zgubione";

    auto const & genuses{std::get<planet_details_t>(got.bodies[0].details).genuses_};
    expect(genuses.size() == 1u);
    expect(genuses[0].Sampled) << "probka zgubiona";
    expect(genuses[0].Species_Localised == "Bacterium Aurasus"sv) << "gatunek zgubiony";

    auto faction{dbs.load_faction("Crew of Pethes")};
    expect(bool(faction));
    expect(faction->has_value());
    expect((*faction)->reputation > 87.4 and (*faction)->reputation < 87.6) << "reputacja zgubiona";
  };

  // to jest cala stawka rozdzialu: wspolna wiedza o galaktyce moze byc przebudowana przez drugie
  // konto, a postep tej postaci ma to przezyc - dlatego klucze sa naturalne, a nie oid-owe
  "przebudowa galaxy nie gubi postepu"_test = [&]
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
    expect(got.fss_complete) << "fss_complete nie przezylo przebudowy galaxy";
    expect(std::get<planet_details_t>(got.bodies[0].details).mapped) << "mapped nie przezylo przebudowy galaxy";
    expect(std::get<planet_details_t>(got.bodies[0].details).genuses_[0].Sampled)
      << "probka nie przezyla przebudowy galaxy";

    // tozsamosc frakcji przepadla razem z galaxy i wraca z journali z nowym oid; reputacja
    // przezyla w bazie osobistej i ma sie do niej przyczepic po nazwie, mimo innego oid
    expect(bool(fresh.update_faction_info(info::faction_info_t{.name = "Crew of Pethes", .reputation = 87.5})));

    auto faction{fresh.load_faction("Crew of Pethes")};
    expect(bool(faction));
    expect(faction->has_value());
    expect((*faction)->reputation > 87.4) << "reputacja nie przezyla przebudowy galaxy";
  };

  return {};
  }
