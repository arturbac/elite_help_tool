#include <boost/ut.hpp>
#include <data/bgs.h>
#include <events/missions.h>
#include <star_system.h>

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
  }
