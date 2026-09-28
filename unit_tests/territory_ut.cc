#include <boost/ut.hpp>
#include <territory.h>

namespace
  {
using namespace std::chrono_literals;

constexpr std::chrono::sys_seconds wave{std::chrono::sys_days{std::chrono::September / 27 / 2026} + 9h};

[[nodiscard]]
auto faction(std::string name, double influence) -> territory::faction_t
  {
  return territory::faction_t{
    .name = std::move(name), .influence = influence, .moved = {}, .active = {}, .pending = {}
  };
  }

///\brief two systems as they stood on 28.09: Big Ounce behind Crew in BD-I, Crew third in CD-I
[[nodiscard]]
auto cluster() -> std::vector<territory::system_t>
  {
  territory::system_t bd{.system_address = 1u, .name = "Bleia Eohn BD-I a64-1", .controlling = "Crew of Pethes"};
  bd.factions
    = {faction("Crew of Pethes", 48.4), faction("Big Ounce Buccaneers", 41.2), faction("Camorra of Purui", 10.4)};
  territory::system_t cd{.system_address = 2u, .name = "Bleia Eohn CD-I a64-0", .controlling = "Cartel of HIP 83983"};
  cd.factions = {
    faction("Cartel of HIP 83983", 37.3),
    faction("The Dukes of Mikunn", 32.3),
    faction("Crew of Pethes", 24.0),
    faction("The Mercs of Mikunn", 6.5)
  };
  territory::system_t pethes{.system_address = 3u, .name = "Pethes", .controlling = "Equestrian Naval Fleet"};
  pethes.factions = {faction("Equestrian Naval Fleet", 73.4), faction("Crew of Pethes", 1.6)};
  return {bd, cd, pethes};
  }

std::vector<std::string> const own{"Crew of Pethes", "Big Ounce Buccaneers", "Camorra of Purui"};
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  "the tick is known where influence moved since the wave, unchanged where only read, else not seen"_test = []
  {
    territory::system_t system{.name = "x"};
    expect(territory::tick_seen(system, wave) == territory::tick_seen_e::not_seen);
    system.seen = wave - 1h;
    system.changed = wave - 20h;
    expect(territory::tick_seen(system, wave) == territory::tick_seen_e::not_seen);
    system.seen = wave + 2h;
    expect(territory::tick_seen(system, wave) == territory::tick_seen_e::unchanged);
    system.changed = wave + 1h;
    expect(territory::tick_seen(system, wave) == territory::tick_seen_e::known);
    // with no wave known there is nothing to wait for
    expect(territory::tick_seen(territory::system_t{}, std::nullopt) == territory::tick_seen_e::known);
  };

  "the lead is the controlling faction's over the strongest of the rest, below zero once overtaken"_test = []
  {
    auto systems{cluster()};
    auto held{territory::lead(systems[0])};
    expect(held.has_value() and held->rival == std::string{"Big Ounce Buccaneers"});
    expect(held.has_value() and std::abs(held->margin - 7.2) < 1e-9);

    // the game names the controller, the strongest may be another - control changes only through a conflict
    systems[1].controlling = "The Dukes of Mikunn";
    auto overtaken{territory::lead(systems[1])};
    expect(overtaken.has_value() and overtaken->rival == std::string{"Cartel of HIP 83983"});
    expect(overtaken.has_value() and overtaken->margin < 0.0);

    systems[1].controlling = "nobody here";
    expect(not territory::lead(systems[1]).has_value());
  };

  "standings count control and find the nearest system to take and the thinnest lead held"_test = []
  {
    auto const systems{cluster()};
    auto const result{territory::standings(systems, own)};
    expect(result.size() == 3u);

    territory::standing_t const & crew{result[0]};
    expect(crew.present == 3u and crew.controls == 1u);
    expect(crew.thinnest_system == std::string{"Bleia Eohn BD-I a64-1"} and crew.thinnest_lead.has_value());
    expect(crew.thinnest_rival == std::string{"Big Ounce Buccaneers"});
    // 13.3 behind the Cartel in CD-I is nearer than 71.8 behind the Fleet in Pethes
    expect(crew.closest_system == std::string{"Bleia Eohn CD-I a64-0"});
    expect(crew.closest_controller == std::string{"Cartel of HIP 83983"});
    expect(crew.closest_gap.has_value() and std::abs(*crew.closest_gap - 13.3) < 1e-9);

    territory::standing_t const & ounce{result[1]};
    expect(ounce.present == 1u and ounce.controls == 0u and not ounce.thinnest_lead.has_value());
    expect(ounce.closest_system == std::string{"Bleia Eohn BD-I a64-1"});

    territory::standing_t const & camorra{result[2]};
    expect(camorra.present == 1u and camorra.closest_controller == std::string{"Crew of Pethes"});
  };

  "notes name the wars, the retreats and one's own faction low before the game says Retreat"_test = []
  {
    auto systems{cluster()};
    systems[1].factions[3].active = "CivilUnrest, Bust, Retreat";
    systems[1].factions[1].pending = "Retreat";
    systems[1].wars.push_back(
      territory::war_t{
        .war_type = "war",
        .faction1 = "Crew of Pethes",
        .faction2 = "The Mercs of Mikunn",
        .won_days1 = 3u,
        .won_days2 = 0u,
        .ticks_left = 1u,
        .active = true
      }
    );
    auto const found{territory::notes(systems[1], own, 2.5)};
    expect(found.size() == 3u);
    expect(
      found.size() == 3u and found[0] == std::string{"war: Crew of Pethes 3:0 The Mercs of Mikunn, 1 more war ticks"}
    );
    expect(found.size() == 3u and found[1] == std::string{"The Dukes of Mikunn to retreat at 32.3%"});
    expect(found.size() == 3u and found[2] == std::string{"The Mercs of Mikunn retreats at 6.5%"});

    // a word for a state is a whole word - "Retreating" or a part of another does not count
    systems[1].factions[3].active = "NotRetreat";
    systems[1].factions[1].pending.clear();
    systems[1].wars.clear();
    expect(territory::notes(systems[1], own, 2.5).empty());

    // someone else's faction low is their affair; one's own is warned of
    auto const low{territory::notes(systems[2], own, 2.5)};
    expect(low.size() == 1u and low[0].starts_with("Crew of Pethes low at 1.6%"));
    systems[2].factions[1].name = "Noblemen of Pethes";
    expect(territory::notes(systems[2], own, 2.5).empty());
  };

  "the shared sector comes off the procedural names, proper names stay whole"_test = []
  {
    auto const systems{cluster()};
    std::string const sector{territory::common_sector(systems)};
    expect(sector == std::string{"Bleia Eohn"});
    expect(territory::short_name("Bleia Eohn BD-I a64-1", sector) == std::string{"BD-I a64-1"});
    expect(territory::short_name("Pethes", sector) == std::string{"Pethes"});
    expect(territory::short_name("Bleia Eohnx AB-C d1", sector) == std::string{"Bleia Eohnx AB-C d1"});

    // one procedural name is no cluster, and a proper name of three words has no sector
    std::vector<territory::system_t> lone{systems[0], systems[2]};
    expect(territory::common_sector(lone).empty());
    std::vector<territory::system_t> proper{
      territory::system_t{.name = "Col 285 Sector"}, territory::system_t{.name = "Col 285 Sector"}
    };
    expect(territory::common_sector(proper).empty());
    std::vector<territory::system_t> multi{
      territory::system_t{.name = "Col 285 Sector JF-N a50-4"}, territory::system_t{.name = "Col 285 Sector AB-C d1-2"}
    };
    expect(territory::common_sector(multi) == std::string{"Col 285 Sector"});
  };

  "a distance needs both positions"_test = []
  {
    auto const d{territory::distance(std::array{0.0, 0.0, 0.0}, std::array{3.0, 4.0, 12.0})};
    expect(d.has_value() and std::abs(*d - 13.0) < 1e-9);
    expect(not territory::distance(std::nullopt, std::array{1.0, 1.0, 1.0}).has_value());
  };
  }
