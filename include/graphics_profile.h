#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

///\brief which of two sets of the game's graphics fits where the ship is, against what the game has set now
///
/// The game's upscaler costs little and its edges stair-step: good enough over the ground of a settlement,
/// where frames are dear, and hard on the eyes along the long straight edges of a station or a carrier in
/// space. The game keeps its graphics in Options/Graphics/Custom.<major>.<minor>.fxcfg and writes the file
/// when the options are applied in its menu; that the game reads the file back while it runs is not known,
/// so nothing here writes it - the overlay only says which set the place wants, until the file holds it
namespace graphics_profile
  {
///\brief the three values of the file that make the difference between the sets
struct setting_t
  {
  ///\brief <UpscalingQuality>
  uint32_t upscaling{};
  ///\brief <SSAAMultiplier>: the share of the screen's width and height the world is drawn at
  double supersampling{1.0};
  ///\brief <AAMode>
  uint32_t anti_aliasing{};
  };

///\brief a set of graphics, named as the game's menu names it - the file's numbers name nothing
struct profile_t
  {
  std::string name;
  uint32_t upscaling{};
  double supersampling{1.0};
  uint32_t anti_aliasing{};
  };

///\brief the three values from the file's text, nullopt when one is missing
[[nodiscard]]
auto parse(std::string_view text) -> std::optional<setting_t>;

///\brief the file the game writes last - the one of its current version - in the directory
[[nodiscard]]
auto newest_file(std::filesystem::path const & graphics_dir) -> std::filesystem::path;

///\brief Options/Graphics of the game whose journals are in the directory: both lie in the same Wine user's
/// home, "Saved Games/Frontier Developments/Elite Dangerous" and "AppData/Local/Frontier Developments/Elite
/// Dangerous/Options/Graphics"; empty when the journals are not under "Saved Games"
[[nodiscard]]
auto graphics_dir_of(std::filesystem::path const & journal_dir) -> std::filesystem::path;

[[nodiscard]]
auto matches(setting_t const & setting, profile_t const & profile) noexcept -> bool;

enum struct place_e : uint8_t
  {
  ///\brief near a planet - orbital cruise, glide, flight over the ground, on foot or docked there
  planet,
  ///\brief anywhere else: deep space, supercruise, stations, carriers
  space
  };

///\brief from Status.json: near a planet the game gives latitude and longitude (Flags bit 21, HasLatLong) - from
/// orbital cruise down to the ground, on foot on it (Flags2 bit 4) as well
[[nodiscard]]
auto place_of(uint64_t flags, uint64_t flags2) noexcept -> place_e;

///\brief a place counts only once it has lasted a while, so one flicker of the flags does not ask for the menu
struct watch_t
  {
  std::optional<place_e> place;
  std::optional<place_e> seen;
  std::chrono::steady_clock::time_point seen_at;
  };

///\brief the place now, once it has stood for at least `wait_for`
auto settle(
  watch_t & watch, place_e now_seen, std::chrono::steady_clock::time_point now, std::chrono::milliseconds wait_for
) -> std::optional<place_e>;

///\brief the line to show: what to set for the place and what is set; nullopt when the file holds it already
[[nodiscard]]
auto hint(place_e place, setting_t const & setting, profile_t const & planet, profile_t const & space)
  -> std::optional<std::string>;
  }  // namespace graphics_profile
