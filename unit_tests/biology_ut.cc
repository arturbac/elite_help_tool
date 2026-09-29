#include <boost/ut.hpp>
#include <biology.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <vector>

auto main() -> int
  {
  using namespace boost::ut;

  "surface"_test = []
  {
    "a degree of longitude on the equator"_test = []
    {
      // a body of 1000 km radius - a degree is 2*pi*1e6/360 = 17453 m
      auto const d{bio::surface_distance_m({0.0, 0.0}, {0.0, 1.0}, 1'000'000.0)};
      expect(d > 17'450.0 and d < 17'456.0) << d;
    };

    "a step near the pole is short even across many degrees of longitude"_test = []
    {
      auto const d{bio::surface_distance_m({89.999, 0.0}, {89.999, 90.0}, 1'000'000.0)};
      expect(d < 30.0) << d;
    };

    "bearing east and north"_test = []
    {
      expect(std::abs(bio::bearing_deg({0.0, 0.0}, {0.0, 1.0}) - 90.0) < 0.01);
      expect(std::abs(bio::bearing_deg({0.0, 0.0}, {1.0, 0.0}) - 0.0) < 0.01);
      expect(std::abs(bio::bearing_deg({0.0, 0.0}, {-1.0, 0.0}) - 180.0) < 0.01);
    };
  };

  "at risk"_test = []
  {
    // a directory of journals, oldest first by name, as the game writes them
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / "eht_at_risk_ut"};
    std::filesystem::create_directories(dir);
    auto const write = [&](char const * name, std::initializer_list<char const *> lines)
    {
      std::ofstream out{dir / name, std::ios::trunc};
      for(char const * line: lines)
        out << line << '\n';
    };
    write(
      "Journal.2026-09-01T100000.01.log",
      {R"({ "event":"Commander", "FID":"F1", "Name":"ME" })",
       R"({ "event":"Died" })",
       R"({ "event":"ScanOrganic", "ScanType":"Analyse", "Species_Localised":"Bacterium Aurasus" })"}
    );
    write(
      "Journal.2026-09-02T100000.01.log",
      {R"({ "event":"Commander", "FID":"F2", "Name":"OTHER" })",
       R"({ "event":"SellOrganicData" })",
       R"({ "event":"Died" })"}
    );
    write(
      "Journal.2026-09-03T100000.01.log",
      {R"({ "event":"Commander", "FID":"F1", "Name":"ME" })",
       R"({ "event":"Bounty", "Rewards":[ { "Faction":"A", "Reward":100 }, { "Faction":"B", "Reward":10 } ] })",
       R"({ "event":"RedeemVoucher", "Type":"bounty", "Factions":[ { "Faction":"A", "Amount":100 } ] })",
       R"({ "event":"RedeemVoucher", "Type":"CombatBond", "Factions":[ { "Faction":"B", "Amount":5 } ] })",
       R"({ "event":"Bounty", "Rewards":[ { "Faction":"A", "Reward":1000 } ] })",
       R"({ "event":"ScanOrganic", "ScanType":"Log", "Species_Localised":"Frutexa Acus" })",
       R"({ "event":"ScanOrganic", "ScanType":"Analyse", "Species_Localised":"Frutexa Acus", "WasLogged":false })",
       R"({ "event":"Scan", "StarSystem":"S1", "BodyName":"S1 A", "StarType":"K", "StellarMass":1.0, "WasDiscovered":true })",
       R"({ "event":"Scan", "StarSystem":"S2", "BodyName":"S2 A", "StarType":"N", "StellarMass":1.0, "WasDiscovered":true })",
       R"({ "event":"MultiSellExplorationData", "Discovered":[ { "SystemName":"S1", "NumBodies":1 } ] })"}
    );

    bio::at_risk_t const mine{bio::at_risk(dir, "F1")};
    // the other account's sale and death are not mine; my own death two sessions back ends the count, after
    // the sample analysed later in that session
    expect(mine.samples.size() == 2_u) << mine.samples.size();
    // the newest first: Frutexa Acus, logged by nobody before, brings four times its value on top
    expect(mine.samples.front().bonus == uint64_t{mine.samples.front().value} * 4u) << mine.samples.front().bonus;
    expect(mine.samples.back().bonus == 0_u) << mine.samples.back().bonus;
    // A's first bounty was handed in, B's never was, A's second came after the hand-in
    expect(mine.bounties == 1010_u) << mine.bounties;
    // S1 was sold after its scan, S2 was not: a neutron star of one solar mass, 22628 * (1 + 1/66.25)
    expect(mine.cartography == 22969_u) << mine.cartography;

    bio::at_risk_t const anyone{bio::at_risk(dir, "")};
    // with no account named, the other one's death ends it at once
    expect(anyone.samples.size() == 1_u) << anyone.samples.size();

    for(char const * name:
        {"Journal.2026-09-01T100000.01.log", "Journal.2026-09-02T100000.01.log", "Journal.2026-09-03T100000.01.log"})
      std::filesystem::remove(dir / name);
    std::filesystem::remove(dir);
  };

  "cartography sales"_test = []
  {
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / "eht_cartography_ut"};
    std::filesystem::create_directories(dir);
    auto const write = [&](char const * name, std::initializer_list<char const *> lines)
    {
      std::ofstream out{dir / name, std::ios::trunc};
      for(char const * line: lines)
        out << line << '\n';
    };
    write(
      "Journal.2026-09-01T100000.01.log",
      {R"({ "event":"Commander", "FID":"F1", "Name":"ME" })",
       R"({ "event":"Scan", "StarSystem":"S1", "BodyName":"S1 A", "StarType":"N", "StellarMass":1.0, "WasDiscovered":true })",
       R"({ "event":"Scan", "StarSystem":"S1", "BodyName":"S1 1", "PlanetClass":"Icy body", "MassEM":1.0, "WasDiscovered":true, "WasMapped":true })",
       R"({ "event":"Scan", "StarSystem":"S2", "BodyName":"S2 A", "StarType":"N", "StellarMass":1.0, "WasDiscovered":true })",
       R"({ "timestamp":"2026-09-01T11:00:00Z", "event":"MultiSellExplorationData", "Discovered":[ { "SystemName":"S1", "NumBodies":2 } ], "BaseValue":50000, "Bonus":0, "TotalEarnings":46500 })"}
    );
    write(
      "Journal.2026-09-02T100000.01.log",
      {R"({ "event":"Commander", "FID":"F2", "Name":"OTHER" })",
       R"({ "event":"Scan", "StarSystem":"S2", "BodyName":"S2 B", "StarType":"N", "StellarMass":1.0, "WasDiscovered":true })",
       R"({ "timestamp":"2026-09-02T11:00:00Z", "event":"SellExplorationData", "Systems":[ "S9" ], "Discovered":[ "S9" ], "BaseValue":1, "Bonus":0, "TotalEarnings":1 })"}
    );
    write(
      "Journal.2026-09-03T100000.01.log",
      {R"({ "event":"Commander", "FID":"F1", "Name":"ME" })",
       R"({ "timestamp":"2026-09-03T11:00:00Z", "event":"SellExplorationData", "Systems":[ "S2", "S3" ], "Discovered":[ "S2", "S3" ], "BaseValue":30000, "Bonus":0, "TotalEarnings":30000 })"}
    );

    auto const sales{bio::cartography_sales(dir, "F1")};
    // the other account's sale is not mine, nor its scan of S2 B
    expect(sales.size() == 2_u) << sales.size();
    if(sales.size() == 2u)
      {
      // a neutron star 22628 * (1 + 1/66.25) and an icy body scanned but not mapped, at the floor of 500
      expect(sales[0].systems == std::vector<std::string>{"S1"});
      expect(sales[0].estimate == 23469_u) << sales[0].estimate;
      expect(sales[0].priced == 2_u and sales[0].bodies == 2_u and sales[0].total == 46500_u);
      expect(sales[0].when == std::chrono::sys_days{std::chrono::September / 1 / 2026} + std::chrono::hours{11});
      // the single sale names its systems plainly and counts no bodies
      expect(sales[1].systems.size() == 2_u and sales[1].bodies == 0_u and sales[1].estimate == 22969_u);
      }

    // only a sale of one system is an exact price
    auto const accuracy{bio::estimate_accuracy(sales)};
    expect(accuracy.has_value() and accuracy->sales == 1_u);
    expect(accuracy.has_value() and std::abs(accuracy->median - 46500.0 / 23469.0) < 1e-9);

    for(char const * name:
        {"Journal.2026-09-01T100000.01.log", "Journal.2026-09-02T100000.01.log", "Journal.2026-09-03T100000.01.log"})
      std::filesystem::remove(dir / name);
    std::filesystem::remove(dir);
  };

  "colony"_test = []
  {
    expect(bio::colony_range_m("Bacterium") == 500_u);
    expect(bio::colony_range_m("Osseus") == 800_u);
    expect(bio::colony_range_m("Luteolum Anemone") == 100_u);
    expect(bio::colony_range_m("Brain Trees") == 100_u);
    expect(bio::colony_range_m("Nothing") == 0_u);
  };

  "value"_test = []
  {
    expect(bio::species_value("Stratum Tectonicas") == 19'010'800u);
    // the journal's name for a brain tree carries its kind in front of the family
    expect(bio::species_value("Roseum Brain Tree") == 1'593'700u);
    expect(not bio::species_value("Nothing Whatsoever").has_value());
  };

  "predict"_test = []
  {
    using bio::species_record_t;
    std::array const history{
      species_record_t{
        "Stratum", "Stratum Tectonicas", "High metal content body", "CarbonDioxide", "", 170.0, 5.0, 1000.0, "M"
      },
      species_record_t{
        "Stratum", "Stratum Tectonicas", "High metal content body", "CarbonDioxide", "", 180.0, 4.0, 1000.0, "K"
      },
      species_record_t{"Stratum", "Stratum Excutitus", "Rocky body", "SulphurDioxide", "", 170.0, 2.0, 1000.0, "K"},
      species_record_t{"Stratum", "Stratum Paleas", "Rocky body", "CarbonDioxide", "", 250.0, 3.0, 1000.0, "F"},
      species_record_t{"Bacterium", "Bacterium Aurasus", "Rocky body", "CarbonDioxide", "", 175.0, 4.0, 1000.0, "K"},
    };

    bio::conditions_t const world{
      .planet_class = "High metal content body",
      .atmosphere_type = "CarbonDioxide",
      .volcanism = "",
      .surface_temperature = 175.0,
      .surface_gravity = 4.5,
      .surface_pressure = 1000.0,
      .star_type = "K"
    };

    auto const guess{bio::predict("Stratum", world, history)};
    expect(fatal(guess.size() == 2_u)) << "the sulphur-dioxide species must drop out";
    expect(guess[0].species == "Stratum Tectonicas");
    expect(guess[0].fit == bio::fit_e::fits);
    expect(guess[0].seen == 2_u);
    expect(guess[0].share == 1.0_d);
    expect(guess[1].species == "Stratum Paleas");
    expect(guess[1].fit == bio::fit_e::near) << "seen only far warmer";

    "a genus never seen under this atmosphere still says what it was elsewhere"_test = [&]
    {
      auto const other{bio::predict("Stratum", bio::conditions_t{.atmosphere_type = "Neon"}, history)};
      expect(other.size() == 3_u);
      expect(other[0].fit == bio::fit_e::unlike);
    };
  };

  "knowledge"_test = []
  {
    using bio::species_record_t;
    using enum bio::novelty_e;
    std::vector<species_record_t> history;
    // ten bacteria on the same kind of world under a K star
    for(int i{}; i != 10; ++i)
      history.push_back(
        species_record_t{"Bacterium", "Bacterium Aurasus", "Rocky body", "CarbonDioxide", "", 170.0 + i, 3.0, 1000.0, "K"}
      );
    history.push_back(species_record_t{"Stratum", "Stratum Tectonicas", "Rocky body", "CarbonDioxide", "", 175.0, 3.0, 1000.0, "K"});

    bio::conditions_t world{
      .planet_class = "Rocky body",
      .atmosphere_type = "CarbonDioxide",
      .surface_temperature = 175.0,
      .surface_gravity = 3.0,
      .star_type = "K"
    };
    auto const often{bio::knowledge("Bacterium", world, history, 3u)};
    expect(often.novelty == known);
    expect(often.alike == 10_u);
    expect(bio::knowledge("Stratum", world, history, 3u).novelty == few) << "one find alone";
    expect(bio::knowledge("Tussock", world, history, 3u).novelty == never);

    auto warm{world};
    warm.surface_temperature = 191.0;
    auto const warmer_one{bio::knowledge("Bacterium", warm, history, 3u)};
    expect(warmer_one.novelty == warmer);
    expect(std::abs(warmer_one.beyond - 12.0) < 1e-9);

    auto heavy{world};
    heavy.surface_gravity = 3.6;
    auto const heavier_one{bio::knowledge("Bacterium", heavy, history, 3u)};
    expect(heavier_one.novelty == heavier);
    expect(std::abs(heavier_one.beyond - 0.2) < 1e-9);

    auto neon{world};
    neon.atmosphere_type = "Neon";
    expect(bio::knowledge("Bacterium", neon, history, 3u).novelty == atmosphere);

    auto f_star{world};
    f_star.star_type = "F";
    expect(bio::knowledge("Bacterium", f_star, history, 3u).novelty == star);
  };

  "merge"_test = []
  {
    using bio::species_record_t;
    std::vector<species_record_t> own{
      species_record_t{.genus = "Stratum", .species = "Stratum Tectonicas", .system_address = 1u, .body_id = 2u},
      species_record_t{.genus = "Bacterium", .species = "Bacterium Aurasus", .system_address = 1u, .body_id = 2u},
    };
    std::vector<species_record_t> other{
      // the same find, read by both accounts out of the shared journals
      species_record_t{.genus = "Stratum", .species = "Stratum Tectonicas", .system_address = 1u, .body_id = 2u},
      species_record_t{.genus = "Stratum", .species = "Stratum Tectonicas", .system_address = 7u, .body_id = 3u},
      // and one the other galaxy holds twice
      species_record_t{.genus = "Stratum", .species = "Stratum Tectonicas", .system_address = 7u, .body_id = 3u},
      species_record_t{.genus = "Bacterium", .species = "Bacterium Aurasus", .system_address = 0u, .body_id = 1u},
    };
    bio::merge_history(own, std::move(other));
    expect(own.size() == 4_u) << "a find both accounts know counts once";
  };
  }
