#pragma once
#include <chrono>
#include <cstdint>
#include <string>

///\brief community goals: what the commander's percentile band earns, and how long is left
namespace community_goal
  {

///\brief the reward brackets a goal's rewards are usually given in - by percentage of contributors
enum struct bracket_e
  {
  outside,
  top_75,
  top_50
  };

///\brief the bracket of a band from the journal; 0 is a goal not contributed to yet
[[nodiscard]]
constexpr auto bracket_of(uint32_t band) noexcept -> bracket_e
  {
  if(band == 0u or band > 75u)
    return bracket_e::outside;
  if(band > 50u)
    return bracket_e::top_75;
  return bracket_e::top_50;
  }

///\brief the time left until expiry, in days and hours, or minutes in the last hour - "ended" once past
[[nodiscard]]
auto time_left(std::chrono::sys_seconds expiry, std::chrono::sys_seconds now) -> std::string;

  }  // namespace community_goal
