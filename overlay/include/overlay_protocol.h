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
  bottom_right,
  ///\brief the upper part of the middle screen, to the left of the centre line
  ///\detail for what has to be read without looking away from the fight. It flanks the centre rather
  /// than sitting at the screen's edge, because on a triple screen the edges are where one does not
  /// look - and it stays clear of the middle itself, which is where the fight is
  centre_top_left,
  ///\brief the same, to the right of the centre line
  centre_top_right
  };

consteval auto adl_enum_bounds(corner_e)
  {
  using enum corner_e;
  return simple_enum::adl_info{top_left, centre_top_right};
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

///\brief the superpower a faction answers to, drawn as its emblem
///\detail the emblem says the allegiance, which several factions share; the marker above says which
/// faction this is. They sit side by side because neither answers the other's question
enum struct emblem_e : uint8_t
  {
  none,
  federation,
  empire,
  alliance
  };

consteval auto adl_enum_bounds(emblem_e)
  {
  using enum emblem_e;
  return simple_enum::adl_info{none, alliance};
  }

///\brief where the faction went at the last recalculation
///\detail unknown is not the same as flat: flat says the faction held its ground through a tick we
/// actually saw, unknown says we have not looked at the system since the tick came
enum struct trend_e : uint8_t
  {
  unknown,
  up,
  flat,
  down
  };

consteval auto adl_enum_bounds(trend_e)
  {
  using enum trend_e;
  return simple_enum::adl_info{unknown, down};
  }

///\brief a single line of text, the colour as 0xRRGGBB
struct line_t
  {
  std::string text;
  uint32_t color{0xffffffu};
  ///\brief drawn in the line's own colour before the text; none leaves the text where it was
  marker_e marker{marker_e::none};
  ///\brief drawn after the marker, tinted with the line's colour
  emblem_e emblem{emblem_e::none};
  ///\brief drawn between the text and the suffix
  trend_e trend{trend_e::unknown};
  ///\brief what follows the trend mark - it is a field of its own only so the mark can stand
  /// between the value it speaks about and whatever comes after it
  std::string suffix;
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

///\brief a filled circle, or a ring when outline is set - in pixels at scale 1, like the whole diagram
struct disc_t
  {
  float x{};
  float y{};
  float radius{};
  uint32_t color{0xffffffu};
  bool outline{};
  };

///\brief a straight stroke between two points
///\detail with relative set the points are offsets from (ax, ay) and keep their shape - the stroke
/// belongs to a symbol - while plain strokes stretch with the picture, as a connecting line should
struct segment_t
  {
  float x0{};
  float y0{};
  float x1{};
  float y1{};
  uint32_t color{0x808080u};
  bool relative{};
  float ax{};
  float ay{};
  };

///\brief a piece of text anchored at a point - align 0 puts the point at its left edge, 0.5 in its
/// middle, 1 at its right; vertically the text is always centred on the point
struct label_t
  {
  float x{};
  float y{};
  std::string text;
  uint32_t color{0xbbbbbbu};
  float align{};
  };

///\brief a picture made of plain shapes, laid out by the tool
///\detail the coordinates are pixels at scale 1 with y growing downwards. The layer spreads the
/// picture across the whole band and enlarges it upright by zoom, so every row adds the same height;
/// sizes - radii, symbols, text - follow the upright factor, so a disc stays round however wide the
/// band. It knows nothing of what the shapes stand for
struct diagram_t
  {
  float width{};
  float height{};
  ///\brief how much larger than the rest of the overlay the picture is drawn upright
  float zoom{1.f};
  ///\brief what part of the band's width the picture takes, 0..1 - the tool decides, so the
  /// proportions can be changed without installing the layer again
  float share{1.f};
  std::vector<segment_t> segments;
  std::vector<disc_t> discs;
  std::vector<label_t> labels;
  };

///\brief how large a block's text is set
///\detail a new field, not a new value of an existing enumeration - an older layer skips a field it
/// does not know and goes on drawing, where an unknown enumerator would cost it the whole frame
enum struct text_e : uint8_t
  {
  normal,
  ///\brief for the blocks that are long lists rather than glances
  small
  };

consteval auto adl_enum_bounds(text_e)
  {
  using enum text_e;
  return simple_enum::adl_info{normal, small};
  }

///\brief the contents of one corner of the screen
struct block_t
  {
  corner_e corner{corner_e::top_left};
  ///\brief the block fades after this many milliseconds without a refresh; 0 disables fading
  uint32_t ttl_ms{};
  std::vector<line_t> lines;
  std::vector<chart_t> charts;
  text_e text{text_e::normal};
  ///\brief drawn after the charts; a field an older layer skips, so it simply goes without the picture
  std::vector<diagram_t> diagrams;
  };

///\brief how the layer lays the overlay out - sent by the tool with every frame
///\detail the layer keeps no settings of its own: these come from the tool's settings file, so one file
/// shapes everything and a change there shows without touching the game. A zero means "work it out
/// from the screen", which is what the layer did before anyone told it anything. The defaults are
/// what it draws with until the first frame arrives
struct layout_t
  {
  ///\brief the text size as a multiple of 13 pixels; 0 follows the screen's height (height / 780)
  float scale{};
  ///\brief the small text of the long lists against the ordinary one
  float small_text{0.75f};
  ///\brief the width of the middle screen in pixels; 0 derives it from the height at 16:9
  float centre_width{};
  ///\brief the width of the side bands in pixels; 0 takes the whole screen beside the middle one
  float side_width{};
  ///\brief the line the head-up readouts stand on, as a share of the screen's height
  float hud_bottom{0.33f};
  ///\brief how far either side of the centre the head-up readouts stand, as a share of the middle screen
  float hud_gap{0.245f};
  ///\brief the widest a head-up readout grows, as a share of the middle screen
  float hud_width{0.22f};
  ///\brief the gap between a corner block and the screen's edge, in pixels at scale 1
  float corner_margin{14.f};
  ///\brief how opaque the ground under the blocks is, 0..1
  float window_alpha{0.35f};
  ///\brief the widest a chart is drawn, in pixels at scale 1
  float chart_width{360.f};
  ///\brief the layer's own line - frame rate and connection - at the top of the right band
  bool stats{true};
  };

///\brief a picture of the middle of the screen, asked of the layer
///\detail the tool asks when a sample of a plant is taken - the commander is looking straight at it then.
/// The request rides with every frame until the next one replaces it, so it is the change of the number
/// that asks, never its presence. A layer that joins late takes the number it first sees as already
/// served - otherwise a restarted game would photograph whatever it shows the moment it connects
struct capture_t
  {
  ///\brief 0 asks for nothing; every new number asks for one picture
  uint64_t id{};
  ///\brief where to write it - a binary PPM, put under this name only once complete
  std::string path;
  ///\brief the side of the square, as a share of the screen's height
  float size{0.6f};
  ///\brief how long the layer holds the picture back, showing the player the frame meanwhile - the scan's
  /// own rings and the sampler sealing the sample are on the screen right after it
  uint32_t delay_ms{};
  };

///\brief the full image to draw - replaces the previous one entirely, only the newest counts
struct frame_t
  {
  uint64_t seq{};
  std::vector<block_t> blocks;
  ///\brief a field an older layer skips; a frame without it leaves the layer on its defaults
  layout_t layout;
  ///\brief a field an older layer skips, and simply takes no pictures
  capture_t capture;
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
