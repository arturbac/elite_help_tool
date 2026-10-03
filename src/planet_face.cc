#include <planet_face.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>

namespace planet_face
  {
namespace
  {
  ///\brief how large the copy the ball is looked for on is, across its longer side
  constexpr uint32_t working_size{400u};

  ///\brief a grey picture of floats, 0..1
  struct grey_t
    {
    uint32_t width{};
    uint32_t height{};
    std::vector<float> value;

    [[nodiscard]]
    auto at(uint32_t x, uint32_t y) const -> float
      { return value[size_t{y} * width + x]; }
    };

  ///\brief the picture shrunk to working size by averaging the pixels each new one covers
  [[nodiscard]]
  auto shrink(image_t const & picture, float & scale) -> grey_t
    {
    uint32_t const longer{std::max(picture.width, picture.height)};
    scale = std::min(1.f, float(working_size) / float(longer));
    grey_t grey{
      .width = std::max(1u, uint32_t(std::lround(float(picture.width) * scale))),
      .height = std::max(1u, uint32_t(std::lround(float(picture.height) * scale))),
      .value = {}
    };
    grey.value.assign(size_t{grey.width} * grey.height, 0.f);
    std::vector<uint32_t> count(grey.value.size(), 0u);
    for(uint32_t y{}; y != picture.height; ++y)
      {
      uint32_t const gy{std::min(grey.height - 1u, uint32_t(float(y) * scale))};
      uint8_t const * row{picture.rgb.data() + size_t{y} * picture.width * 3u};
      for(uint32_t x{}; x != picture.width; ++x)
        {
        uint32_t const gx{std::min(grey.width - 1u, uint32_t(float(x) * scale))};
        size_t const at{size_t{gy} * grey.width + gx};
        grey.value[at] += float(row[x * 3u] + row[x * 3u + 1u] + row[x * 3u + 2u]) / (3.f * 255.f);
        ++count[at];
        }
      }
    for(size_t ix{}; ix != grey.value.size(); ++ix)
      if(count[ix] != 0u)
        grey.value[ix] /= float(count[ix]);
    return grey;
    }

  ///\brief each pixel with its four neighbours - the stars go soft, the rim stays
  auto soften(std::vector<float> & value, uint32_t width, uint32_t height) -> void
    {
    std::vector<float> const source{value};
    for(uint32_t y{1u}; y + 1u < height; ++y)
      for(uint32_t x{1u}; x + 1u < width; ++x)
        {
        size_t const at{size_t{y} * width + x};
        value[at] = (source[at] + source[at - 1u] + source[at + 1u] + source[at - width] + source[at + width]) / 5.f;
        }
    }

  [[nodiscard]]
  auto luminance(uint8_t const * px) -> float
    { return float(px[0] + px[1] + px[2]) / (3.f * 255.f); }

  [[nodiscard]]
  auto is_hud(uint8_t const * p) -> bool;
  [[nodiscard]]
  auto is_cyan(uint8_t const * p) -> bool;

  ///\brief the pixels of the working copy the HUD's colours fall on, and those around them
  [[nodiscard]]
  auto hud_mask(image_t const & picture, grey_t const & grey, float scale) -> std::vector<bool>
    {
    std::vector<bool> marked(grey.value.size(), false);
    for(uint32_t y{}; y != picture.height; ++y)
      for(uint32_t x{}; x != picture.width; ++x)
        if(uint8_t const * const p{picture.rgb.data() + (size_t{y} * picture.width + x) * 3u}; is_hud(p) or is_cyan(p))
          marked
            [size_t{std::min(grey.height - 1u, uint32_t(float(y) * scale))} * grey.width
             + std::min(grey.width - 1u, uint32_t(float(x) * scale))] = true;
    // grown by two, for the soft edge the ring leaves on the pixels beside it
    for(int pass{}; pass != 2; ++pass)
      {
      std::vector<bool> const source{marked};
      for(uint32_t y{1u}; y + 1u < grey.height; ++y)
        for(uint32_t x{1u}; x + 1u < grey.width; ++x)
          if(
            size_t const at{size_t{y} * grey.width + x};
            source[at - 1u] or source[at + 1u] or source[at - grey.width] or source[at + grey.width]
          )
            marked[at] = true;
      }
    return marked;
    }
  }  // namespace

auto find_disc(image_t const & picture, bool past_hud) -> std::optional<disc_t>
  {
  if(picture.empty())
    return std::nullopt;
  float scale{};
  grey_t grey{shrink(picture, scale)};
  uint32_t const w{grey.width};
  uint32_t const h{grey.height};
  if(w < 16u or h < 16u)
    return std::nullopt;
  std::vector<bool> const hud{past_hud ? hud_mask(picture, grey, scale) : std::vector<bool>{}};
  soften(grey.value, w, h);

  // Sobel
  std::vector<float> gx(grey.value.size(), 0.f);
  std::vector<float> gy(grey.value.size(), 0.f);
  std::vector<float> magnitude(grey.value.size(), 0.f);
  for(uint32_t y{1u}; y + 1u < h; ++y)
    for(uint32_t x{1u}; x + 1u < w; ++x)
      {
      size_t const at{size_t{y} * w + x};
      gx[at] = (grey.at(x + 1u, y) - grey.at(x - 1u, y)) * 2.f + (grey.at(x + 1u, y - 1u) - grey.at(x - 1u, y - 1u))
               + (grey.at(x + 1u, y + 1u) - grey.at(x - 1u, y + 1u));
      gy[at] = (grey.at(x, y + 1u) - grey.at(x, y - 1u)) * 2.f + (grey.at(x - 1u, y + 1u) - grey.at(x - 1u, y - 1u))
               + (grey.at(x + 1u, y + 1u) - grey.at(x + 1u, y - 1u));
      magnitude[at] = std::hypot(gx[at], gy[at]);
      }

  // the strongest eighth of the edges, and only sharp ones: the smooth shading of a ball lit from the side
  // is a gradient too, and every bit of it points at the spot under the star, not at the middle
  std::vector<float> sorted{magnitude};
  auto const nth{sorted.begin() + std::ptrdiff_t(sorted.size() * 92u / 100u)};
  std::nth_element(sorted.begin(), nth, sorted.end());
  float const eighth{*nth};
  auto const top{sorted.begin() + std::ptrdiff_t(sorted.size() * 995u / 1000u)};
  std::nth_element(sorted.begin(), top, sorted.end());
  float const threshold{std::max({eighth, *top * 0.3f, 1e-3f})};

  struct edge_t
    { float x, y, ux, uy; };

  std::vector<edge_t> edges;
  for(uint32_t y{1u}; y + 1u < h; ++y)
    for(uint32_t x{1u}; x + 1u < w; ++x)
      if(size_t const at{size_t{y} * w + x}; magnitude[at] > threshold and (hud.empty() or not hud[at]))
        edges.push_back(edge_t{float(x), float(y), gx[at] / magnitude[at], gy[at] / magnitude[at]});

  int const shorter{int(std::min(w, h))};
  int const r_min{std::max(3, shorter * 6 / 100)};
  int const r_max{shorter * 75 / 100};

  // every edge votes for the centres along its gradient, both ways - the ball may be lighter or darker
  std::vector<float> votes(grey.value.size(), 0.f);
  for(edge_t const & e: edges)
    for(float const sign: {1.f, -1.f})
      for(int r{r_min}; r < r_max; ++r)
        {
        int const cx{int(std::lround(e.x + sign * e.ux * float(r)))};
        int const cy{int(std::lround(e.y + sign * e.uy * float(r)))};
        if(cx < 0 or cy < 0 or cx >= int(w) or cy >= int(h))
          break;
        votes[size_t(cy) * w + size_t(cx)] += 1.f;
        }
  soften(votes, w, h);
  soften(votes, w, h);
  // the circle centred on a peak of the votes
  auto const circle_at = [&](size_t centre) -> std::optional<disc_t>
  {
    float const cx{float(centre % w)};
    float const cy{float(centre / w)};

    // the radius most edges pointing along it stand at, per unit of rim - a large circle is not better for being long
    std::vector<float> at_radius(size_t(r_max) + 2u, 0.f);
    for(edge_t const & e: edges)
      {
      float const dx{e.x - cx};
      float const dy{e.y - cy};
      float const d{std::hypot(dx, dy) + 1e-6f};
      if(std::abs((dx * e.ux + dy * e.uy) / d) <= 0.9f)
        continue;
      if(auto const bin{size_t(std::lround(d))}; bin < at_radius.size())
        at_radius[bin] += 1.f;
      }
    int radius{};
    float score{};
    for(int r{r_min}; r <= r_max; ++r)
      if(float const s{at_radius[size_t(r)] / float(r)}; s > score)
        {
        score = s;
        radius = r;
        }
    if(radius == 0)
      return std::nullopt;

    // how much of the rim shows: of 90 arcs, those with an edge on the circle
    std::array<bool, 90> arcs{};
    for(edge_t const & e: edges)
      {
      float const dx{e.x - cx};
      float const dy{e.y - cy};
      float const d{std::hypot(dx, dy) + 1e-6f};
      if(std::abs(d - float(radius)) >= 2.5f or std::abs((dx * e.ux + dy * e.uy) / d) <= 0.9f)
        continue;
      auto const arc{
        size_t((std::atan2(dy, dx) + std::numbers::pi_v<float>) / (2.f * std::numbers::pi_v<float>)*90.f) % 90u
      };
      arcs[arc] = true;
      }
    float const rim{float(std::ranges::count(arcs, true)) / 90.f};
    float const r{float(radius)};
    return disc_t{
      .x = (cx + 0.5f) / scale,
      .y = (cy + 0.5f) / scale,
      .radius = r / scale,
      .rim = rim,
      .inside = cx - r >= 0.f and cy - r >= 0.f and cx + r < float(w) and cy + r < float(h)
    };
  };

  auto const best{std::ranges::max_element(votes)};
  if(*best <= 0.f)
    return std::nullopt;
  auto const first{circle_at(size_t(best - votes.begin()))};
  // from the cockpit only a whole ball is of use, and the arc of the cockpit's frame at the side of the view
  // can outvote a ball in front of a bright sky - so the next peaks are tried, the votes round each one tried
  // put out, until one is whole
  if(not past_hud or not first or first->inside)
    return first;
  for(int tried{}; tried != 8; ++tried)
    {
    auto const peak{std::ranges::max_element(votes)};
    if(*peak <= 0.f)
      break;
    auto const centre{size_t(peak - votes.begin())};
    if(tried != 0)
      if(auto const next{circle_at(centre)}; next and next->inside)
        return next;
    int const px{int(centre % w)};
    int const py{int(centre / w)};
    for(int y{std::max(0, py - r_min)}; y <= std::min(int(h) - 1, py + r_min); ++y)
      for(int x{std::max(0, px - r_min)}; x <= std::min(int(w) - 1, px + r_min); ++x)
        votes[size_t(y) * w + size_t(x)] = 0.f;
    }
  return first;
  }

namespace
  {
  ///\brief the ball resampled, the scanner's marks still on it
  [[nodiscard]]
  auto sample_ball(image_t const & picture, disc_t const & disc, uint32_t side) -> image_t
    {
    image_t face{.width = side, .height = side, .rgb = std::vector<uint8_t>(size_t{side} * side * 3u, 0u)};
    if(picture.empty() or side == 0u)
      return face;
    // a little inside the rim, which the scanner outlines and darkens
    float const reach{disc.radius * 0.97f};
    // each pixel the mean of a 3 x 3 pattern over the part of the picture it covers
    float const step{reach * 2.f / float(side) / 3.f};
    std::vector<float> colour(size_t{side} * side * 3u, 0.f);
    std::vector<bool> inside(size_t{side} * side, false);
    for(uint32_t y{}; y != side; ++y)
      for(uint32_t x{}; x != side; ++x)
        {
        float const u{(float(x) + 0.5f) / float(side) * 2.f - 1.f};
        float const v{(float(y) + 0.5f) / float(side) * 2.f - 1.f};
        size_t const at{size_t{y} * side + x};
        if(u * u + v * v > 1.f)
          continue;
        inside[at] = true;
        std::array<float, 3> sum{};
        for(int oy{-1}; oy <= 1; ++oy)
          for(int ox{-1}; ox <= 1; ++ox)
            {
            auto const px{uint32_t(std::clamp(disc.x + u * reach + float(ox) * step, 0.f, float(picture.width - 1u)))};
            auto const py{uint32_t(std::clamp(disc.y + v * reach + float(oy) * step, 0.f, float(picture.height - 1u)))};
            uint8_t const * const p{picture.rgb.data() + (size_t{py} * picture.width + px) * 3u};
            for(size_t c{}; c != 3u; ++c)
              sum[c] += float(p[c]) / 255.f;
            }
        for(size_t c{}; c != 3u; ++c)
          colour[at * 3u + c] = sum[c] / 9.f;
        }
    for(size_t at{}; at != inside.size(); ++at)
      if(inside[at])
        for(size_t c{}; c != 3u; ++c)
          // at least 1, so a dark pixel is not taken for the black outside the ball
          face.rgb[at * 3u + c] = uint8_t(std::clamp(std::lround(colour[at * 3u + c] * 255.f), 1l, 255l));
    return face;
    }

  ///\brief the ship's HUD over the ball: its saturated orange and yellow, which no surface comes near
  [[nodiscard]]
  auto is_hud(uint8_t const * p) -> bool
    {
    return p[0] > 140u and int(p[2]) * 4 < int(p[0]) and int(p[1]) * 20 < int(p[0]) * 19 and int(p[1]) * 5 > int(p[0]);
    }

  [[nodiscard]]
  auto is_cyan(uint8_t const * p) -> bool
    { return int(p[2]) - int(p[0]) > 51 and int(p[1]) - int(p[0]) > 25; }

  ///\brief the scanner's marks filled from around them: its white dot and needle - much brighter than the
  /// ball and nearly colourless - and, where no filter paints the ball, its thin cyan rings
  ///\param strokes also the thin bright strokes of the HUD's words - a pixel well above the median of the 5 x 5
  /// around it - and the marks grown by two pixels rather than one: a view from the cockpit has the target's
  /// name and distance written over the ball
  auto clear_marks(image_t & face, bool cyan_rings, bool strokes = false) -> void
    {
    uint32_t const side{face.width};
    size_t const pixels{size_t{side} * side};
    std::vector<bool> inside(pixels, false);
    std::vector<float> colour(pixels * 3u, 0.f);
    for(size_t at{}; at != pixels; ++at)
      {
      uint8_t const * const p{face.rgb.data() + at * 3u};
      inside[at] = p[0] != 0u or p[1] != 0u or p[2] != 0u;
      for(size_t c{}; c != 3u; ++c)
        colour[at * 3u + c] = float(p[c]) / 255.f;
      }

    std::vector<float> lowest;
    for(size_t at{}; at != inside.size(); ++at)
      if(inside[at])
        lowest.push_back(std::min({colour[at * 3u], colour[at * 3u + 1u], colour[at * 3u + 2u]}));
    if(lowest.empty())
      return;
    std::vector<float> sorted{lowest};
    auto const median{sorted.begin() + std::ptrdiff_t(sorted.size() / 2u)};
    std::nth_element(sorted.begin(), median, sorted.end());
    float const usual{*median};

    std::vector<bool> mark(inside.size(), false);
    for(size_t at{}; at != inside.size(); ++at)
      {
      if(not inside[at])
        continue;
      float const r{colour[at * 3u]};
      float const g{colour[at * 3u + 1u]};
      float const b{colour[at * 3u + 2u]};
      float const most{std::max({r, g, b})};
      float const least{std::min({r, g, b})};
      float const saturation{(most - least) / std::max(most, 1e-3f)};
      bool const white{least > usual + 0.2f and saturation < 0.2f};
      bool const cyan{cyan_rings and is_cyan(face.rgb.data() + at * 3u)};
      mark[at] = white or cyan or is_hud(face.rgb.data() + at * 3u);
      }
    if(strokes)
      for(uint32_t y{2u}; y + 2u < side; ++y)
        for(uint32_t x{2u}; x + 2u < side; ++x)
          {
          size_t const at{size_t{y} * side + x};
          if(not inside[at])
            continue;
          std::array<float, 25> around{};
          size_t n{};
          for(uint32_t dy{}; dy != 5u; ++dy)
            for(uint32_t dx{}; dx != 5u; ++dx)
              {
              size_t const near{size_t{y + dy - 2u} * side + x + dx - 2u};
              around[n++] = (colour[near * 3u] + colour[near * 3u + 1u] + colour[near * 3u + 2u]) / 3.f;
              }
          std::nth_element(around.begin(), around.begin() + 12, around.end());
          float const lum{(colour[at * 3u] + colour[at * 3u + 1u] + colour[at * 3u + 2u]) / 3.f};
          if(lum > around[12] + 0.1f)
            mark[at] = true;
          }
    // a pixel more around each mark, for its soft edge - two for the words and rings of the HUD
    std::vector<bool> grown{mark};
    for(int round{}; round != (strokes ? 2 : 1); ++round)
      {
      std::vector<bool> const before{grown};
      for(uint32_t y{1u}; y + 1u < side; ++y)
        for(uint32_t x{1u}; x + 1u < side; ++x)
          {
          size_t const at{size_t{y} * side + x};
          grown[at] = before[at] or before[at - 1u] or before[at + 1u] or before[at - side] or before[at + side];
          }
      }

    // filled from the edge of each mark inwards, a ring of pixels at a time
    std::vector<bool> good(inside.size(), false);
    for(size_t at{}; at != inside.size(); ++at)
      good[at] = inside[at] and not grown[at];
    for(int round{}; round != 64; ++round)
      {
      std::vector<size_t> filled;
      for(uint32_t y{1u}; y + 1u < side; ++y)
        for(uint32_t x{1u}; x + 1u < side; ++x)
          {
          size_t const at{size_t{y} * side + x};
          if(not inside[at] or good[at])
            continue;
          std::array<float, 3> sum{};
          int count{};
          for(size_t const n: {at - 1u, at + 1u, at - side, at + side})
            if(good[n])
              {
              for(size_t c{}; c != 3u; ++c)
                sum[c] += colour[n * 3u + c];
              ++count;
              }
          if(count == 0)
            continue;
          for(size_t c{}; c != 3u; ++c)
            colour[at * 3u + c] = sum[c] / float(count);
          filled.push_back(at);
          }
      if(filled.empty())
        break;
      for(size_t const at: filled)
        good[at] = true;
      }

    for(size_t at{}; at != inside.size(); ++at)
      if(inside[at])
        for(size_t c{}; c != 3u; ++c)
          face.rgb[at * 3u + c] = uint8_t(std::clamp(std::lround(colour[at * 3u + c] * 255.f), 1l, 255l));
    }

  ///\brief the normal of the ball at a pixel of a face side x side
  [[nodiscard]]
  auto ball_normal(size_t at, uint32_t side) -> std::array<float, 3>
    {
    float const half{float(side) / 2.f};
    float const u{(float(at % side) + 0.5f - half) / half};
    float const v{(float(at / side) + 0.5f - half) / half};
    return {u, v, std::sqrt(std::max(0.f, 1.f - u * u - v * v))};
    }

  [[nodiscard]]
  auto on_ball(image_t const & face, size_t at) -> bool
    {
    uint8_t const * const p{face.rgb.data() + at * 3u};
    return p[0] != 0u or p[1] != 0u or p[2] != 0u;
    }

  ///\brief where the light comes from: brightness = normal . w over the lit pixels, w fitted by least squares
  [[nodiscard]]
  auto fit_light(image_t const & face) -> std::optional<std::array<float, 3>>
    {
    uint32_t const side{face.width};
    size_t const pixels{size_t{side} * side};
    float bright{};
    for(size_t at{}; at != pixels; ++at)
      if(on_ball(face, at))
        bright = std::max(bright, luminance(face.rgb.data() + at * 3u));
    std::array<std::array<double, 3>, 3> m{};
    std::array<double, 3> rhs{};
    for(size_t at{}; at != pixels; ++at)
      {
      if(not on_ball(face, at))
        continue;
      float const l{luminance(face.rgb.data() + at * 3u)};
      if(l < bright * 0.15f)
        continue;
      auto const n{ball_normal(at, side)};
      for(size_t i{}; i != 3u; ++i)
        {
        rhs[i] += double(n[i]) * l;
        for(size_t j{}; j != 3u; ++j)
          m[i][j] += double(n[i]) * n[j];
        }
      }
    // Cramer's rule for the 3 x 3 system
    auto const det = [](std::array<std::array<double, 3>, 3> const & a)
    {
      return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
             + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    };
    double const d{det(m)};
    if(std::abs(d) < 1e-9)
      return std::nullopt;
    std::array<float, 3> light{};
    for(size_t k{}; k != 3u; ++k)
      {
      auto mk{m};
      for(size_t i{}; i != 3u; ++i)
        mk[i][k] = rhs[i];
      light[k] = float(det(mk) / d);
      }
    float const length{std::hypot(light[0], light[1], light[2])};
    if(length < 1e-6f)
      return std::nullopt;
    for(float & c: light)
      c /= length;
    return light;
    }

  ///\brief the share of the ball that should be in daylight and is dark - the cockpit's frame across it
  [[nodiscard]]
  auto occluded(image_t const & face, std::array<float, 3> const & light) -> float
    {
    uint32_t const side{face.width};
    size_t const pixels{size_t{side} * side};
    std::vector<float> lit;
    for(size_t at{}; at != pixels; ++at)
      if(on_ball(face, at))
        {
        auto const n{ball_normal(at, side)};
        if(n[0] * light[0] + n[1] * light[1] + n[2] * light[2] > 0.4f)
          lit.push_back(luminance(face.rgb.data() + at * 3u));
        }
    if(lit.empty())
      return 1.f;
    std::vector<float> sorted{lit};
    auto const median{sorted.begin() + std::ptrdiff_t(sorted.size() / 2u)};
    std::nth_element(sorted.begin(), median, sorted.end());
    float const usual{*median};
    return float(std::ranges::count_if(lit, [&](float l) { return l < usual * 0.3f; })) / float(lit.size());
    }

  ///\brief the share of the rim the ball stands out of the sky at: in each of 16 sectors the median colour of a
  /// band just inside the rim, 0.7 to 0.9 radii, against that of a band just outside, 1.1 to 1.35 - the HUD's
  /// colours left out; none when hardly any sector has both bands in the picture
  ///\detail a real ball differs from what lies behind it, darker or lighter or of another colour, at least
  /// along its lit side; a circle the stars and dust outline has the same sky on both sides of it
  [[nodiscard]]
  auto standing_out(image_t const & view, disc_t const & disc) -> std::optional<float>
    {
    constexpr size_t sectors{16u};
    // per sector, inside and outside, the three channels
    std::array<std::array<std::array<std::vector<float>, 3>, 2>, sectors> bands;
    float const outer{disc.radius * 1.35f};
    auto const clamp = [](float v, uint32_t limit) { return uint32_t(std::clamp(v, 0.f, float(limit))); };
    for(uint32_t y{clamp(disc.y - outer, view.height)}; y != clamp(disc.y + outer + 1.f, view.height); ++y)
      for(uint32_t x{clamp(disc.x - outer, view.width)}; x != clamp(disc.x + outer + 1.f, view.width); ++x)
        {
        float const dx{float(x) - disc.x};
        float const dy{float(y) - disc.y};
        float const d{std::hypot(dx, dy) / disc.radius};
        bool const in{d >= 0.7f and d <= 0.9f};
        if(not in and (d < 1.1f or d > 1.35f))
          continue;
        uint8_t const * const p{view.rgb.data() + (size_t{y} * view.width + x) * 3u};
        if(is_hud(p) or is_cyan(p))
          continue;
        auto const sector{
          size_t((std::atan2(dy, dx) + std::numbers::pi_v<float>) / (2.f * std::numbers::pi_v<float>)*float(sectors))
          % sectors
        };
        for(size_t c{}; c != 3u; ++c)
          bands[sector][in ? 0u : 1u][c].push_back(float(p[c]) / 255.f);
        }
    auto const median = [](std::vector<float> & v)
    {
      auto const middle{v.begin() + std::ptrdiff_t(v.size() / 2u)};
      std::nth_element(v.begin(), middle, v.end());
      return *middle;
    };
    size_t seen{};
    size_t apart{};
    for(auto & sector: bands)
      {
      if(sector[0][0].size() < 4u or sector[1][0].size() < 4u)
        continue;
      ++seen;
      float step{};
      for(size_t c{}; c != 3u; ++c)
        step = std::max(step, std::abs(median(sector[0][c]) - median(sector[1][c])));
      apart += step > 0.08f ? 1u : 0u;
      }
    if(seen < sectors / 2u)
      return std::nullopt;
    return float(apart) / float(seen);
    }
  }  // namespace

auto cut_face(image_t const & picture, disc_t const & disc, uint32_t side) -> image_t
  {
  image_t face{sample_ball(picture, disc, side)};
  clear_marks(face, look_of(face).cyan <= 0.04f);
  return face;
  }

auto look_of(image_t const & face) -> look_t
  {
  look_t look{};
  if(face.empty())
    return look;
  // the middle of the ball only, 0.9 of its radius
  float const half{float(face.width) / 2.f};
  std::vector<float> lum;
  std::array<double, 3> sum{};
  for(uint32_t y{}; y != face.height; ++y)
    for(uint32_t x{}; x != face.width; ++x)
      {
      float const u{(float(x) + 0.5f - half) / half};
      float const v{(float(y) + 0.5f - half) / half};
      if(u * u + v * v > 0.81f)
        continue;
      uint8_t const * const p{face.rgb.data() + (size_t{y} * face.width + x) * 3u};
      for(size_t c{}; c != 3u; ++c)
        sum[c] += double(p[c]) / 255.0;
      lum.push_back(luminance(p));
      if(is_cyan(p))
        look.cyan += 1.f;
      }
  if(lum.empty())
    return look;
  look.cyan /= float(lum.size());
  look.red = float(sum[0] / double(lum.size()));
  look.green = float(sum[1] / double(lum.size()));
  look.blue_mean = float(sum[2] / double(lum.size()));
  look.blue = look.blue_mean - (look.red + look.green) / 2.f;
  std::vector<float> sorted{lum};
  auto const median{sorted.begin() + std::ptrdiff_t(sorted.size() / 2u)};
  std::nth_element(sorted.begin(), median, sorted.end());
  float const usual{*median};
  look.streaks = float(std::ranges::count_if(lum, [&](float l) { return l > usual + 0.25f; })) / float(lum.size());
  return look;
  }

auto lit_colour(image_t const & photo) -> std::optional<std::array<float, 3>>
  {
  auto const disc{find_disc(photo)};
  if(not disc or disc->rim < 0.5f)
    return std::nullopt;
  image_t const face{cut_face(photo, disc_t{.x = disc->x, .y = disc->y, .radius = disc->radius * 0.85f}, 64u)};
  // the lit side is the brighter part: the pixels above the 60th in a hundred
  std::vector<std::pair<float, size_t>> pixels;
  for(size_t at{}; at != size_t{face.width} * face.height; ++at)
    {
    uint8_t const * const p{face.rgb.data() + at * 3u};
    if(p[0] != 0u or p[1] != 0u or p[2] != 0u)
      pixels.emplace_back(luminance(p), at);
    }
  if(pixels.size() < 16u)
    return std::nullopt;
  auto const cut{pixels.begin() + std::ptrdiff_t(pixels.size() * 60u / 100u)};
  std::nth_element(pixels.begin(), cut, pixels.end());
  std::array<double, 3> sum{};
  size_t count{};
  for(auto it{cut}; it != pixels.end(); ++it, ++count)
    for(size_t c{}; c != 3u; ++c)
      sum[c] += double(face.rgb[it->second * 3u + c]) / 255.0;
  return std::array<float, 3>{
    float(sum[0] / double(count)), float(sum[1] / double(count)), float(sum[2] / double(count))
  };
  }

auto retint(image_t face, std::array<float, 3> colour) -> image_t
  {
  size_t const pixels{size_t{face.width} * face.height};
  double sum{};
  double squares{};
  size_t count{};
  for(size_t at{}; at != pixels; ++at)
    if(uint8_t const * const p{face.rgb.data() + at * 3u}; p[0] != 0u or p[1] != 0u or p[2] != 0u)
      {
      double const l{luminance(p)};
      sum += l;
      squares += l * l;
      ++count;
      }
  if(count == 0u)
    return face;
  double const mean{std::max(sum / double(count), 1e-3)};
  double const spread{std::sqrt(std::max(0.0, squares / double(count) - mean * mean)) / mean};
  // a filtered view is flat: its relief is drawn up to a visible spread, never more than threefold
  double const gain{spread > 1e-3 ? std::clamp(0.18 / spread, 1.0, 3.0) : 1.0};
  for(size_t at{}; at != pixels; ++at)
    {
    uint8_t * const p{face.rgb.data() + at * 3u};
    if(p[0] == 0u and p[1] == 0u and p[2] == 0u)
      continue;
    double const detail{std::clamp(1.0 + (luminance(p) / mean - 1.0) * gain, 0.0, 2.5)};
    for(size_t c{}; c != 3u; ++c)
      // at least 1, so a dark pixel is not taken for the black outside the ball
      p[c] = uint8_t(std::clamp(std::lround(detail * double(colour[c]) * 1.15 * 255.0), 1l, 255l));
    }
  return face;
  }

auto above_dashboard(image_t view, float top, float height) -> image_t
  {
  if(height <= 0.f or view.empty())
    return view;
  auto const rows{
    uint32_t(std::clamp(std::round((dashboard_top - top) / height * float(view.height)), 0.f, float(view.height)))
  };
  view.height = rows;
  view.rgb.resize(size_t{view.width} * rows * 3u);
  return view;
  }

auto judge_approach(image_t const & view, float pixel_scale, float full_size, std::string * why)
  -> std::optional<approach_t>
  {
  auto const refuse = [why](std::string reason) -> std::optional<approach_t>
  {
    if(why != nullptr)
      *why = std::move(reason);
    return std::nullopt;
  };
  // the night side's rim is lost against the black of space, so only the lit part of it shows
  // the target's cyan ring stands on the ball, near its middle, and would be taken for a small ball
  auto const disc{find_disc(view, true)};
  if(not disc)
    return refuse("no ball found");
  if(disc->rim < 0.3f)
    return refuse(std::format("rim {:.2f} under 0.30, {:.0f} px", disc->rim, disc->radius * pixel_scale));
  if(not disc->inside)
    return refuse(std::format("not whole in the view, {:.0f} px", disc->radius * pixel_scale));
  // farther, the game draws the ball without the face it shows up close
  if(disc->radius * pixel_scale < full_size)
    return refuse(std::format("{:.0f} px, under {:.0f}", disc->radius * pixel_scale, full_size));
  image_t const ball{sample_ball(view, *disc, 96u)};
  std::vector<float> lum;
  size_t hud{};
  for(size_t at{}; at != size_t{ball.width} * ball.height; ++at)
    if(uint8_t const * const p{ball.rgb.data() + at * 3u}; p[0] != 0u or p[1] != 0u or p[2] != 0u)
      {
      lum.push_back(luminance(p));
      hud += is_hud(p) ? 1u : 0u;
      }
  if(lum.size() < 64u)
    return refuse("hardly any of the ball lit");
  std::vector<float> sorted{lum};
  auto const p95{sorted.begin() + std::ptrdiff_t(sorted.size() * 95u / 100u)};
  std::nth_element(sorted.begin(), p95, sorted.end());
  float const bright{*p95};
  // a ball in the dark, or a black disc against the stars, is no face
  if(bright < 0.08f)
    return refuse(std::format("too dark, {:.2f}", bright));
  // far off the planet is a dot, and the noise of the stars and dust behind it can pass for a rim - a ball made
  // of the sky has the same sky on both sides of its rim, a real one stands out of what lies behind it
  if(auto const apart{standing_out(view, *disc)}; apart and *apart < 0.25f)
    return refuse(
      std::format(
        "no different from the sky round it, {:.2f} of the rim, {:.0f} px", *apart, disc->radius * pixel_scale
      )
    );
  approach_t judged{
    .disc = *disc,
    .lit = float(std::ranges::count_if(lum, [&](float l) { return l > bright * 0.25f; })) / float(lum.size()),
    .hud = float(hud) / float(lum.size()),
    .score = 0.f
  };
  float const streaks{look_of(ball).streaks};
  // what the cockpit's frame hides counts against the view as much as three times its share
  auto const light{fit_light(ball)};
  float const hidden{light ? occluded(ball, *light) : 1.f};
  // more than a little hidden, and the frame would be on the face
  if(hidden > 0.06f)
    return refuse(std::format("{:.0f}% hidden by the frame, {:.0f} px", hidden * 100.f, disc->radius * pixel_scale));
  judged.score = judged.lit * std::max(0.f, 1.f - 8.f * judged.hud) * std::max(0.f, 1.f - 3.f * hidden)
                 * (streaks > 0.03f ? 0.2f : 1.f);
  return judged;
  }

auto approach_face(image_t const & view, disc_t const & disc, uint32_t side) -> image_t
  {
  image_t face{sample_ball(view, disc, side)};
  // the target's ring and its words stand on the ball; a ring is cyan, which a ball seldom is
  clear_marks(face, look_of(face).cyan <= 0.1f, true);
  size_t const pixels{size_t{side} * side};
  float const half{float(side) / 2.f};
  auto const normal = [&](size_t at) -> std::array<float, 3>
  {
    float const u{(float(at % side) + 0.5f - half) / half};
    float const v{(float(at / side) + 0.5f - half) / half};
    return {u, v, std::sqrt(std::max(0.f, 1.f - u * u - v * v))};
  };
  auto const inside = [&](size_t at)
  {
    uint8_t const * const p{face.rgb.data() + at * 3u};
    return p[0] != 0u or p[1] != 0u or p[2] != 0u;
  };

  auto const fitted{fit_light(face)};
  if(not fitted)
    return face;
  std::array<float, 3> const light{*fitted};

  // each pixel divided by how squarely it faced the light, never by less than 0.45 - the limb is hazy
  // rather than dark, and dividing it by its small cosine would light a bright ring round the face
  constexpr float night{0.2f};
  std::vector<float> albedo(pixels * 3u, 0.f);
  std::vector<bool> day(pixels, false);
  for(size_t at{}; at != pixels; ++at)
    {
    if(not inside(at))
      continue;
    auto const n{normal(at)};
    float const facing{n[0] * light[0] + n[1] * light[1] + n[2] * light[2]};
    if(facing <= night)
      continue;
    day[at] = true;
    for(size_t c{}; c != 3u; ++c)
      albedo[at * 3u + c] = float(face.rgb[at * 3u + c]) / 255.f / std::max(facing, 0.45f);
    }
  std::array<double, 3> mean{};
  size_t count{};
  for(size_t at{}; at != pixels; ++at)
    if(day[at])
      {
      for(size_t c{}; c != 3u; ++c)
        mean[c] += albedo[at * 3u + c];
      ++count;
      }
  if(count == 0u)
    return face;
  for(double & c: mean)
    c /= double(count);
  // brought to the brightness a ball shows in full light, never brighter than it was
  float const scale{std::min(1.f, 0.6f / std::max(float((mean[0] + mean[1] + mean[2]) / 3.0), 1e-3f))};

  image_t out{.width = side, .height = side, .rgb = std::vector<uint8_t>(pixels * 3u, 0u)};
  for(size_t at{}; at != pixels; ++at)
    {
    if(not inside(at))
      continue;
    size_t from{at};
    if(not day[at])
      {
      // the night side mirrored across the terminator onto the day side, or the day's mean where that
      // lands behind the ball
      auto n{normal(at)};
      float const facing{n[0] * light[0] + n[1] * light[1] + n[2] * light[2]};
      for(size_t c{}; c != 3u; ++c)
        n[c] -= 2.f * facing * light[c];
      auto const x{int(std::lround(n[0] * half + half - 0.5f))};
      auto const y{int(std::lround(n[1] * half + half - 0.5f))};
      size_t const mirrored{size_t(std::clamp(y, 0, int(side) - 1)) * side + size_t(std::clamp(x, 0, int(side) - 1))};
      from = n[2] > 0.f and day[mirrored] ? mirrored : pixels;
      }
    for(size_t c{}; c != 3u; ++c)
      {
      float const value{from == pixels ? float(mean[c]) : albedo[from * 3u + c]};
      out.rgb[at * 3u + c] = uint8_t(std::clamp(std::lround(value * scale * 255.f), 1l, 255l));
      }
    }
  return out;
  }

auto make_face(
  std::span<image_t const> views,
  image_t const * photo,
  image_t const * approach,
  std::array<float, 3> fallback,
  uint32_t side
) -> image_t
  {
  bool const from_cockpit{approach != nullptr and approach->width == side and not approach->empty()};

  struct candidate_t
    {
    image_t face;
    look_t look;
    float radius{};
    };

  std::optional<candidate_t> best;
  for(image_t const & view: views)
    {
    auto const disc{find_disc(view)};
    if(not disc or disc->rim < 0.6f or not disc->inside)
      continue;
    // judged before the marks are cleared, or a probe's streak would be filled over and pass
    image_t face{sample_ball(view, *disc, side)};
    look_t const look{look_of(face)};
    if(look.streaks > 0.03f)
      continue;
    // the least painted wins, and of two alike the larger shows more
    auto const rank = [](look_t const & l, float radius) { return l.cyan - radius / 20000.f; };
    if(not best or rank(look, disc->radius) < rank(best->look, best->radius))
      best = candidate_t{.face = std::move(face), .look = look, .radius = disc->radius};
    }
  if(not best)
    return from_cockpit ? *approach : image_t{};
  bool const own_colours{best->look.cyan <= 0.04f};
  clear_marks(best->face, own_colours);
  // a view in its own colours is taken as it is; a painted one gives way to the cockpit's true colours
  if(own_colours)
    return std::move(best->face);
  if(from_cockpit)
    return *approach;
  std::optional<std::array<float, 3>> colour;
  if(photo != nullptr)
    colour = lit_colour(*photo);
  return retint(std::move(best->face), colour.value_or(fallback));
  }
  }  // namespace planet_face
