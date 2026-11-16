#include <galaxy_map_view.h>

#include <boost/ut.hpp>

using namespace boost::ut;
using namespace std::string_view_literals;

auto main() -> int
  {
  using atmosphere_odds::body_t;
  using atmosphere_odds::census_t;
  using atmosphere_odds::system_t;

  "a procedural name, as the map writes it and as the journal does; a name of its own is none"_test = []
  {
    auto const map{galaxy_map_view::parse("BLEIA EOHN PW-D B32-5")};
    expect(fatal(map.has_value()));
    expect(map->sector == "BLEIA EOHN"sv and map->block == "PW-D"sv and map->mass_code == 'b' and map->number == "32-5"sv);
    auto const journal{galaxy_map_view::parse("Spoihee IT-F d12-1113")};
    expect(fatal(journal.has_value()));
    expect(journal->mass_code == 'd' and journal->number == "12-1113"sv);
    expect(galaxy_map_view::parse("Spoihee SX-J c25-1")->number == "25-1"sv);
    expect(galaxy_map_view::parse("Bleia Eohn QT-O d7-43")->mass_code == 'd');
    expect(not galaxy_map_view::parse("Colonia").has_value() and not galaxy_map_view::parse("-6,920 : -1,425 : 20,490").has_value());
  };

  auto const planet = [](uint64_t address, uint32_t id, std::string atmosphere)
  { return body_t{.address = address, .body_id = id, .atmosphere_type = std::move(atmosphere), .landable = true, .parent_star = 0}; };
  auto const star = [](uint64_t address, std::string type)
  { return body_t{.address = address, .body_id = 0u, .star = true, .star_type = std::move(type)}; };

  // c: two systems in full, one with Water (rare: known once), one with Neon; d: one in full without atmosphere, one
  // not in full; a name of its own counts in no code
  census_t const census{
    .systems = {
      system_t{1u, "K", 3u, "Alpha AB-C c1-1"},
      system_t{2u, "G", 2u, "Alpha AB-C c1-2"},
      system_t{3u, "F", 1u, "Alpha AB-D d2-3"},
      system_t{4u, "F", 9u, "Alpha AB-D d2-4"},
      system_t{5u, "K", 2u, "Colonia"}
    },
    .bodies = {
      star(1u, "K"), planet(1u, 1u, "Water"), planet(1u, 2u, "Neon"),
      star(2u, "G"), planet(2u, 1u, "Neon"),
      star(3u, "F"),
      star(4u, "F"),
      star(5u, "K"), planet(5u, 1u, "Neon")
    }
  };
  auto const prepared{atmosphere_odds::prepare(census)};

  "a mass code's systems: in full, with an atmosphere, with a rare one, their stars"_test = [&]
  {
    auto const odds{galaxy_map_view::mass_code_odds(census, prepared, 1u)};
    expect(fatal(odds.size() == 2_ul));
    auto const & c{odds.at('c')};
    expect(c.systems == 2_u and c.full == 2_u and c.with_atmosphere == 2_u and c.with_rare == 1_u);
    auto const & d{odds.at('d')};
    expect(d.systems == 2_u and d.full == 1_u and d.with_atmosphere == 0_u and d.with_rare == 0_u);
    expect(fatal(d.stars.size() == 1_ul));
    expect(d.stars[0].first == "F"sv and d.stars[0].second == 2_u);
  };

  "the names in view: each once, by mass code, the codes with rare ones first, the ones never visited"_test = [&]
  {
    auto const odds{galaxy_map_view::mass_code_odds(census, prepared, 1u)};
    auto const known{galaxy_map_view::known_names(census)};
    std::vector<std::string> const labels{
      "ALPHA AB-D D2-3", "ALPHA AB-D D2-3", "ALPHA AB-C C1-1", "ALPHA AB-C C1-7", "ALPHA AB-? C1-8", "-6,920 : -1,425 : 20,490", "COLONIA"};
    auto const view{galaxy_map_view::view(labels, odds, known)};
    expect(view.names == 3_u) << "twice drawn once, a '?' and the coordinates none, a name of its own not grouped";
    expect(view.sector == "ALPHA"sv);
    expect(fatal(view.groups.size() == 2_ul));
    expect(view.groups[0].code == 'c' and view.groups[0].in_view == 2_u);
    expect(fatal(view.groups[0].unvisited.size() == 1_ul));
    expect(view.groups[0].unvisited[0] == "ALPHA AB-C C1-7"sv);
    expect(view.groups[1].code == 'd' and view.groups[1].unvisited.empty());
  };
  }
