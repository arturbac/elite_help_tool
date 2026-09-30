#pragma once

#include <simple_enum/glaze_json_enum_name.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <optional>
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

///\brief a part of a line's text in a colour of its own - counted in bytes of the text
struct span_t
  {
  uint32_t from{};
  uint32_t length{};
  uint32_t color{0xffffffu};
  };

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
  ///\brief a small square before everything else, in these colours cut along the diagonal - the economies
  /// that produce a commodity; empty draws none. A field an older layer skips
  std::vector<uint32_t> swatch;
  ///\brief a dot in the square's corner - produced only at ports on the ground
  bool swatch_dot{};
  ///\brief keeps the square's room without drawing one, so that a heading of columns stands above the
  /// text of lines that have squares. A field an older layer skips
  bool swatch_space{};
  ///\brief parts of the text in colours of their own, in order and not overlapping; a line with any is not
  /// wrapped. A field an older layer skips, drawing the whole text in the line's colour
  std::vector<span_t> spans;
  ///\brief keeps the emblem's column without a marker - a list with no chart beside it needs no markers, but
  /// its names still stand in one column. A field an older layer skips, drawing the line without its emblem
  bool emblem_column{};
  ///\brief an arrow before everything else on the line, turned this many degrees clockwise from straight up -
  /// straight up being straight ahead. A field an older layer skips, drawing the line without it
  std::optional<float> pointer;
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
  ///\brief drawn as a lit ball rather than a flat disc - fields an older layer skips, drawing the flat disc
  bool sphere{};
  ///\brief where the light comes from, a direction in the picture's own coordinates - towards the star
  float light_x{-1.f};
  float light_y{};
  ///\brief how much the surface shines, 0 matte rock to 1 open water
  float gloss{};
  ///\brief lights itself, as a star does: no night side, only a darker limb
  bool glows{};
  ///\brief the body's face - a square binary PPM the tool wrote where the layer can read it, the ball
  /// filling it; a new name is a new face. The ball is drawn in color until the face is in
  std::string face;
  ///\brief the air around the ball: its colour, and how far it reaches above the surface as a part of the
  /// radius - 0 for a body without one
  uint32_t atmosphere{};
  float atmosphere_depth{};
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

///\brief a small picture drawn in a block - a binary PPM the tool wrote where the layer can read it
///\detail a new name is a new picture; the layer reads each file once, when the name first appears
struct picture_t
  {
  std::string path;
  };

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
  ///\brief drawn last, in a grid across the band; a field an older layer skips as well
  std::vector<picture_t> pictures;
  ///\brief how many pictures stand side by side
  uint32_t picture_columns{3u};
  ///\brief drawn in a column of its own beside the corner's stack, towards the middle, aligned with the
  /// same edge - a field an older layer skips, so there the block simply joins the stack
  bool beside{false};
  ///\brief stands in the middle screen rather than in its corner - centred across it, the top edge middle_y
  /// from the middle screen's centre and middle_width wide at most, both in shares of that screen's height
  /// like a cover's place. A field an older layer skips, so there the block joins its corner
  bool middle{false};
  float middle_y{};
  float middle_width{};
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
  ///\brief how much of a bright game shows through the ground, before window_alpha darkens it further
  ///\detail The ground takes each pixel g beneath it to g*(1+k) - g*g: dark space stays as it is, an ice
  /// body turns dark grey - a half-transparent ground alone left the text unreadable over one. The most
  /// that shows through is (1+k)^2/4, 0.30 at the default. Below 0 the ground is window_alpha alone
  float bright_ground{0.1f};
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
  ///\brief taken without the frame, the countdown or the word that it was - a picture the player did not
  /// ask for and should not be bothered with, such as the surface scanner's view of a planet
  bool quiet{};
  ///\brief the picture's width to its height - 1 a square, 16/9 the shape of the middle screen. A field an older
  /// layer skips, taking a square of the same height
  float aspect{1.f};
  ///\brief a rectangle of the whole surface instead of the middle, as shares of its width and height - taken
  /// when region_width is above 0. Fields an older layer skips, taking the middle
  float region_left{};
  float region_top{};
  float region_width{};
  float region_height{};
  };

///\brief a small copy of the middle of the screen the layer keeps fresh in a file shared with the tool
///\detail Shrunk on the graphics card by halving it again and again, each step the mean of 2 x 2 pixels, and
/// read out once the frame's fence has passed - the game waits for none of it. The tool watches the copy
/// for what it looks for and asks for a real picture of the part that holds it. See sample_header_t
struct sample_t
  {
  ///\brief how often a new copy is made; 0 makes none
  uint32_t every_ms{};
  ///\brief the part of the screen, as capture_t has it: its height as a share of the screen's, its shape
  float size{0.8f};
  float aspect{16.f / 9.f};
  ///\brief about how wide the copy is - the halving stops at the first width not above twice this
  uint32_t width{256u};
  };

///\brief the head of the shared file of the sample, the pixels right after it as RGBA
///\detail seq is odd while the layer writes and even once it is done; the tool copies the pixels and takes
/// them only when seq was the same even number before and after
struct sample_header_t
  {
  uint32_t magic{0x53544845u};
  uint32_t version{1u};
  uint64_t seq{};
  uint32_t width{};
  uint32_t height{};
  ///\brief the milliseconds of the system clock when the frame was drawn
  uint64_t taken_ms{};
  ///\brief where the sample lies on the whole surface, as shares of its width and height
  float left{};
  float top{};
  float region_width{};
  float region_height{};
  ///\brief the whole surface in pixels
  uint32_t surface_width{};
  uint32_t surface_height{};
  std::array<uint32_t, 4> reserved{};
  };

///\brief the largest sample the file holds, and the file's size
inline constexpr uint32_t sample_max_side{512u};
inline constexpr size_t sample_file_size{sizeof(sample_header_t) + size_t{sample_max_side} * sample_max_side * 4u};

///\brief a picture of the whole screen, overlay and all, taken by the layer when the player presses a key
///\detail The layer watches the key itself - the tool never sees the game's keyboard. It writes the picture
/// into the spool beside the socket, named screenshot_ with the moment in milliseconds, and the tool
/// files it from there
struct screenshot_t
  {
  ///\brief the X key name, as in xev or xmodmap - F11, Print, KP_Multiply; empty takes no screenshots
  std::string key{"F11"};
  };

///\brief how the layer names a screenshot in the spool, before the moment and the extension
inline constexpr std::string_view screenshot_prefix{"screenshot_"};

///\brief a patch painted over the game's own interface, to hide a mistake of the game and put it right
///\detail The place is measured on the middle screen, in shares of its height and from its centre: the game
/// draws its interface there at the ordinary shape of a monitor, so the patch stays on the same spot of
/// that interface whatever the resolution and however wide the surface beside it
struct cover_t
  {
  float x{};
  float y{};
  float width{};
  float height{};
  ///\brief painted fully opaque - the colour of what the patch stands on
  uint32_t ground{0x020304u};
  ///\brief drawn in the patch's middle, as tall as emblem_height; none leaves the patch bare
  emblem_e emblem{emblem_e::none};
  float emblem_height{};
  uint32_t emblem_color{0xffffffu};
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
  ///\brief a field an older layer skips, and simply takes no screenshots
  screenshot_t screenshot;
  ///\brief a field an older layer skips, and simply leaves the game's interface as it is
  std::vector<cover_t> covers;
  ///\brief a field an older layer skips, and simply keeps no sample
  sample_t sample;
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

///\brief where the layer writes its pictures and the tool reads them - beside the socket, so each account
/// running with a socket of its own has a spool of its own too
[[nodiscard]]
inline auto default_spool_path() -> std::string
  {
  std::string path{default_socket_path()};
  auto const slash{path.find_last_of('/')};
  path.resize(slash == std::string::npos ? 0u : slash);
  return path + "/captures";
  }

///\brief the shared file of the sample, named after the socket - two accounts playing at once each have their own
[[nodiscard]]
inline auto sample_file_path() -> std::string
  {
  std::string stem{default_socket_path()};
  if(auto const slash{stem.find_last_of('/')}; slash != std::string::npos)
    stem.erase(0, slash + 1u);
  if(auto const dot{stem.find_last_of('.')}; dot != std::string::npos)
    stem.resize(dot);
  return default_spool_path() + "/" + stem + "_sample.bin";
  }
  }  // namespace overlay
