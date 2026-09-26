#pragma once

#include <simple_enum/glaze_json_enum_name.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

///\brief the protocol between elite_help_tool and the layer drawing in the game window
///
/// the layer is deliberately dumb - it receives finished lines of text and knows nothing of the database or game logic.
/// that keeps SQLite, and any state at all, out of the game process
namespace overlay
  {
enum struct corner_e : uint8_t
  {
  top_left,
  top_right,
  bottom_left,
  bottom_right
  };

consteval auto adl_enum_bounds(corner_e)
  {
  using enum corner_e;
  return simple_enum::adl_info{top_left, bottom_right};
  }

///\brief the mark drawn in front of a name, and on that name's line in a chart
///\detail the colour alone does not separate factions of the same allegiance - three independents
/// share one colour, and only the shape tells their lines apart
enum struct marker_e : uint8_t
  {
  none,
  circle,
  diamond,
  triangle,
  square,
  cross
  };

consteval auto adl_enum_bounds(marker_e)
  {
  using enum marker_e;
  return simple_enum::adl_info{none, cross};
  }

///\brief a single line of text, the colour as 0xRRGGBB
struct line_t
  {
  std::string text;
  uint32_t color{0xffffffu};
  ///\brief drawn in the line's own colour before the text; none leaves the text where it was
  marker_e marker{marker_e::none};
  };

///\brief one point of a series, both coordinates already scaled to 0..1 by the tool
///\detail the layer applies no logarithm and knows no units - x 0 is the left edge of the window,
/// y 0 its bottom. every decision about the scale is made where the database is
struct point_t
  {
  float x{};
  float y{};
  };

///\brief one line of a chart
///\detail the legend is the block's own text lines - they carry the same colour and the same
/// marker, so the name is here only to make a captured frame readable
struct series_t
  {
  std::string name;
  uint32_t color{0xffffffu};
  marker_e marker{marker_e::circle};
  std::vector<point_t> points;
  };

///\brief a horizontal guide with its label, position as y in 0..1
struct grid_line_t
  {
  float y{};
  std::string label;
  };

///\brief a chart drawn under the block's text
struct chart_t
  {
  std::string caption;
  ///\brief the height of the plot itself in pixels at scale 1, without the legend
  uint32_t height{120u};
  std::vector<grid_line_t> grid;
  std::vector<series_t> series;
  };

///\brief the contents of one corner of the screen
struct block_t
  {
  corner_e corner{corner_e::top_left};
  ///\brief the block fades after this many milliseconds without a refresh; 0 disables fading
  uint32_t ttl_ms{};
  std::vector<line_t> lines;
  std::vector<chart_t> charts;
  };

///\brief the full image to draw - replaces the previous one entirely, only the newest counts
struct frame_t
  {
  uint64_t seq{};
  std::vector<block_t> blocks;
  };

///\brief the socket lives under $HOME, the only place visible on both sides of the pressure-vessel container
[[nodiscard]]
inline auto default_socket_path() -> std::string
  {
  if(char const * const from_env{std::getenv("EHT_OVERLAY_SOCKET")}; from_env != nullptr and *from_env != '\0')
    return from_env;

  char const * const home{std::getenv("HOME")};
  return std::string{home != nullptr ? home : "/tmp"} + "/.local/share/elite_help_tool/overlay.sock";
  }
  }  // namespace overlay
