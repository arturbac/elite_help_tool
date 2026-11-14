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
  }
