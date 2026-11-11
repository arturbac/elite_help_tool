#include <galaxy_region.h>
#include <galaxy_regions_data.h>

#include <cmath>

namespace galaxy_region
  {
namespace
  {
  // the map's corner, and its cells: 2048 a side, 4096/83 light years each
  constexpr double x0{-49985.0};
  constexpr double z0{-24105.0};
  constexpr double cells_per_ly{83.0 / 4096.0};
  }  // namespace

auto at(double x, double z) noexcept -> std::string_view
  {
  double const px{std::floor((x - x0) * cells_per_ly)};
  double const pz{std::floor((z - z0) * cells_per_ly)};
  if(not(px >= 0.0 and pz >= 0.0 and pz < 2048.0 and px < 2048.0))
    return {};
  auto const row{static_cast<size_t>(pz)};
  auto const cell{static_cast<uint32_t>(px)};
  uint32_t passed{};
  for(uint32_t ix{galaxy_regions_data::row_starts[row]}; ix != galaxy_regions_data::row_starts[row + 1u]; ++ix)
    {
    galaxy_regions_data::run_t const run{galaxy_regions_data::runs[ix]};
    if(cell < passed + run.length)
      return galaxy_regions_data::names[run.region];
    passed += run.length;
    }
  return {};
  }

auto of_address(uint64_t system_address) noexcept -> std::string_view
  {
  // the boxel's corner, in tens of light years from the galaxy's corner: z in the lowest bits after the mass code,
  // then y, then x, each the shorter the larger the boxel
  unsigned const mass{static_cast<unsigned>(system_address & 7u)};
  uint64_t const z{((system_address >> 3u) & (0x3FFFu >> mass)) << mass};
  uint64_t const x{((system_address >> (30u - mass * 2u)) & (0x3FFFu >> mass)) << mass};
  return at(double(x) * 10.0 + x0, double(z) * 10.0 + z0);
  }
  }  // namespace galaxy_region
