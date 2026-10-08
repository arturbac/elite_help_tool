#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

///\brief journal events: community goals - the commander's standing in each one signed up for
namespace events
  {

///\brief one community goal as the game tells it - the totals of everyone and the commander's own part
struct community_goal_entry_t
  {
  uint64_t CGID{};
  std::string Title;
  std::string SystemName;
  std::string MarketName;
  std::chrono::sys_seconds Expiry;
  bool IsComplete{};
  uint64_t CurrentTotal{};
  uint64_t PlayerContribution{};
  uint64_t NumContributors{};
  std::string TierReached;
  bool PlayerInTopRank{};
  ///\brief the best percentage of contributors the commander is in: 10, 25, 50, 75 or 100
  uint32_t PlayerPercentileBand{};
  };

///\brief every community goal the commander takes part in - written at login and on opening a goal's panel
struct community_goal_t
  {
  std::vector<community_goal_entry_t> CurrentGoals;
  };

  }  // namespace events
