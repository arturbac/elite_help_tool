#include <boost/ut.hpp>
#include <community_goal.h>
#include <events/community_goal.h>
#include <json_io.h>

auto main() -> int
  {
  using namespace boost::ut;
  using community_goal::bracket_e;
  using namespace std::chrono_literals;

  "a band falls into its reward bracket"_test = []
  {
    expect(community_goal::bracket_of(10u) == bracket_e::top_50);
    expect(community_goal::bracket_of(25u) == bracket_e::top_50);
    expect(community_goal::bracket_of(50u) == bracket_e::top_50);
    expect(community_goal::bracket_of(75u) == bracket_e::top_75);
    expect(community_goal::bracket_of(100u) == bracket_e::outside);
    expect(community_goal::bracket_of(0u) == bracket_e::outside) << "nothing contributed yet";
  };

  "the time left is told in days, hours or minutes"_test = []
  {
    std::chrono::sys_seconds const now{std::chrono::sys_days{std::chrono::year{2026} / 10 / 9} + 12h};
    expect(community_goal::time_left(now + 24h * 5 + 22h + 30min, now) == std::string{"5d 22h"});
    expect(community_goal::time_left(now + 3h + 15min, now) == std::string{"3h 15m"});
    expect(community_goal::time_left(now + 42min, now) == std::string{"42m"});
    expect(community_goal::time_left(now, now) == std::string{"ended"});
    expect(community_goal::time_left(now - 1h, now) == std::string{"ended"});
  };

  "the journal's CommunityGoal line is read"_test = []
  {
    std::string const line{
      R"({"timestamp":"2026-08-15T13:20:52Z","event":"CommunityGoal","CurrentGoals":[{"CGID":852,)"
      R"("Title":"Asura Calls for Assistance","SystemName":"Asura","MarketName":"Mizuno Dock",)"
      R"("Expiry":"2026-08-06T10:00:00Z","IsComplete":true,"CurrentTotal":11500663,"PlayerContribution":76,)"
      R"("NumContributors":5082,"TopTier":{"Name":"Tier 1","Bonus":""},"TopRankSize":10,)"
      R"("PlayerInTopRank":false,"TierReached":"Tier 1","PlayerPercentileBand":75,"Bonus":30000000}]})"
    };
    ::events::community_goal_t goal;
    expect(not eht::json::read_lenient(goal, line));
    expect(fatal(goal.CurrentGoals.size() == 1u));
    auto const & g{goal.CurrentGoals.front()};
    expect(g.CGID == 852u);
    expect(g.MarketName == std::string{"Mizuno Dock"});
    expect(g.PlayerContribution == 76u);
    expect(g.NumContributors == 5082u);
    expect(g.PlayerPercentileBand == 75u);
    expect(g.IsComplete);
    expect(g.Expiry == std::chrono::sys_seconds{std::chrono::sys_days{std::chrono::year{2026} / 8 / 6} + 10h});
  };
  }
