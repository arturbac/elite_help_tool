#include <port_model.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>
#include <span>

namespace port_model
  {
namespace
  {
  struct vec_t
    {
    float x{};
    float y{};
    float z{};
    };

  [[nodiscard]]
  constexpr auto operator+(vec_t a, vec_t b) -> vec_t
    { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

  [[nodiscard]]
  constexpr auto operator-(vec_t a, vec_t b) -> vec_t
    { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

  [[nodiscard]]
  constexpr auto operator*(vec_t a, float k) -> vec_t
    { return {a.x * k, a.y * k, a.z * k}; }

  [[nodiscard]]
  constexpr auto dot(vec_t a, vec_t b) -> float
    { return a.x * b.x + a.y * b.y + a.z * b.z; }

  [[nodiscard]]
  constexpr auto cross(vec_t a, vec_t b) -> vec_t
    { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }

  [[nodiscard]]
  auto length(vec_t a) -> float
    { return std::sqrt(dot(a, a)); }

  [[nodiscard]]
  auto unit(vec_t a) -> vec_t
    {
    float const l{length(a)};
    return l > 0.f ? a * (1.f / l) : a;
    }

  struct polygon_t
    {
    std::vector<vec_t> points;
    part_e part{part_e::hull};
    ///\brief painted over the face it lies on - a slot is drawn on its face, not beside it
    bool decal{};
    };

  [[nodiscard]]
  auto centroid(std::vector<vec_t> const & points) -> vec_t
    {
    vec_t sum{};
    for(vec_t const & p: points)
      sum = sum + p;
    return sum * (1.f / static_cast<float>(points.size()));
    }

  ///\brief Newell's normal - sound for any planar polygon, whatever its first three points
  [[nodiscard]]
  auto normal_of(std::vector<vec_t> const & points) -> vec_t
    {
    vec_t n{};
    for(size_t i{}; i != points.size(); ++i)
      {
      vec_t const & a{points[i]};
      vec_t const & b{points[(i + 1u) % points.size()]};
      n = n + vec_t{(a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y)};
      }
    return unit(n);
    }

  ///\brief turns the polygon to face away from a point inside the solid it bounds
  auto outward(polygon_t & polygon, vec_t inside) -> void
    {
    if(dot(normal_of(polygon.points), centroid(polygon.points) - inside) < 0.f)
      std::ranges::reverse(polygon.points);
    }

  ///\brief the faces of a convex solid - every plane of the given normals through its outermost corners
  auto convex_solid(std::vector<polygon_t> & out, std::vector<vec_t> const & corners, std::span<vec_t const> normals, part_e part)
    -> void
    {
    vec_t const middle{centroid(corners)};
    for(vec_t n: normals)
      {
      n = unit(n);
      float reach{-1e9f};
      for(vec_t const & c: corners)
        reach = std::max(reach, dot(c - middle, n));
      polygon_t face{.part = part};
      for(vec_t const & c: corners)
        if(dot(c - middle, n) > reach - 1e-4f)
          face.points.push_back(c);
      if(face.points.size() < 3u)
        continue;
      // around the face's middle, in the plane's own axes
      vec_t const mid{centroid(face.points)};
      vec_t const u{unit(face.points.front() - mid)};
      vec_t const v{cross(n, u)};
      std::ranges::sort(
        face.points,
        [&](vec_t const & a, vec_t const & b)
        { return std::atan2(dot(a - mid, v), dot(a - mid, u)) < std::atan2(dot(b - mid, v), dot(b - mid, u)); }
      );
      outward(face, middle);
      out.push_back(std::move(face));
      }
    }

  ///\brief a box around a centre, its half sides along three axes that need not be the model's own
  auto box(std::vector<polygon_t> & out, vec_t centre, vec_t a, vec_t b, vec_t c, part_e part = part_e::hull) -> void
    {
    std::vector<vec_t> corners;
    for(float const i: {-1.f, 1.f})
      for(float const j: {-1.f, 1.f})
        for(float const k: {-1.f, 1.f})
          corners.push_back(centre + a * i + b * j + c * k);
    std::array const normals{a, a * -1.f, b, b * -1.f, c, c * -1.f};
    convex_solid(out, corners, normals, part);
    }

  auto box(std::vector<polygon_t> & out, vec_t centre, vec_t half, part_e part = part_e::hull) -> void
    { box(out, centre, {half.x, 0.f, 0.f}, {0.f, half.y, 0.f}, {0.f, 0.f, half.z}, part); }

  ///\brief an upright cylinder, the top cap a part of its own - on a spindle it is the way in
  auto cylinder(std::vector<polygon_t> & out, vec_t centre, float radius, float half, int sides, part_e top) -> void
    {
    polygon_t upper{.part = top};
    polygon_t lower{};
    for(int i{}; i != sides; ++i)
      {
      float const a0{2.f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(sides)};
      float const a1{2.f * std::numbers::pi_v<float> * static_cast<float>(i + 1) / static_cast<float>(sides)};
      vec_t const p0{centre.x + radius * std::cos(a0), 0.f, centre.z + radius * std::sin(a0)};
      vec_t const p1{centre.x + radius * std::cos(a1), 0.f, centre.z + radius * std::sin(a1)};
      polygon_t side{
        .points = {
          p0 + vec_t{0.f, centre.y - half, 0.f},
          p1 + vec_t{0.f, centre.y - half, 0.f},
          p1 + vec_t{0.f, centre.y + half, 0.f},
          p0 + vec_t{0.f, centre.y + half, 0.f}
        }
      };
      outward(side, centre);
      out.push_back(std::move(side));
      upper.points.push_back(p0 + vec_t{0.f, centre.y + half, 0.f});
      lower.points.push_back(p0 + vec_t{0.f, centre.y - half, 0.f});
      }
    outward(upper, centre);
    outward(lower, centre);
    out.push_back(std::move(upper));
    out.push_back(std::move(lower));
    }

  auto sphere(std::vector<polygon_t> & out, vec_t centre, float radius, int around, int rings) -> void
    {
    auto const at = [&](int ring, int step) -> vec_t
    {
      float const lat{std::numbers::pi_v<float> * (static_cast<float>(ring) / static_cast<float>(rings) - 0.5f)};
      float const lon{2.f * std::numbers::pi_v<float> * static_cast<float>(step) / static_cast<float>(around)};
      return centre + vec_t{std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon)} * radius;
    };
    for(int ring{}; ring != rings; ++ring)
      for(int step{}; step != around; ++step)
        {
        polygon_t face;
        // the poles close into triangles
        face.points.push_back(at(ring, step));
        if(ring != 0)
          face.points.push_back(at(ring, step + 1));
        face.points.push_back(at(ring + 1, step + 1));
        if(ring + 1 != rings)
          face.points.push_back(at(ring + 1, step));
        outward(face, centre);
        out.push_back(std::move(face));
        }
    }

  ///\brief a ring lying flat, its tube round
  auto torus(std::vector<polygon_t> & out, float major, float minor, int around, int tube) -> void
    {
    auto const at = [&](int step, int round) -> vec_t
    {
      float const a{2.f * std::numbers::pi_v<float> * static_cast<float>(step) / static_cast<float>(around)};
      float const b{2.f * std::numbers::pi_v<float> * static_cast<float>(round) / static_cast<float>(tube)};
      float const r{major + minor * std::cos(b)};
      return {r * std::cos(a), minor * std::sin(b), r * std::sin(a)};
    };
    for(int step{}; step != around; ++step)
      for(int round{}; round != tube; ++round)
        {
        polygon_t face{.points = {at(step, round), at(step + 1, round), at(step + 1, round + 1), at(step, round + 1)}};
        float const a{2.f * std::numbers::pi_v<float> * (static_cast<float>(step) + 0.5f) / static_cast<float>(around)};
        outward(face, vec_t{major * std::cos(a), 0.f, major * std::sin(a)});
        out.push_back(std::move(face));
        }
    }

  ///\brief a lump of rock: an icosahedron split once, each corner pushed in or out by where it points
  auto rock(std::vector<polygon_t> & out, float radius) -> void
    {
    constexpr float phi{std::numbers::phi_v<float>};
    std::vector<vec_t> corners{
      {-1, phi, 0}, {1, phi, 0}, {-1, -phi, 0}, {1, -phi, 0}, {0, -1, phi}, {0, 1, phi},
      {0, -1, -phi}, {0, 1, -phi}, {phi, 0, -1}, {phi, 0, 1}, {-phi, 0, -1}, {-phi, 0, 1}
    };
    constexpr std::array<std::array<int, 3>, 20> faces{{{0, 11, 5}, {0, 5, 1},  {0, 1, 7},   {0, 7, 10}, {0, 10, 11},
                                                        {1, 5, 9},  {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
                                                        {3, 9, 4},  {3, 4, 2},  {3, 2, 6},   {3, 6, 8},  {3, 8, 9},
                                                        {4, 9, 5},  {2, 4, 11}, {6, 2, 10},  {8, 6, 7},  {9, 8, 1}}};
    // the same corner shared by several faces must move the same way, so the push depends on the direction alone
    auto const lump = [radius](vec_t p) -> vec_t
    {
      vec_t const d{unit(p)};
      float const bump{
        0.86f + 0.09f * std::sin(5.f * d.x + 2.f * d.y + 1.f) + 0.07f * std::cos(4.f * d.z - 3.f * d.y) - 0.05f * d.x * d.z
      };
      return d * (radius * bump);
    };
    for(auto const & [a, b, c]: faces)
      {
      vec_t const pa{unit(corners[a])};
      vec_t const pb{unit(corners[b])};
      vec_t const pc{unit(corners[c])};
      vec_t const ab{unit(pa + pb)};
      vec_t const bc{unit(pb + pc)};
      vec_t const ca{unit(pc + pa)};
      for(std::array<vec_t, 3> const & t: std::array<std::array<vec_t, 3>, 4>{{{pa, ab, ca}, {ab, pb, bc}, {ca, bc, pc}, {ab, bc, ca}}})
        {
        polygon_t face{.points = {lump(t[0]), lump(t[1]), lump(t[2])}, .part = part_e::rock};
        outward(face, {});
        out.push_back(std::move(face));
        }
      }
    }

  ///\brief the docking slot: a flat dark bar across the face, level with the horizon
  auto slot_on(std::vector<polygon_t> & out, polygon_t const & face) -> void
    {
    vec_t const mid{centroid(face.points)};
    vec_t const n{normal_of(face.points)};
    vec_t u{cross(vec_t{0.f, 1.f, 0.f}, n)};
    if(length(u) < 0.1f)
      u = cross(vec_t{1.f, 0.f, 0.f}, n);
    u = unit(u);
    vec_t const v{cross(n, u)};
    float inner{1e9f};
    for(vec_t const & p: face.points)
      inner = std::min(inner, length(p - mid));
    float const w{0.62f * inner};
    float const h{0.16f * inner};
    polygon_t slot{
      .points = {mid + u * -w + v * -h, mid + u * w + v * -h, mid + u * w + v * h, mid + u * -w + v * h},
      .part = part_e::slot,
      .decal = true
    };
    if(dot(normal_of(slot.points), n) < 0.f)
      std::ranges::reverse(slot.points);
    out.push_back(std::move(slot));
    }

  ///\brief turns the model to the view: a little from the side and from above
  struct view_t
    {
    float yaw{};
    float pitch{};

    [[nodiscard]]
    auto operator()(vec_t p) const -> vec_t
      {
      float const cy{std::cos(yaw)};
      float const sy{std::sin(yaw)};
      vec_t const r{cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z};
      float const cp{std::cos(pitch)};
      float const sp{std::sin(pitch)};
      return {r.x, cp * r.y - sp * r.z, sp * r.y + cp * r.z};
      }
    };

  constexpr float degree{std::numbers::pi_v<float> / 180.f};

  [[nodiscard]]
  auto build(kind_e kind) -> std::vector<polygon_t>
    {
    std::vector<polygon_t> out;
    constexpr float phi{std::numbers::phi_v<float>};
    switch(kind)
      {
      case kind_e::coriolis:
        {
        std::vector<vec_t> corners;
        for(float const a: {-1.f, 1.f})
          for(float const b: {-1.f, 1.f})
            {
            corners.push_back(vec_t{a, b, 0.f} * std::numbers::sqrt2_v<float> * 0.5f);
            corners.push_back(vec_t{a, 0.f, b} * std::numbers::sqrt2_v<float> * 0.5f);
            corners.push_back(vec_t{0.f, a, b} * std::numbers::sqrt2_v<float> * 0.5f);
            }
        std::vector<vec_t> normals{{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        for(float const a: {-1.f, 1.f})
          for(float const b: {-1.f, 1.f})
            for(float const c: {-1.f, 1.f})
              normals.push_back({a, b, c});
        convex_solid(out, corners, normals, part_e::hull);
        break;
        }
      case kind_e::dodec:
        {
        std::vector<vec_t> corners;
        for(float const a: {-1.f, 1.f})
          for(float const b: {-1.f, 1.f})
            {
            for(float const c: {-1.f, 1.f})
              corners.push_back({a, b, c});
            corners.push_back({0.f, a / phi, b * phi});
            corners.push_back({a / phi, b * phi, 0.f});
            corners.push_back({a * phi, 0.f, b / phi});
            }
        std::vector<vec_t> normals;
        for(float const a: {-1.f, 1.f})
          for(float const b: {-1.f, 1.f})
            {
            // the faces' middles run round the other way than the corners' pattern
            normals.push_back({a, 0.f, b * phi});
            normals.push_back({b * phi, a, 0.f});
            normals.push_back({0.f, a * phi, b});
            }
        for(vec_t & c: corners)
          c = c * (1.f / std::numbers::sqrt3_v<float>);
        convex_solid(out, corners, normals, part_e::hull);
        break;
        }
      case kind_e::ocellus:
        sphere(out, {}, 0.78f, 12, 7);
        // the spindle through the poles, the way in at its upper end
        cylinder(out, {0.f, 0.86f, 0.f}, 0.14f, 0.12f, 8, part_e::slot);
        cylinder(out, {0.f, -0.86f, 0.f}, 0.14f, 0.12f, 8, part_e::hull);
        break;
      case kind_e::orbis:
        {
        sphere(out, {}, 0.2f, 8, 5);
        cylinder(out, {0.f, 0.36f, 0.f}, 0.08f, 0.17f, 6, part_e::slot);
        cylinder(out, {0.f, -0.36f, 0.f}, 0.08f, 0.17f, 6, part_e::hull);
        for(int i{}; i != 3; ++i)
          {
          float const a{(30.f + 120.f * static_cast<float>(i)) * degree};
          vec_t const d{std::cos(a), 0.f, std::sin(a)};
          vec_t const side{-d.z, 0.f, d.x};
          box(out, d * 0.47f, d * 0.26f, vec_t{0.f, 0.035f, 0.f}, side * 0.035f);
          }
        torus(out, 0.84f, 0.14f, 20, 6);
        break;
        }
      case kind_e::outpost:
        box(out, {}, {0.07f, 0.95f, 0.07f});
        box(out, {0.f, 0.7f, 0.f}, {0.3f, 0.15f, 0.2f});
        box(out, {0.22f, 0.22f, 0.f}, {0.14f, 0.12f, 0.14f});
        box(out, {-0.21f, 0.28f, 0.f}, {0.13f, 0.15f, 0.12f});
        box(out, {0.2f, -0.16f, 0.04f}, {0.12f, 0.1f, 0.12f});
        box(out, {0.f, -0.58f, 0.f}, {0.45f, 0.04f, 0.3f});
        break;
      case kind_e::asteroid:
        rock(out, 0.84f);
        box(out, {0.05f, 0.8f, 0.08f}, {0.14f, 0.12f, 0.14f});
        break;
      }
    return out;
    }

  ///\brief the kinds whose way in is a slot cut in the face turned towards us
  [[nodiscard]]
  constexpr auto slotted(kind_e kind) -> bool
    { return kind == kind_e::coriolis or kind == kind_e::dodec; }
  }  // namespace

auto facets(kind_e kind, float radius, float light_x, float light_y) -> std::vector<facet_t>
  {
  view_t const view{
    .yaw = 32.f * degree,
    // a ring seen from so little above is a line; the Orbis leans towards us so its ring opens up
    .pitch = (kind == kind_e::orbis ? 38.f : 24.f) * degree
  };

  std::vector<polygon_t> model{build(kind)};
  for(polygon_t & polygon: model)
    for(vec_t & p: polygon.points)
      p = view(p);

  if(slotted(kind))
    {
    polygon_t const * front{};
    for(polygon_t const & polygon: model)
      if(polygon.points.size() >= 4u and (front == nullptr or normal_of(polygon.points).z > normal_of(front->points).z))
        front = &polygon;
    if(front != nullptr)
      {
      polygon_t const face{*front};
      std::vector<polygon_t> slot;
      slot_on(slot, face);
      // right after its face and as deep, so the stable order paints it over the face and nothing else
      model.insert(model.begin() + (front - model.data()) + 1, std::move(slot.front()));
      }
    }

  // whatever faces away is hidden behind the rest, and the others are painted from the back forwards
  struct placed_t
    {
    polygon_t const * polygon;
    float depth;
    vec_t normal;
    };
  std::vector<placed_t> shown;
  for(polygon_t const & polygon: model)
    if(vec_t const n{normal_of(polygon.points)}; n.z > 0.01f)
      shown.push_back({&polygon, polygon.decal and not shown.empty() ? shown.back().depth : centroid(polygon.points).z, n});
  std::ranges::stable_sort(shown, {}, &placed_t::depth);

  // the star lies on the picture's plane, a little towards us, so the side facing us is never black
  vec_t const light{unit(vec_t{light_x, -light_y, 0.f}) + vec_t{0.f, 0.f, 0.7f}};
  vec_t const towards{unit(light)};

  std::vector<facet_t> out;
  out.reserve(shown.size());
  for(placed_t const & placed: shown)
    {
    facet_t facet{.part = placed.polygon->part};
    facet.shade = 0.22f + 0.78f * std::max(0.f, dot(placed.normal, towards));
    // a face turned to us goes round counter-clockwise, and the layer fills clockwise - its smoothed edge
    // falls outside the polygon only that way
    for(vec_t const & p: placed.polygon->points | std::views::reverse)
      facet.points.push_back({p.x * radius, -p.y * radius});
    out.push_back(std::move(facet));
    }
  return out;
  }
  }  // namespace port_model
