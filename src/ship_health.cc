#include <ship_health.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

namespace ship_health
  {
auto repairs_hull(std::string_view journal_name) noexcept -> bool
  { return same_item("wear", journal_name); }

auto same_item(std::string_view loadout_item, std::string_view journal_name) noexcept -> bool
  {
  if(journal_name.starts_with('$'))
    journal_name.remove_prefix(1u);
  if(journal_name.ends_with(';'))
    journal_name.remove_suffix(1u);
  if(journal_name.ends_with("_name"))
    journal_name.remove_suffix(5u);
  return std::ranges::equal(
    loadout_item,
    journal_name,
    [](char a, char b) noexcept -> bool
    {
      return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
    }
  );
  }

auto frame_shift_drive(std::span<events::module_t> modules) noexcept -> events::module_t *
  {
  auto const it{std::ranges::find(modules, std::string_view{"FrameShiftDrive"}, &events::module_t::Slot)};
  return it == modules.end() ? nullptr : &*it;
  }

auto frame_shift_drive(std::span<events::module_t const> modules) noexcept -> events::module_t const *
  {
  auto const it{std::ranges::find(modules, std::string_view{"FrameShiftDrive"}, &events::module_t::Slot)};
  return it == modules.end() ? nullptr : &*it;
  }

auto wears_per_jump(std::string_view item) noexcept -> bool { return item.contains("overcharge"); }

auto fsd_reading(events::module_t const & fsd, uint32_t jumps, double wear_per_jump) noexcept -> fsd_reading_t
  {
  double const read{fsd.Health};
  if(jumps == 0u or wear_per_jump <= 0.0 or not wears_per_jump(fsd.Item))
    return {.health = read, .estimated = false};
  return {.health = std::max(0.0, read - wear_per_jump * jumps), .estimated = true};
  }

auto percent(double health) noexcept -> uint32_t
  {
  // neither lands on whole percents exactly: the journal's health is a float - 0.95 is 0.94999998 - and
  // 1 - 10 * 0.01 is a hair under 0.9. A hundredth of a percent is far above both, far below anything shown
  constexpr double rounding_slack{1e-4};
  return static_cast<uint32_t>(std::floor(std::clamp(health, 0.0, 1.0) * 100.0 + rounding_slack));
  }

auto fsd_line(fsd_reading_t reading, uint32_t jumps) -> std::string
  {
  if(reading.estimated)
    return std::format("FSD ~{}% est., {} jump{}", percent(reading.health), jumps, jumps == 1u ? "" : "s");
  return std::format("FSD {}%", percent(reading.health));
  }
  }  // namespace ship_health
