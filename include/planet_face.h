#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

///\brief the face of a planet for the overlay's balls, dug out of the pictures the tool already takes
///
/// The surface scanner shows the body as a ball lit evenly all over - nearly its bare colours, which is
/// what a ball shaded by the overlay wants. The scanner's pictures are not all of use: in most a filter
/// paints the regions of a genus in cyan, in some the probes streak across the ball, some are too close
/// for the ball to fit. So the ball is found in every picture, the ones it is whole and clear in are
/// kept, and the least tinted of them gives the face. When every one is tinted, only the relief is
/// taken from it and the colour from elsewhere - the sky album's photo of the body, or its class.
namespace planet_face
  {
///\brief a picture as plain rows of RGB, 3 bytes a pixel
struct image_t
  {
  uint32_t width{};
  uint32_t height{};
  std::vector<uint8_t> rgb;

  [[nodiscard]]
  auto empty() const noexcept -> bool
    { return width == 0u or height == 0u or rgb.size() < size_t{width} * height * 3u; }
  };

///\brief the ball found in a picture, in its pixels
struct disc_t
  {
  float x{};
  float y{};
  float radius{};
  ///\brief how much of the rim shows as an edge, 0..1 - a ball the scanner draws whole is near 1
  float rim{};
  ///\brief the whole ball lies within the picture
  bool inside{};
  };

///\brief the largest clear circle in the picture - the ball, where there is one
///\detail looked for on a copy about 400 pixels across: every edge votes for the centres its gradient
/// points at, the best centre takes the radius most of its edges stand at
///\param past_hud edges in or beside the HUD's cyan and orange do not vote - for views from the cockpit, where
/// the target's ring stands on the ball
[[nodiscard]]
auto find_disc(image_t const & picture, bool past_hud = false) -> std::optional<disc_t>;

///\brief what a ball looks like in a picture, over the ball's middle
struct look_t
  {
  ///\brief how much bluer than it is red and green
  float blue{};
  ///\brief the share of cyan pixels - a filter paints its regions cyan, a ball in its own colours has
  /// hardly any, only the scanner's rings
  float cyan{};
  ///\brief the share of pixels much brighter than the ball's own - a probe's streak
  float streaks{};
  ///\brief the mean colour, 0..1
  float red{};
  float green{};
  float blue_mean{};
  };

///\brief the ball cut out and resampled to side x side: the ball fills the square, outside it stays black,
/// and the scanner's own marks - its white dot, its needle, its cyan rings - are filled from around them
[[nodiscard]]
auto cut_face(image_t const & picture, disc_t const & disc, uint32_t side) -> image_t;

[[nodiscard]]
auto look_of(image_t const & face) -> look_t;

///\brief the colour of the body's lit side in a photo of it, 0..1 each
[[nodiscard]]
auto lit_colour(image_t const & photo) -> std::optional<std::array<float, 3>>;

///\brief the face's relief kept, its colour replaced - for a face tinted by a filter
[[nodiscard]]
auto retint(image_t face, std::array<float, 3> colour) -> image_t;

///\brief a view from the cockpit on the way to the body, as good as it is for a face
struct approach_t
  {
  disc_t disc;
  ///\brief the share of the ball in daylight - the star behind the ship lights it all
  float lit{};
  ///\brief the share of it under the ship's HUD, its saturated orange
  float hud{};
  ///\brief higher is better: daylight, little HUD, no streaks - the size counts no more once it is full
  float score{};
  };

///\brief the radius on the screen from which a ball is large enough for a face, in screen pixels
inline constexpr float full_radius{110.f};

///\brief the top of the cockpit's dashboard as a share of the screen's height - below it the target's hologram
/// is a ball too, lit and textured, and would be taken for the planet
inline constexpr float dashboard_top{0.7f};

///\brief the view without its rows below dashboard_top on the screen - the part where a planet can be seen
///\param top the view's top as a share of the screen's height
///\param height the view's height as a share of the screen's height
[[nodiscard]]
auto above_dashboard(image_t view, float top, float height) -> image_t;

///\brief the ball in a view from the cockpit, judged; none when it is not whole and clear in it, or smaller
/// on the screen than full_size
///\param pixel_scale screen pixels to a pixel of the view - above 1 for a small sample, so that its ball is
/// judged at the size it has on the screen
///\param full_size the ball's radius on the screen from which it is taken; larger is no better
///\param why when given, says what a view was turned down for - for the log, which otherwise shows only views kept
[[nodiscard]]
auto judge_approach(image_t const & view, float pixel_scale = 1.f, float full_size = full_radius, std::string * why = nullptr)
  -> std::optional<approach_t>;

///\brief the ball of a cockpit view as a face, its light taken off: the direction of the light is fitted to
/// the brightness over the ball, each pixel divided by how squarely it faced the light, and the night side
/// filled from the day side mirrored across the terminator
[[nodiscard]]
auto approach_face(image_t const & view, disc_t const & disc, uint32_t side) -> image_t;

///\brief the face made of what there is, best first: a scanner view in the body's own colours; a face from
/// the cockpit (approach, already side x side, may be null); a scanner view tinted by a filter, its colour
/// from the photo or else the fallback. Empty when nothing shows the ball clear
[[nodiscard]]
auto make_face(
  std::span<image_t const> views,
  image_t const * photo,
  image_t const * approach,
  std::array<float, 3> fallback,
  uint32_t side
) -> image_t;
  }  // namespace planet_face
