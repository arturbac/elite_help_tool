#include <atmosphere_odds.h>

#include <boost/ut.hpp>

using namespace boost::ut;
using namespace std::string_view_literals;

auto main() -> int
  {
  using atmosphere_odds::body_t;
  using atmosphere_odds::census_t;
  using atmosphere_odds::system_t;

  auto const star = [](uint64_t address, uint32_t id, std::string type, double distance = 0.0)
  {
    return body_t{
      .address = address, .body_id = id, .star = true, .star_type = std::move(type), .distance_ls = distance
    };
  };
  auto const planet = [](
                        uint64_t address,
                        uint32_t id,
                        std::string atmosphere,
                        int64_t parent_star,
                        int64_t parent_planet = -1,
                        std::string kind = "Rocky body"
                      )
  {
    return body_t{
      .address = address,
      .body_id = id,
      .atmosphere_type = std::move(atmosphere),
      .landable = true,
      .parent_star = parent_star,
      .parent_planet = parent_planet,
      .planet_class = std::move(kind)
    };
  };

  // 1: a K system in full with a Methane moon of a gas giant; 2: a K system in full without; 3: an M system in full
  // with a Neon planet; 4: a K system half scanned with Methane - not counted in the chance, counted in the shares
  census_t const census{
    .systems = {system_t{1u, "K", 3u}, system_t{2u, "K", 2u}, system_t{3u, "", 2u}, system_t{4u, "K", 5u}},
    .bodies = {
      star(1u, 0u, "K"),
      planet(1u, 1u, "", 0, -1, "Sudarsky class I gas giant"),
      planet(1u, 2u, "Methane", 0, 1),
      star(2u, 0u, "K"),
      planet(2u, 1u, "None", 0),
      star(3u, 0u, "M"),
      planet(3u, 1u, "Neon", 0),
      star(4u, 0u, "K"),
      planet(4u, 1u, "Methane", 0)
    }
  };

  "a system counts in the chance only when every body is known"_test = [&]
  {
    auto const prepared{atmosphere_odds::prepare(census)};
    expect(prepared.systems_known == 4_u);
    expect(fatal(prepared.full.size() == 3_ul));
    expect(prepared.full.at(3u) == "M"sv) << "no arrival star from FSDJump: the nearest star scanned";
    expect(prepared.worlds.size() == 3_ul) << "an empty or None atmosphere is none";
  };

  "the chance of a class over its systems in full, its lift over the average"_test = [&]
  {
    auto const prepared{atmosphere_odds::prepare(census)};
    auto const rows{atmosphere_odds::by_arrival_star(prepared, "Methane")};
    expect(fatal(rows.size() == 2_ul));
    expect(rows[0].key == "K"sv and rows[0].systems == 2_u and rows[0].systems_with == 1_u);
    expect(rows[0].chance == 0.5_d and std::abs(rows[0].lift - 1.5) < 1e-9) << "1 of 3 in all, 1 of 2 for K";
    expect(rows[1].key == "M"sv and rows[1].systems_with == 0_u and rows[1].weak);
    auto const list{atmosphere_odds::atmospheres(prepared)};
    expect(fatal(list.size() == 2_ul));
    expect(list[0].atmosphere == "Neon"sv) << "the rarest first";
    expect(list[1].worlds == 2_u and list[1].systems_with == 1_u);
  };

  "the parent star and the parent body"_test = [&]
  {
    auto const prepared{atmosphere_odds::prepare(census)};
    auto const stars{atmosphere_odds::by_parent_star(prepared, "Methane")};
    expect(fatal(stars.size() == 1_ul));
    expect(stars[0].key == "K"sv and stars[0].worlds == 2_u and stars[0].share == 1.0_d);
    expect(std::abs(stars[0].background - 2.0 / 3.0) < 1e-9);
    auto const bodies{atmosphere_odds::by_parent_body(prepared, "Methane")};
    expect(fatal(bodies.size() == 2_ul));
    expect(bodies[0].key == atmosphere_odds::parent_name(atmosphere_odds::parent_e::star));
    expect(bodies[1].key == atmosphere_odds::parent_name(atmosphere_odds::parent_e::gas_giant));
  };

  "another galaxy adds what the first does not know"_test = [&]
  {
    census_t const other{
      .systems = {system_t{3u, "M", 2u}, system_t{5u, "T", 1u}},
      .bodies = {star(3u, 0u, "M"), star(5u, 0u, "T"), star(1u, 0u, "K")}
    };
    auto const merged{atmosphere_odds::merge(census, other)};
    expect(merged.systems.size() == 5_ul);
    expect(merged.systems[2].arrival_type == "M"sv) << "the arrival star taken where the first had none";
    expect(merged.bodies.size() == census.bodies.size() + 1u) << "a body once";
  };

  "the landable planets of each atmosphere, a planet in two galaxies once"_test = [&]
  {
    using atmosphere_odds::landable_t;
    auto const counts{atmosphere_odds::landable_counts(
      {landable_t{1u, 3u, "Argon"}, landable_t{1u, 3u, "Argon"}, landable_t{2u, 3u, "Argon"}, landable_t{2u, 4u, "Water"},
       landable_t{2u, 5u, "None"}}
    )};
    expect(fatal(counts.size() == 2_ul));
    expect(counts.at("Argon") == 2_u) << "the same planet from the other galaxy counts once";
    expect(counts.at("Water") == 1_u);
  };

  "the atmospheres here, the rarest first, a planet not in the databases yet still known"_test = [&]
  {
    atmosphere_odds::counts_t const known{{"CarbonDioxide", 456u}, {"Argon", 255u}, {"Water", 5u}};
    std::vector<std::string> const here{"CarbonDioxide", "None", "", "Argon", "CarbonDioxide", "Water", "Oxygen"};
    auto const rows{atmosphere_odds::atmospheres_here(here, known)};
    expect(fatal(rows.size() == 4_ul));
    expect(rows[0].atmosphere == "Oxygen"sv and rows[0].here == 1_u and rows[0].known == 1_u);
    expect(rows[1].atmosphere == "Water"sv and rows[1].known == 5_u);
    expect(rows[2].atmosphere == "Argon"sv and rows[2].known == 255_u);
    expect(rows[3].atmosphere == "CarbonDioxide"sv and rows[3].here == 2_u and rows[3].known == 456_u);
  };

  "a planet outside what is known of its type, itself left out of the range"_test = [&]
  {
    using atmosphere_odds::beyond_e;
    using atmosphere_odds::landable_t;
    std::vector<landable_t> known;
    for(uint32_t i{}; i != 6u; ++i)
      known.push_back(landable_t{1u, i, "Ammonia", 152.0 + i * 4.0, 200.0 + i * 100.0, 1.0 + i * 0.2, i == 0u});
    landable_t const cold{2u, 1u, "Ammonia", 148.0, 300.0, 1.5, false};
    known.push_back(cold);
    auto const found{atmosphere_odds::outliers(cold, known)};
    expect(fatal(found.size() == 1_ul));
    expect(found[0].what == beyond_e::colder and found[0].low == 152.0_d and found[0].high == 172.0_d)
      << "the planet itself is no part of the range";
    landable_t const heavy_hot{2u, 2u, "Ammonia", 180.0, 900.0, 2.5, true};
    auto const more{atmosphere_odds::outliers(heavy_hot, known)};
    expect(fatal(more.size() == 4_ul));
    expect(more[0].what == beyond_e::warmer and more[1].what == beyond_e::denser and more[2].what == beyond_e::heavier);
    expect(more[3].what == beyond_e::volcanic and more[3].known == 1_u) << "one other volcanic: worth a look";
    expect(atmosphere_odds::outliers(landable_t{1u, 2u, "Ammonia", 160.0, 400.0, 1.4, false}, known).empty())
      << "inside the range: nothing";
  };

  "a type known on few planets says so instead of a range"_test = [&]
  {
    using atmosphere_odds::landable_t;
    std::vector<landable_t> const known{landable_t{1u, 1u, "Water", 430.0, 9000.0, 0.5, false}, landable_t{1u, 2u, "Water", 440.0, 9100.0, 0.5, false}};
    auto const found{atmosphere_odds::outliers(landable_t{3u, 1u, "Water", 500.0, 9500.0, 0.6, false}, known)};
    expect(fatal(found.size() == 1_ul));
    expect(found[0].what == atmosphere_odds::beyond_e::few_known and found[0].known == 2_u);
  };
  }
