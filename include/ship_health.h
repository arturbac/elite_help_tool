#pragma once
#include <events/ships.h>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

///\brief the health of the ship's modules between the journal's few readings of it - Loadout gives every
/// module's, AfmuRepairs the one repaired in flight, and nothing in between
namespace ship_health
  {

///\brief Loadout names a module by its item, `int_hyperdrive_size5_class5`; AfmuRepairs and Repair spell the
/// same item `$int_hyperdrive_size5_class5_name;` - either case
[[nodiscard]]
auto same_item(std::string_view loadout_item, std::string_view journal_name) noexcept -> bool;

///\brief a Repair item that is the hull - `Wear`, in either case and spelling
[[nodiscard]]
auto repairs_hull(std::string_view journal_name) noexcept -> bool;

///\brief the frame shift drive among the modules, nullptr without one
[[nodiscard]]
auto frame_shift_drive(std::span<events::module_t> modules) noexcept -> events::module_t *;

[[nodiscard]]
auto frame_shift_drive(std::span<events::module_t const> modules) noexcept -> events::module_t const *;

///\brief a supercruise overcharge drive wears with every jump, as seen in play; no other drive is taken to
[[nodiscard]]
auto wears_per_jump(std::string_view item) noexcept -> bool;

struct fsd_reading_t
  {
  ///\brief share of full health, 0 to 1
  double health;
  ///\brief guessed from the jumps since the last reading, not read
  bool estimated;
  };

///\brief the drive's health as last read, less the wear of the jumps since for a drive that wears per jump
[[nodiscard]]
auto fsd_reading(events::module_t const & fsd, uint32_t jumps, double wear_per_jump) noexcept -> fsd_reading_t;

///\brief the health in whole percent, rounded down - 89.9 is not yet 90
[[nodiscard]]
auto percent(double health) noexcept -> uint32_t;

///\brief the overlay's line: `FSD 93%`, or `FSD ~88% est., 12 jumps`
[[nodiscard]]
auto fsd_line(fsd_reading_t reading, uint32_t jumps) -> std::string;

  }  // namespace ship_health
