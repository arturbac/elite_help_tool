#include <planet_face.h>

#include <algorithm>
#include <cmath>
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
  }  // namespace

auto find_disc(image_t const & picture) -> std::optional<disc_t>
  {
  if(picture.empty())
    return std::nullopt;
  float scale{};
  grey_t grey{shrink(picture, scale)};
  uint32_t const w{grey.width};
  uint32_t const h{grey.height};
  if(w < 16u or h < 16u)
    return std::nullopt;
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

  // the strongest eighth of the edges
  std::vector<float> sorted{magnitude};
  auto const nth{sorted.begin() + std::ptrdiff_t(sorted.size() * 92u / 100u)};
  std::nth_element(sorted.begin(), nth, sorted.end());
  float const threshold{std::max(*nth, 1e-3f)};

  struct edge_t
    { float x, y, ux, uy; };

  std::vector<edge_t> edges;
  for(uint32_t y{1u}; y + 1u < h; ++y)
    for(uint32_t x{1u}; x + 1u < w; ++x)
      if(size_t const at{size_t{y} * w + x}; magnitude[at] > threshold)
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
  auto const best{std::ranges::max_element(votes)};
  if(*best <= 0.f)
    return std::nullopt;
  auto const centre{size_t(best - votes.begin())};
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

  [[nodiscard]]
  auto is_cyan(uint8_t const * p) -> bool
    { return int(p[2]) - int(p[0]) > 51 and int(p[1]) - int(p[0]) > 25; }

  ///\brief the scanner's marks filled from around them: its white dot and needle - much brighter than the
  /// ball and nearly colourless - and, where no filter paints the ball, its thin cyan rings
  auto clear_marks(image_t & face, bool cyan_rings) -> void
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
      mark[at] = white or cyan;
      }
    // a pixel more around each mark, for its soft edge
    std::vector<bool> grown{mark};
    for(uint32_t y{1u}; y + 1u < side; ++y)
      for(uint32_t x{1u}; x + 1u < side; ++x)
        {
        size_t const at{size_t{y} * side + x};
        grown[at] = mark[at] or mark[at - 1u] or mark[at + 1u] or mark[at - side] or mark[at + side];
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

auto make_face(std::span<image_t const> views, image_t const * photo, std::array<float, 3> fallback, uint32_t side)
  -> image_t
  {
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
    return {};
  bool const own_colours{best->look.cyan <= 0.04f};
  clear_marks(best->face, own_colours);
  // a view in its own colours is taken as it is
  if(own_colours)
    return std::move(best->face);
  std::optional<std::array<float, 3>> colour;
  if(photo != nullptr)
    colour = lit_colour(*photo);
  return retint(std::move(best->face), colour.value_or(fallback));
  }
  }  // namespace planet_face
