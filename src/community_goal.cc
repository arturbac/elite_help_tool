#include <community_goal.h>
#include <format>

namespace community_goal
  {
auto time_left(std::chrono::sys_seconds expiry, std::chrono::sys_seconds now) -> std::string
  {
  if(expiry <= now)
    return "ended";
  auto const left{expiry - now};
  auto const days{std::chrono::floor<std::chrono::days>(left)};
  auto const hours{std::chrono::floor<std::chrono::hours>(left - days)};
  if(days.count() > 0)
    return std::format("{}d {}h", days.count(), hours.count());
  if(hours.count() > 0)
    return std::format("{}h {}m", hours.count(), std::chrono::floor<std::chrono::minutes>(left - hours).count());
  return std::format("{}m", std::chrono::floor<std::chrono::minutes>(left).count());
  }
  }  // namespace community_goal
