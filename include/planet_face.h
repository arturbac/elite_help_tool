#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
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
[[nodiscard]]
auto find_disc(image_t const & picture) -> std::optional<disc_t>;

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
  ///\brief higher is better: daylight, size up to 200 px of radius, little HUD, no streaks
  float score{};
  };

///\brief the ball in a view from the cockpit, judged; none when it is not whole and clear in it
[[nodiscard]]
auto judge_approach(image_t const & view) -> std::optional<approach_t>;

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
