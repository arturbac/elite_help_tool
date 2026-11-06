#pragma once

#include <chrono>
#include <cmath>
#include <optional>
#include <tuple>

///\brief the order the overlay lists the open missions in
///
/// The list is cut short when there are many, so what comes first is what stays in sight: the work still to do
/// before what only waits to be handed in, and of each the nearest to where the commander is now - a list by
/// expiry alone showed the same rows wherever the ship went.
namespace mission_order
  {
///\brief how far a mission's place is from the commander; what is not known of it stands last
struct distance_t
  {
  std::optional<double> ly;
  std::optional<double> ls;
  };

struct key_t
  {
  ///\brief done, waiting only to be handed in
  bool done{};
  distance_t away;
  std::chrono::sys_seconds expiry{};
  };

///\brief how far the place is along the Ls - from the commander in this system, from the arrival star in another
[[nodiscard]]
inline auto ls_away(std::optional<double> place_ls, bool this_system, std::optional<double> here_ls)
  -> std::optional<double>
  {
  if(place_ls and this_system and here_ls)
    return std::abs(*place_ls - *here_ls);
  return place_ls;
  }

[[nodiscard]]
inline auto before(key_t const & a, key_t const & b) -> bool
  {
  auto const rank = [](key_t const & key)
  {
    return std::tuple{
      key.done,
      not key.away.ly.has_value(),
      key.away.ly.value_or(0.0),
      not key.away.ls.has_value(),
      key.away.ls.value_or(0.0),
      key.expiry
    };
  };
  return rank(a) < rank(b);
  }
  }  // namespace mission_order
