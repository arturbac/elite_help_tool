#include <boost/ut.hpp>
#include <data/bgs.h>
#include <events/missions.h>
#include <star_system.h>

#include <chrono>
#include <span>
#include <vector>

auto main() -> int
  {
  using namespace boost::ut;

  "body short name"_test = []
  {
    expect(body_short_name("Bleia Eohn QT-O d7-43", "Bleia Eohn QT-O d7-43 A 2") == "A 2");
    expect(body_short_name("Bleia Eohn QT-O d7-43", "Bleia Eohn QT-O d7-43") == "");
    // on foot in a port Status.json names the port, shorter than the system
    expect(body_short_name("Bleia Eohn QT-O d7-43", "Coppel City") == "Coppel City");
  };

  "planet of a ring"_test = []
  {
    expect(planet_name_from_ring_name("18 Camelopardalis", "18 Camelopardalis AB 3 A Ring") == "AB 3");
  };

  "state effects go to their system"_test = []
  {
    auto const ep_up{::events::state_effect_t{.Effect = "$MISSIONUTIL_Interaction_Summary_EP_up;", .Trend = "UpGood"}};
    auto const sp_down{::events::state_effect_t{.Effect = "$MISSIONUTIL_Interaction_Summary_SP_down;", .Trend = "DownBad"}};
    auto const sp_up{::events::state_effect_t{.Effect = "$MISSIONUTIL_Interaction_Summary_SP_up;", .Trend = "UpGood"}};
    auto const outbreak{::events::state_effect_t{.Effect = "$MISSIONUTIL_Interaction_Summary_Outbreak_down;", .Trend = "DownGood"}};
    ::events::influence_effect_t const system{.SystemAddress = 1u, .Trend = "UpGood", .Influence = "++"};

    // one system takes every effect, and the health bar is not counted
    ::events::faction_effect_t const single{.Faction = "a", .Effects = {ep_up, sp_down, outbreak}, .Influence = {system}};
    expect(info::state_shift(single, 0u).economy == 1_i);
    expect(info::state_shift(single, 0u).security == -1_i);

    // as many effects as systems - in order
    ::events::faction_effect_t const paired{.Faction = "a", .Effects = {sp_down, sp_up}, .Influence = {system, system}};
    expect(info::state_shift(paired, 0u).security == -1_i);
    expect(info::state_shift(paired, 1u).security == 1_i);

    // fewer - all to the first
    ::events::faction_effect_t const fewer{.Faction = "a", .Effects = {ep_up}, .Influence = {system, system}};
    expect(info::state_shift(fewer, 0u).economy == 1_i);
    expect(info::state_shift(fewer, 1u).economy == 0_i);
  };
  "a war no longer listed is over"_test = []
  {
    using std::chrono::sys_days;
    using std::chrono::days;
    using namespace std::chrono_literals;
    auto const row = [](sys_days day, std::string status, std::string f2, uint32_t won2) -> info::conflict_t
    {
      return info::conflict_t{
        .system_address = 7u,
        .timestamp = std::chrono::sys_seconds{day},
        .war_type = "war",
        .status = std::move(status),
        .faction1 = "Dukes",
        .stake1 = {},
        .won_days1 = 0u,
        .faction2 = std::move(f2),
        .stake2 = {},
        .won_days2 = won2
      };
    };
    sys_days const oct{2026y / 10 / 1};
    // the war with Cartel stopped being listed after its last active day, the one with Pethes ended in view
    std::vector<info::conflict_t> const history{
      row(oct + days{15}, "pending", "Cartel", 0u),
      row(oct + days{20}, "active", "Cartel", 1u),
      row(oct + days{24}, "active", "Pethes", 0u),
      row(oct + days{28}, "", "Pethes", 3u)
    };
    std::chrono::sys_seconds const reading{oct + days{31}};

    auto const closed{info::vanished_conflicts(history, {}, reading)};
    expect(fatal(closed.size() == 1u));
    expect(closed.front().faction2 == "Cartel");
    expect(closed.front().status.empty());
    expect(closed.front().won_days2 == 1_u);
    expect(closed.front().timestamp == reading);

    // still listed - in either order of the pair - it runs on
    std::vector<info::conflict_t> const listed{row(oct + days{31}, "active", "Cartel", 1u)};
    expect(info::vanished_conflicts(history, listed, reading).empty());
    info::conflict_t swapped{listed.front()};
    std::swap(swapped.faction1, swapped.faction2);
    expect(info::vanished_conflicts(history, std::span{&swapped, 1u}, reading).empty());

    // a reading older than the last row closes nothing
    expect(info::vanished_conflicts(history, {}, std::chrono::sys_seconds{oct + days{18}}).empty());
  };
  }
