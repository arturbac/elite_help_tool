#pragma once

#include <cstdint>
#include <string_view>

///\brief the galactic region a system stands in - the one the game names in a codex entry
///
/// The journal names a region only in a codex entry. The region map of EliteDangerousRegionMap (MIT, see
/// galaxy_regions_data.h) gives it for any system: a system's address names the boxel it stands in, and the
/// region of the boxel's corner is the system's - on 195 systems of Sjona's codex entries it gave the game's every
/// time, where the star's own position missed two by a border
namespace galaxy_region
  {
///\brief the region by the system's address, empty outside the map
[[nodiscard]]
auto of_address(uint64_t system_address) noexcept -> std::string_view;

///\brief the region at a point of the galaxy's plane, in light years from Sol; empty outside the map
[[nodiscard]]
auto at(double x, double z) noexcept -> std::string_view;
  }  // namespace galaxy_region
