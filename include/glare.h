#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

///\brief the white glare some Odyssey settlements fill the screen with - measured, and written down as evidence
///
/// Since an update of the game, rooms of some settlements are now and then lit so brightly that nearly the
/// whole screen is white and only a few edges show. It is to be reported to Frontier with what the network did
/// at that moment, and a tool watching the network needs to know the moment. So while the commander is inside
/// a settlement the middle of the screen is measured every few seconds, and a screen gone white is written down
/// as a marker - one JSON file of its own, with the picture beside it - into a directory another tool watches
namespace glare
  {
///\brief how bright a picture is, from its grey copy
struct metrics_t
  {
  ///\brief the average brightness, 0..1
  double luma_mean{};
  ///\brief the brightness 99 of 100 pixels stay below, 0..1
  double luma_p99{};
  ///\brief the share of the pixels at or above the burnt-out level, in percent
  double overexposed_pct{};
  };

///\brief the measure of a grey picture, 0..255 per pixel; burnt out is a pixel at or above that level
[[nodiscard]]
auto measure(std::span<uint8_t const> grey, uint8_t burnt_out) -> metrics_t;

///\brief where the marker was set, as the game's journal and Status.json say
struct game_t
  {
  std::string system;
  std::string body;
  std::string settlement;
  ///\brief the newest moment of the journal - the game's clock, not always the machine's
  std::string journal_ts;
  };

struct image_t
  {
  std::string path;
  uint32_t width{};
  uint32_t height{};
  };

///\brief one moment written down - the fields as the evidence tool reads them
struct marker_t
  {
  ///\brief the machine's clock at the moment, UTC with milliseconds and a Z
  std::string ts_utc;
  std::string source{"eht"};
  ///\brief auto-luma when measured, defect when set by hand
  std::string kind{"auto-luma"};
  std::string note;
  std::vector<std::string> screenshots;
  std::vector<image_t> images;
  metrics_t metrics;
  game_t game;
  };

///\brief a moment as the marker writes it - 2026-09-29T05:47:21.123Z
[[nodiscard]]
auto iso_utc(std::chrono::system_clock::time_point at) -> std::string;

///\brief the marker as JSON
[[nodiscard]]
auto to_json(marker_t const & marker) -> std::string;

///\brief the file's name in the markers directory - the moment, then the source: 2026-09-29T05:47:21.123Z_eht.json
[[nodiscard]]
auto file_name(marker_t const & marker) -> std::string;

///\brief whether a measured screen counts as the glare: nearly all of it burnt out
[[nodiscard]]
auto is_glare(metrics_t const & metrics, double overexposed_pct) noexcept -> bool;
  }  // namespace glare
