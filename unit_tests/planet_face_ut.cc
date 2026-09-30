#include <planet_face.h>

#include <boost/ut.hpp>

#include <cmath>
#include <cstdlib>

using namespace boost::ut;

namespace
  {
///\brief a ball of one colour on a sky of scattered stars, with a white dot in its middle as the scanner draws
[[nodiscard]]
auto ball_on_stars(uint32_t width, uint32_t height, float x, float y, float r, std::array<uint8_t, 3> colour)
  -> planet_face::image_t
  {
  planet_face::image_t picture{
    .width = width, .height = height, .rgb = std::vector<uint8_t>(size_t{width} * height * 3u, 8u)
  };
  std::srand(7u);
  for(int star{}; star != int(width * height / 150u); ++star)
    {
    size_t const at{size_t(std::rand()) % (size_t{width} * height)};
    for(size_t c{}; c != 3u; ++c)
      picture.rgb[at * 3u + c] = 230u;
    }
  for(uint32_t py{}; py != height; ++py)
    for(uint32_t px{}; px != width; ++px)
      {
      float const dx{float(px) - x};
      float const dy{float(py) - y};
      float const d{std::hypot(dx, dy)};
      if(d > r)
        continue;
      uint8_t * const p{picture.rgb.data() + (size_t{py} * width + px) * 3u};
      bool const dot{d < r * 0.04f};
      for(size_t c{}; c != 3u; ++c)
        p[c] = dot ? 255u : colour[c];
      }
  return picture;
  }
  }  // namespace

auto main() -> int
  {
  "the ball is found where it lies"_test = []
  {
    auto const picture{ball_on_stars(800u, 600u, 430.f, 280.f, 150.f, {150u, 130u, 170u})};
    auto const disc{planet_face::find_disc(picture)};
    expect(disc.has_value()) << "no ball found";
    if(disc)
      {
      expect(std::abs(disc->x - 430.f) < 6.f) << disc->x;
      expect(std::abs(disc->y - 280.f) < 6.f) << disc->y;
      expect(std::abs(disc->radius - 150.f) < 6.f) << disc->radius;
      expect(disc->rim > 0.8f) << "a whole ball shows its whole rim";
      expect(disc->inside);
      }
  };

  "the target's cyan ring on the ball is not taken for a small ball"_test = []
  {
    auto picture{ball_on_stars(800u, 600u, 400.f, 300.f, 200.f, {150u, 140u, 150u})};
    // a thin cyan ring near the middle, as the HUD draws round the target
    for(uint32_t py{}; py != 600u; ++py)
      for(uint32_t px{}; px != 800u; ++px)
        if(float const d{std::hypot(float(px) - 405.f, float(py) - 300.f)}; std::abs(d - 30.f) < 1.5f)
          {
          uint8_t * const p{picture.rgb.data() + (size_t{py} * 800u + px) * 3u};
          p[0] = 30u;
          p[1] = 160u;
          p[2] = 255u;
          }
    auto const disc{planet_face::find_disc(picture, true)};
    expect(disc.has_value() and std::abs(disc->radius - 200.f) < 8.f) << (disc ? disc->radius : 0.f);
  };

  "a ball cut by the edge is not inside"_test = []
  {
    auto const picture{ball_on_stars(600u, 600u, 80.f, 300.f, 150.f, {150u, 130u, 170u})};
    auto const disc{planet_face::find_disc(picture)};
    expect(disc.has_value() and not disc->inside);
  };

  "the face keeps the ball's colour and loses the scanner's dot"_test = []
  {
    // greyish-violet, not blue enough to be taken for a filter
    auto const picture{ball_on_stars(800u, 600u, 400.f, 300.f, 160.f, {150u, 140u, 150u})};
    std::array const views{picture};
    auto const face{planet_face::make_face(views, nullptr, nullptr, {0.5f, 0.5f, 0.5f}, 64u)};
    expect(face.width == 64_u and face.height == 64_u);
    expect(not face.empty());
    if(not face.empty())
      {
      uint8_t const * const middle{face.rgb.data() + (size_t{32u} * 64u + 32u) * 3u};
      expect(std::abs(int(middle[0]) - 150) < 12) << "the dot is filled from the ball around it" << int(middle[0]);
      expect(std::abs(int(middle[1]) - 140) < 12);
      uint8_t const * const corner{face.rgb.data()};
      expect(corner[0] == 0u and corner[1] == 0u) << "outside the ball stays black";
      }
  };

  "a view tinted blue takes its colour from the photo"_test = []
  {
    auto const tinted{ball_on_stars(800u, 600u, 400.f, 300.f, 160.f, {40u, 140u, 220u})};
    auto const photo{ball_on_stars(800u, 600u, 400.f, 300.f, 100.f, {200u, 120u, 80u})};
    std::array const views{tinted};
    auto const face{planet_face::make_face(views, &photo, nullptr, {0.5f, 0.5f, 0.5f}, 64u)};
    expect(not face.empty());
    if(not face.empty())
      {
      uint8_t const * const p{face.rgb.data() + (size_t{20u} * 64u + 32u) * 3u};
      expect(p[0] > p[2]) << "the photo's warm colour, not the filter's blue";
      }
  };

  "a ball lit from the side is judged by its daylight, and its face loses the shadow"_test = []
  {
    // albedo of one colour, lit from the upper left and a little from the front, the night side black
    uint32_t const w{800u};
    uint32_t const h{600u};
    planet_face::image_t view{.width = w, .height = h, .rgb = std::vector<uint8_t>(size_t{w} * h * 3u, 6u)};
    float const lx{-0.6f};
    float const ly{-0.3f};
    float const lz{0.74f};
    for(uint32_t py{}; py != h; ++py)
      for(uint32_t px{}; px != w; ++px)
        {
        float const u{(float(px) - 400.f) / 160.f};
        float const v{(float(py) - 300.f) / 160.f};
        if(u * u + v * v > 1.f)
          continue;
        float const z{std::sqrt(1.f - u * u - v * v)};
        float const facing{std::max(0.f, u * lx + v * ly + z * lz)};
        uint8_t * const p{view.rgb.data() + (size_t{py} * w + px) * 3u};
        std::array<float, 3> const albedo{200.f, 150.f, 120.f};
        for(size_t c{}; c != 3u; ++c)
          p[c] = uint8_t(std::lround(albedo[c] * facing));
        }
    auto const judged{planet_face::judge_approach(view)};
    expect(judged.has_value()) << "the lit ball is found";
    if(not judged)
      return;
    expect(judged->lit > 0.5f and judged->lit < 0.95f) << judged->lit;
    expect(judged->hud < 0.01f);
    // the same ball as a sample shrunk to half: 80 px on the screen, still too far for a face
    expect(not planet_face::judge_approach(view, 0.5f).has_value());
    // twice as large on the screen is no better
    auto const larger{planet_face::judge_approach(view, 2.f)};
    expect(larger.has_value() and std::abs(larger->score - judged->score) < 1e-4f);
    auto const face{planet_face::approach_face(view, judged->disc, 64u)};
    // the lit middle, a point near the terminator and one on the night side all come out alike
    auto const at = [&](uint32_t x, uint32_t y) { return face.rgb.data() + (size_t{y} * 64u + x) * 3u; };
    int const lit_red{at(24u, 28u)[0]};
    int const edge_red{at(44u, 36u)[0]};
    int const night_red{at(52u, 44u)[0]};
    expect(std::abs(edge_red - lit_red) < lit_red / 4) << lit_red << edge_red;
    expect(std::abs(night_red - lit_red) < lit_red / 4) << lit_red << night_red;
    expect(at(24u, 28u)[0] > at(24u, 28u)[2]) << "the colour stays warm";
  };

  "no ball, no face"_test = []
  {
    planet_face::image_t const stars{ball_on_stars(400u, 300u, -500.f, -500.f, 10.f, {0u, 0u, 0u})};
    std::array const views{stars};
    auto const face{planet_face::make_face(views, nullptr, nullptr, {0.5f, 0.5f, 0.5f}, 64u)};
    expect(face.empty());
  };
  }
