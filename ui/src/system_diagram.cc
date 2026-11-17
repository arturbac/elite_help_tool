#include <system_diagram.h>

#include <eht_settings.h>
#include <port_model.h>
#include <ring_ellipse.h>
#include <star_colour.h>
#include <system_tree.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <deque>
#include <iterator>
#include <limits>
#include <map>
#include <numbers>
#include <ranges>
#include <set>
#include <string>
#include <utility>
#include <vector>

///\brief the order the game numbers bodies in - "A 10" after "A 9", not after "A 1"
[[nodiscard]]
auto natural_less(std::string_view a, std::string_view b) -> bool
  {
  auto ta{a | std::views::split(' ')};
  auto tb{b | std::views::split(' ')};
  auto ia{ta.begin()};
  auto ib{tb.begin()};
  for(; ia != ta.end() and ib != tb.end(); ++ia, ++ib)
    {
    std::string_view const x{(*ia).begin(), (*ia).end()};
    std::string_view const y{(*ib).begin(), (*ib).end()};
    if(x == y)
      continue;
    bool const nx{not x.empty() and std::ranges::all_of(x, [](unsigned char c) { return std::isdigit(c) != 0; })};
    bool const ny{not y.empty() and std::ranges::all_of(y, [](unsigned char c) { return std::isdigit(c) != 0; })};
    if(nx and ny and x.size() != y.size())
      return x.size() < y.size();
    return x < y;
    }
  return ib != tb.end();
  }

namespace
  {
namespace system_map
  {
// The sizes and colours come from the settings; what stays here is the spacing of the grid.
// Sizes keep an order, not a scale: a star twice a planet, a giant above a rocky world, a moon small.
// Drawn to scale a system would be one disc and a scatter of dust
constexpr float barycentre_radius{3.5f};
constexpr float star_column{30.f};
constexpr float column{19.f};
constexpr float label_band{16.f};
constexpr float moon_step{10.f};
constexpr float row_gap{8.f};
constexpr float port_step{10.f};

///\brief the orbital ports, each with the outline the game's map gives it; the rest - surface ports,
/// settlements, construction sites, installations, carriers - are not drawn
enum struct port_e : uint8_t
  {
  none,
  coriolis,
  orbis,
  ocellus,
  dodec,
  outpost,
  asteroid
  };

[[nodiscard]]
auto port_shape(std::string_view type) -> port_e
  {
  if(type == "Coriolis")
    return port_e::coriolis;
  // Artemis and Apollo are both Orbis to the journal
  if(type == "Orbis")
    return port_e::orbis;
  // Bernal is the older name of the same wheel
  if(type == "Ocellus" or type == "Bernal")
    return port_e::ocellus;
  if(type == "Dodec")
    return port_e::dodec;
  if(type == "Outpost")
    return port_e::outpost;
  if(type == "AsteroidBase")
    return port_e::asteroid;
  return port_e::none;
  }

[[nodiscard]]
auto port_model_of(port_e shape) -> std::optional<port_model::kind_e>
  {
  switch(shape)
    {
    case port_e::coriolis: return port_model::kind_e::coriolis;
    case port_e::orbis:    return port_model::kind_e::orbis;
    case port_e::ocellus:  return port_model::kind_e::ocellus;
    case port_e::dodec:    return port_model::kind_e::dodec;
    case port_e::outpost:  return port_model::kind_e::outpost;
    case port_e::asteroid: return port_model::kind_e::asteroid;
    case port_e::none:     break;
    }
  return std::nullopt;
  }

///\brief a polygon's colour: the hull in the port colour, the rock of an asteroid base brown-grey, both as lit
/// as the polygon is; the slot stays near black whatever the light
[[nodiscard]]
auto facet_colour(port_model::facet_t const & facet, uint32_t hull) -> uint32_t
  {
  if(facet.part == port_model::part_e::slot)
    return 0x0c1014u;
  uint32_t const base{facet.part == port_model::part_e::rock ? 0x9a8c7cu : hull};
  auto const channel = [&](uint32_t shift) -> uint32_t
  { return uint32_t(std::lround(float((base >> shift) & 0xffu) * std::clamp(facet.shade, 0.f, 1.f))) << shift; };
  return channel(16u) | channel(8u) | channel(0u);
  }

///\brief the last word of a body's name - "3" of "A 3", "a" of "A 3 a"; the row and column say the rest
[[nodiscard]]
auto last_word(std::string_view name) -> std::string_view
  {
  auto const at{name.rfind(' ')};
  return at == std::string_view::npos ? name : name.substr(at + 1);
  }

[[nodiscard]]
auto is_giant(std::string_view planet_class) -> bool
  { return planet_class.contains("gas giant") or planet_class == "Water giant"; }

[[nodiscard]]
auto has_bio(planet_details_t const & details) -> bool
  {
  return not details.genuses_.empty()
         or std::ranges::any_of(
           details.signals_,
           [](events::signal_t const & s) { return s.Count != 0u and s.Type_Localised.contains("Biological"); }
         );
  }

///\brief metal grey, rock yellow, ice white - by the class alone
[[nodiscard]]
auto class_colour(planet_details_t const & details) -> uint32_t
  {
  std::string_view const pc{details.planet_class};
  if(pc.contains("water based life"))
    return 0x6fb6c8u;
  if(pc.contains("ammonia based life"))
    return 0xc8905au;
  if(pc.contains("Helium"))
    return 0xe0d8c0u;
  if(pc == "Water giant")
    return 0x5f8fd8u;
  if(pc.contains("class I gas"))
    return 0xd8b080u;
  if(pc.contains("class II gas"))
    return 0xe8d8b0u;
  if(pc.contains("class III gas"))
    return 0xa8c8e8u;
  if(pc.contains("class IV gas"))
    return 0x9fb0c8u;
  if(pc.contains("class V gas"))
    return 0x8090b0u;
  if(pc == "Metal rich body" or pc == "High metal content body")
    return 0x9a9a9au;
  if(pc == "Rocky body")
    return 0xd9c060u;
  if(pc == "Rocky ice body")
    return 0xe8e0a8u;
  if(pc == "Icy body")
    return 0xf2f6ffu;
  if(pc == "Earthlike body")
    return 0x4fc38au;
  if(pc == "Water world")
    return 0x4a88e0u;
  if(pc == "Ammonia world")
    return 0xb07a40u;
  return 0x808080u;
  }

///\brief the class's colour - and green over all of it where there is life to sample
[[nodiscard]]
auto planet_colour(planet_details_t const & details) -> uint32_t
  {
  if(has_bio(details))
    return eht::settings()->overlay.system_map.bio.rgb;
  return class_colour(details);
  }

///\brief how much the body shines on the map: open water most, ice and clouds less, bare rock hardly
[[nodiscard]]
auto planet_gloss(planet_details_t const & details) -> float
  {
  std::string_view const pc{details.planet_class};
  if(pc == "Water world")
    return 0.8f;
  if(pc == "Earthlike body")
    return 0.6f;
  if(pc == "Water giant" or pc == "Icy body")
    return 0.5f;
  if(pc == "Metal rich body" or pc == "High metal content body" or pc == "Rocky ice body" or pc == "Ammonia world")
    return 0.3f;
  if(pc.contains("gas giant"))
    return 0.2f;
  return 0.1f;
  }

///\brief the colour of the air over a body, by its main gas - roughly the tint the game gives it
[[nodiscard]]
auto air_colour(std::string_view type) -> uint32_t
  {
  if(type.starts_with("Nitrogen"))
    return 0x9fc4ffu;
  if(type.starts_with("Oxygen") or type == "EarthLike" or type == "AmmoniaOxygen")
    return 0x6aa8ffu;
  if(type.starts_with("CarbonDioxide"))
    return 0xd8c8a0u;
  if(type.starts_with("SulphurDioxide"))
    return 0xe8d060u;
  if(type.starts_with("Ammonia"))
    return 0xd8a060u;
  if(type.starts_with("Methane"))
    return 0x7fd8d0u;
  if(type.starts_with("Water"))
    return 0xc8e0ffu;
  if(type.starts_with("Helium"))
    return 0xe8e8d8u;
  if(type.starts_with("Neon"))
    return 0xe0a0c0u;
  if(type.starts_with("Argon"))
    return 0xb0b0ffu;
  if(type.ends_with("Vapour"))
    return 0xff9060u;
  return 0xc0c8d0u;
  }

///\brief how far the air reaches over the surface, as a part of the radius: thin, plain or thick air
[[nodiscard]]
auto air_depth(planet_details_t const & details) -> float
  {
  if(details.atmosphere.empty() or details.atmosphere_type.empty() or details.atmosphere_type == "None")
    return 0.f;
  std::string_view const air{details.atmosphere};
  if(air.contains("thick"))
    return 0.42f;
  if(air.contains("thin"))
    return 0.18f;
  return 0.28f;
  }

///\brief the colour of a ring by what it is made of, the journal's eRingClass_*
[[nodiscard]]
auto ring_colour(std::string_view ring_class, eht::system_map_t const & sm) noexcept -> uint32_t
  {
  if(ring_class.ends_with("Icy"))
    return sm.ring_icy.rgb;
  if(ring_class.ends_with("Rocky"))
    return sm.ring_rocky.rgb;
  if(ring_class.ends_with("MetalRich"))
    return sm.ring_metal_rich.rgb;
  if(ring_class.ends_with("Metalic"))
    return sm.ring_metallic.rgb;
  return sm.label.rgb;
  }

///\brief the rings of a body, inside out, on a ball of this radius - the belts of a star are not rings
[[nodiscard]]
auto ball_rings(star_system_t const & system, events::body_id_t body_id, float radius, eht::system_map_t const & sm)
  -> std::vector<overlay::ball_ring_t>
  {
  std::vector<ring_t const *> own;
  for(ring_t const & ring: system.rings)
    if(ring.parent_body_id == body_id and ring.name.ends_with("Ring"))
      own.push_back(&ring);
  std::ranges::sort(own, {}, &ring_t::inner_rad);

  std::vector<overlay::ball_ring_t> rings;
  rings.reserve(own.size());
  for(size_t i{}; i != own.size(); ++i)
    rings.push_back(
      overlay::ball_ring_t{
        .radius = overlay::ring_radius(radius, i, own.size(), sm.ring_reach),
        .color = ring_colour(own[i]->ring_class, sm)
      }
    );
  return rings;
  }

///\brief a planet or moon drawn as a ball lit from the star at (sx, sy), or from the left without one
[[nodiscard]]
auto planet_disc(
  planet_details_t const & details, float x, float y, float radius, float sx, float sy, std::string face
) -> overlay::disc_t
  {
  float const dx{sx - x};
  float const dy{sy - y};
  bool const beside{dx * dx + dy * dy > 1.f};
  return overlay::disc_t{
    .x = x,
    .y = y,
    .radius = radius,
    .color = planet_colour(details),
    .sphere = true,
    .light_x = beside ? dx : -1.f,
    .light_y = beside ? dy : 0.f,
    .gloss = planet_gloss(details),
    .face = std::move(face),
    .atmosphere = air_colour(details.atmosphere_type),
    .atmosphere_depth = air_depth(details)
  };
  }

[[nodiscard]]
auto star_disc(star_details_t const & details, float x, float y, float radius) -> overlay::disc_t
  {
  return overlay::disc_t{
    .x = x, .y = y, .radius = radius, .color = star_colour(details.star_type), .sphere = true, .glows = true
  };
  }
  }  // namespace system_map

  }  // namespace

///\brief where a mission is owed: a done one goes back to whoever redirected it, an open one to the
/// place it named when it was taken - system and place
[[nodiscard]]
auto owed_place(info::mission_t const & mission) -> std::pair<std::string, std::string>
  {
  bool const done{mission.status == info::mission_status_e::redirected};
  std::string system{done ? mission.redirected_system : mission.destination_system};
  std::string place{
    done ? (mission.redirected_settlement.empty() ? mission.redirected_station : mission.redirected_settlement)
         : std::string{mission.destination_place()}
  };
  return {std::move(system), std::move(place)};
  }

namespace
  {
///\brief the places of this system the open missions send us to, true where one only waits to be handed in
[[nodiscard]]
auto mission_places(std::span<info::mission_t const> missions, std::string_view system)
  -> std::map<std::string, bool, std::less<>>
  {
  std::map<std::string, bool, std::less<>> places;
  for(info::mission_t const & mission: missions)
    {
    bool const done{mission.status == info::mission_status_e::redirected};
    if(not done and mission.status != info::mission_status_e::accepted)
      continue;
    auto [where, place]{owed_place(mission)};
    if(where != system or place.empty())
      continue;
    // a place with work still to do there is shown as such, even if another mission only waits there
    auto [it, fresh]{places.try_emplace(std::move(place), done)};
    if(not fresh)
      it->second = it->second and done;
    }
  return places;
  }

  }  // namespace

///\brief the system as the game's orrery lays it out, without its scale
///
/// A row for every star, its planets along it in the order the game numbers them, the moons hanging
/// under their planet. A row that belongs to a barycentre rather than a star starts with a ring
/// instead of a disc. Stars sharing a barycentre are bracketed on the left, planets sharing one above.
/// The hierarchy is the one the scans gave - the names only say what to write beside each disc
[[nodiscard]]
auto build_system_diagram(
  star_system_t const & system,
  std::span<info::station_t const> stations,
  std::string_view here,
  std::optional<events::status_file_t::destination_t> const & destination,
  std::span<info::mission_t const> missions,
  face_of_t const & face_of,
  bool exploring
) -> std::optional<overlay::diagram_t>
  {
  using namespace system_map;
  using events::body_id_t;

  auto const cfg{eht::settings()};
  eht::system_map_t const & sm{cfg->overlay.system_map};
  float const star_radius{sm.star_radius};
  float const giant_radius{sm.giant_radius};
  float const planet_radius{sm.planet_radius};
  float const moon_radius{sm.moon_radius};
  float const submoon_radius{sm.submoon_radius};
  float const port_size{sm.port_size};
  uint32_t const line_colour{sm.line.rgb};
  uint32_t const label_colour{sm.label.rgb};
  uint32_t const here_colour{sm.here.rgb};
  uint32_t const destination_colour{sm.destination.rgb};
  uint32_t const port_colour{sm.port.rgb};
  uint32_t const mission_colour{sm.mission.rgb};
  uint32_t const handin_colour{cfg->overlay.colours.first.rgb};
  float const arrow{sm.mission_arrow};

  std::map<body_id_t, body_t const *> by_id;
  for(body_t const & body: system.bodies)
    {
    // belt clusters come in as scans without a class - they are no bodies worth a disc
    if(auto const * pd{std::get_if<planet_details_t>(&body.details)}; pd != nullptr and pd->planet_class.empty())
      continue;
    by_id.emplace(body.body_id, &body);
    }
  if(by_id.empty())
    return std::nullopt;

  auto const planet_of = [](body_t const * body) -> planet_details_t const *
  { return std::get_if<planet_details_t>(&body->details); };

  // a moon hangs under its planet only when the planet is known; otherwise it stands in the row itself
  std::map<body_id_t, std::vector<body_t const *>> moons;
  struct row_t
    {
    body_t const * star{};
    ///\brief the barycentre a starless row belongs to
    std::optional<body_id_t> barycentre;
    std::vector<body_t const *> planets;
    };
  std::map<std::pair<int, body_id_t>, row_t> rows;  // (0 star / 1 barycentre / 2 unknown, id)

  for(auto const & [id, body]: by_id)
    if(body->body_type() == body_type_e::star)
      rows[{0, id}].star = body;

  for(auto const & [id, body]: by_id)
    {
    planet_details_t const * const pd{planet_of(body)};
    if(pd == nullptr)
      continue;
    if(pd->parent_planet and by_id.contains(*pd->parent_planet))
      moons[*pd->parent_planet].push_back(body);
    else if(pd->parent_star)
      rows[{0, *pd->parent_star}].planets.push_back(body);
    else if(pd->parent_barycenter)
      {
      row_t & row{rows[{1, *pd->parent_barycenter}]};
      row.barycentre = *pd->parent_barycenter;
      row.planets.push_back(body);
      }
    else
      rows[{2, 0u}].planets.push_back(body);
    }

  auto const by_name = [](body_t const * a, body_t const * b) { return natural_less(a->name, b->name); };
  for(auto & [key, row]: rows)
    std::ranges::sort(row.planets, by_name);
  for(auto & [id, list]: moons)
    std::ranges::sort(list, by_name);

  // the stars in the order of their letters, the rows of barycentres after them
  std::vector<row_t const *> ordered;
  for(auto const & [key, row]: rows)
    ordered.push_back(&row);
  auto const row_name = [](row_t const * row) -> std::string_view
  {
    if(row->star != nullptr)
      return row->star->name;
    return row->planets.empty() ? std::string_view{} : std::string_view{row->planets.front()->name};
  };
  std::ranges::stable_sort(
    ordered,
    [&](row_t const * a, row_t const * b)
    {
      bool const sa{a->star != nullptr};
      bool const sb{b->star != nullptr};
      if(sa != sb)
        return sa;
      return natural_less(row_name(a), row_name(b));
    }
  );

  // (PoC) in the tree's order: a barycentre's row right after its stars, brackets round each barycentre, nested
  struct tree_bracket_t
    {
    row_t const * first{};
    row_t const * last{};
    int depth{};
    std::vector<row_t const *> ticks;
    };
  std::vector<tree_bracket_t> tree_brackets;
  std::set<body_id_t> bracketed_stars;
  std::deque<row_t> merged_rows;
  if(sm.nest_barycentres)
    {
    std::vector<row_t const *> star_rows;
    std::vector<row_t const *> barycentre_rows;
    std::vector<row_t const *> rest;
    for(row_t const * row: ordered)
      if(row->star != nullptr)
        star_rows.push_back(row);
      else if(row->barycentre)
        barycentre_rows.push_back(row);
      else
        rest.push_back(row);
    // a barycentre's prefix: its first planet's name without the system's and the planet's number - "AB" of "AB 1"
    std::vector<std::string_view> prefixes;
    for(row_t const * row: barycentre_rows)
      {
      std::string_view const short_name{body_short_name(system.name, row_name(row))};
      prefixes.push_back(short_name.substr(0, std::min(short_name.size(), short_name.rfind(' '))));
      }
    std::vector<system_tree::star_t> stars;
    std::vector<system_tree::barycentre_t> barycentres;
    std::set<body_id_t> with_row;
    for(size_t ix{}; ix != barycentre_rows.size(); ++ix)
      {
      barycentres.push_back(system_tree::barycentre_t{.body_id = *barycentre_rows[ix]->barycentre, .prefix = prefixes[ix], .has_row = true});
      with_row.insert(*barycentre_rows[ix]->barycentre);
      }
    // the barycentres of stars with no planets of their own, for the brackets pairing the stars
    for(row_t const * row: star_rows)
      {
      auto const & sd{std::get<star_details_t>(row->star->details)};
      stars.push_back(system_tree::star_t{
        .body_id = row->star->body_id, .short_name = body_short_name(system.name, row->star->name), .barycentre = sd.parent_barycenter
      });
      if(sd.parent_barycenter and with_row.insert(*sd.parent_barycenter).second)
        barycentres.push_back(system_tree::barycentre_t{.body_id = *sd.parent_barycenter, .has_row = false});
      }
    system_tree::layout_t const layout{system_tree::order(stars, barycentres)};
    // the barycentres among the same stars in one row standing for the outer one (CD 1, 2, 3 of CD), its planets in
    // the game's order; those circling another of them a pair, bracketed over
    std::vector<row_t const *> layout_rows;
    for(system_tree::row_t const & r: layout.rows)
      {
      if(r.star or r.with.empty())
        {
        layout_rows.push_back(r.star ? star_rows[r.index] : barycentre_rows[r.index]);
        continue;
        }
      row_t & merged{merged_rows.emplace_back(row_t{.barycentre = r.centre, .planets = barycentre_rows[r.index]->planets})};
      for(size_t const ix: r.with)
        std::ranges::copy(barycentre_rows[ix]->planets, std::back_inserter(merged.planets));
      std::ranges::sort(merged.planets, by_name);
      layout_rows.push_back(&merged);
      }
    auto const row_at = [&](size_t at) -> row_t const * { return layout_rows[at]; };
    ordered.clear();
    for(size_t at{}; at != layout.rows.size(); ++at)
      ordered.push_back(row_at(at));
    ordered.insert(ordered.end(), rest.begin(), rest.end());
    for(system_tree::bracket_t const & b: layout.brackets)
      {
      tree_bracket_t t{.first = row_at(b.first), .last = row_at(b.last), .depth = b.depth};
      for(size_t const at: b.ticks)
        t.ticks.push_back(row_at(at));
      for(size_t at{b.first}; at <= b.last; ++at)
        if(row_t const * row{row_at(at)}; row->star != nullptr)
          bracketed_stars.insert(row->star->body_id);
      tree_brackets.push_back(std::move(t));
      }
    }
  // where each row's line stands and how wide its lead disc is, for the tree's brackets
  std::map<row_t const *, std::pair<float, float>> row_lines;

  overlay::diagram_t diagram{};
  std::string_view const here_short{here.empty() ? std::string_view{} : body_short_name(system.name, here)};

  // the destination counts only inside this system - elsewhere the game names nothing but the system
  bool const going_here{destination and destination->System == system.system_address};

  // The places the missions send us to, and the bodies of those standing on the ground - a settlement
  // is known to stand on its body from the approach; a port in space is marked where it is drawn
  auto const places{mission_places(missions, system.name)};
  std::map<body_id_t, bool> mission_bodies;
  for(info::station_t const & station: stations)
    if(auto const it{places.find(station.name)}; it != places.end() and station.body_id)
      {
      auto [at, fresh]{mission_bodies.try_emplace(*station.body_id, it->second)};
      if(not fresh)
        at->second = at->second and it->second;
      }
  // a small arrow pointing at the thing from the left, its tip just clear of the ring round it
  auto const mission_arrow = [&](float x, float y, float r, bool ready)
  {
    float const tip{x - r - 3.f};
    uint32_t const colour{ready ? handin_colour : mission_colour};
    for(auto const & [x0, y0, x1, y1]: std::array<std::array<float, 4>, 3>{
          {{-arrow, 0.f, 0.f, 0.f}, {-arrow * 0.5f, -arrow * 0.5f, 0.f, 0.f}, {-arrow * 0.5f, arrow * 0.5f, 0.f, 0.f}}
        })
      diagram.segments.push_back(
        overlay::segment_t{
          .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1, .color = colour, .relative = true, .ax = tip, .ay = y
        }
      );
  };

  // where the star of the row being drawn stands - the light comes from there
  float star_x{};
  float star_y{};
  // a planet or moon as a ball, with its face once the scanner has shown it; a face hides the colour
  // that said there is life, so a ring around the ball says it instead
  auto const body_disc = [&](body_t const * body, planet_details_t const & details, float x, float y, float r)
  {
    std::string const full{body->name.empty() ? system.name : system.name + " " + body->name};
    std::string face{face_of(system.name, full, class_colour(details))};
    bool const ringed{exploring and not face.empty() and has_bio(details)};
    overlay::disc_t & disc{diagram.discs.emplace_back(planet_disc(details, x, y, r, star_x, star_y, std::move(face)))};
    disc.body = body->body_id;
    disc.rings = ball_rings(system, body->body_id, r, sm);
    // the journal says how the planet leans and nothing of its rings, which lie round its equator
    disc.ring_lean = overlay::ring_lean(details.axial_tilt.value_or(0.0));
    if(ringed)
      diagram.discs.push_back(
        overlay::disc_t{.x = x, .y = y, .radius = r + 1.5f, .color = sm.bio.rgb, .outline = true}
      );
  };

  // what a body's name is written in: mapped already, worth mapping and not yet, or neither
  uint32_t const minimum_value{cfg->overlay.minimum_body_value};
  auto const name_colour = [&](body_t const * body, planet_details_t const & details) -> uint32_t
  {
    if(details.mapped)
      return sm.mapped.rgb;
    if(body->value >= minimum_value)
      return sm.to_map.rgb;
    return label_colour;
  };

  auto const mark_here = [&](body_t const * body, float x, float y, float r)
  {
    if(auto const it{mission_bodies.find(body->body_id)}; it != mission_bodies.end())
      mission_arrow(x, y, r, it->second);
    if(not here_short.empty() and body->name == here_short)
      diagram.discs.push_back(overlay::disc_t{.x = x, .y = y, .radius = r + 3.5f, .color = here_colour, .outline = true});
    // a surface port's destination names the body it stands on
    if(going_here and destination->Body == body->body_id)
      diagram.discs.push_back(
        overlay::disc_t{.x = x, .y = y, .radius = r + 6.f, .color = destination_colour, .outline = true}
      );
  };

  // An orbital port, attached to the body at its own distance from the star: the journal never says
  // what a station circles, but it circles close, and bodies of a system lie hundreds of seconds apart
  std::map<body_id_t, std::vector<info::station_t const *>> ports;
  for(info::station_t const & station: stations)
    {
    if(station.dist_from_star_ls <= 0.0 or port_shape(station.station_type) == port_e::none)
      continue;
    auto const nearest_of = [&](bool stars_only) -> std::pair<body_t const *, double>
    {
      body_t const * nearest{};
      double gap{std::numeric_limits<double>::max()};
      for(auto const & [id, body]: by_id)
        if(stars_only and body->body_type() != body_type_e::star)
          continue;
        else if(double const g{std::abs(body->distance_from_arrival_ls - station.dist_from_star_ls)}; g < gap)
          {
          gap = g;
          nearest = body;
          }
      return {nearest, gap};
    };
    auto [nearest, gap]{nearest_of(false)};
    // no body where the port is: it circles the star itself, as Coppel City does at 24 ls with nothing
    // else inside 300 - or a body never scanned, and then its star's row is still the right place
    if(nearest == nullptr or gap > std::max(5.0, station.dist_from_star_ls * 0.015))
      nearest = nearest_of(true).first;
    if(nearest == nullptr)
      continue;
    // a port by a moon stands in its planet's column, where the moon hangs too
    body_t const * owner{nearest};
    while(auto const * pd{planet_of(owner)})
      {
      if(not pd->parent_planet or not by_id.contains(*pd->parent_planet))
        break;
      owner = by_id.at(*pd->parent_planet);
      }
    ports[owner->body_id].push_back(&station);
    }

  auto const draw_port = [&](info::station_t const & station, float x, float y)
  {
    float const s{port_size};
    // a small model of the port, lit from the row's star; the layer only fills what it is sent
    if(auto const kind{port_model_of(port_shape(station.station_type))}; kind)
      for(port_model::facet_t const & facet: port_model::facets(*kind, s, star_x - x, star_y - y))
        {
        overlay::facet_t out{.ax = x, .ay = y, .points = {}, .color = facet_colour(facet, port_colour)};
        // two decimals are a hundredth of a pixel at scale 1, and keep the frame short
        for(auto const & [px, py]: facet.points)
          out.points.push_back(
            overlay::point_t{.x = std::round(px * 100.f) / 100.f, .y = std::round(py * 100.f) / 100.f}
          );
        diagram.facets.push_back(std::move(out));
        }
    if(not here.empty() and station.name == here)
      diagram.discs.push_back(
        overlay::disc_t{.x = x, .y = y, .radius = s + 3.5f, .color = here_colour, .outline = true}
      );
    if(going_here and station.name == destination->Name)
      diagram.discs.push_back(
        overlay::disc_t{.x = x, .y = y, .radius = s + 6.f, .color = destination_colour, .outline = true}
      );
    if(auto const it{places.find(station.name)}; it != places.end())
      mission_arrow(x, y, s, it->second);
  };

  // A star with nothing around it would take a whole row for one disc - in a system of five stars and
  // two of them with planets that is most of the height. Such stars stand side by side in one row,
  // unless one pairs with a star that has a row of its own: the bracket on the left must reach both
  auto const lone = [&](row_t const * row)
  {
    return row->star != nullptr and row->planets.empty() and not ports.contains(row->star->body_id)
           and not bracketed_stars.contains(row->star->body_id);
  };
  std::set<body_id_t> split_pairs;
  for(row_t const * row: ordered)
    if(row->star != nullptr and not lone(row))
      if(auto const & sd{std::get<star_details_t>(row->star->details)}; sd.parent_barycenter)
        split_pairs.insert(*sd.parent_barycenter);
  auto const packed = [&](row_t const * row)
  {
    if(not lone(row))
      return false;
    auto const & sd{std::get<star_details_t>(row->star->details)};
    return not sd.parent_barycenter or not split_pairs.contains(*sd.parent_barycenter);
  };
  std::vector<body_t const *> packed_stars;
  for(row_t const * row: ordered)
    if(packed(row))
      packed_stars.push_back(row->star);

  // the star rows, remembered for the brackets pairing stars around a shared barycentre
  std::map<body_id_t, std::vector<float>> star_pairs;
  float top{};
  float widest{};

  for(row_t const * row: ordered)
    {
    if(packed(row))
      {
      // the whole group is drawn where its first star stands in the order, the rest are already in it
      if(row->star != packed_stars.front())
        continue;
      float const line_y{top + label_band + star_radius};
      float bottom{line_y + star_radius};
      std::map<body_id_t, std::vector<float>> pairs;
      for(size_t ix{}; ix != packed_stars.size(); ++ix)
        {
        body_t const * const star{packed_stars[ix]};
        auto const & sd{std::get<star_details_t>(star->details)};
        float const x{(static_cast<float>(ix) + 0.5f) * star_column};
        diagram.discs.push_back(
          star_disc(sd, x, line_y, star_radius)
        );
        diagram.discs.back().body = star->body_id;
        diagram.labels.push_back(
          overlay::label_t{
            .x = x, .y = line_y - star_radius - 5.f, .text = star->name, .color = label_colour, .align = 0.5f
          }
        );
        mark_here(star, x, line_y, star_radius);
        if(sd.parent_barycenter)
          pairs[*sd.parent_barycenter].push_back(x);
        }
      // side by side the pair is bracketed underneath, where the names do not stand
      float const bracket_y{line_y + star_radius + 4.f};
      for(auto const & [id, xs]: pairs)
        {
        if(xs.size() < 2u)
          continue;
        diagram.segments.push_back(
          overlay::segment_t{.x0 = xs.front(), .y0 = bracket_y, .x1 = xs.back(), .y1 = bracket_y, .color = label_colour}
        );
        for(float const x: xs)
          diagram.segments.push_back(
            overlay::segment_t{.x0 = x, .y0 = bracket_y, .x1 = x, .y1 = line_y + star_radius, .color = label_colour}
          );
        bottom = bracket_y + 2.f;
        }
      widest = std::max(widest, static_cast<float>(packed_stars.size()) * star_column + 6.f);
      top = bottom + row_gap;
      continue;
      }

    bool const any_giant{std::ranges::any_of(
      row->planets, [&](body_t const * b) { return is_giant(planet_of(b)->planet_class); }
    )};
    float const lead{row->star != nullptr ? star_radius : (any_giant ? giant_radius : planet_radius)};
    float const line_y{top + label_band + lead};
    float const lead_x{star_column / 2.f};
    float bottom{line_y + lead};
    row_lines[row] = {line_y, row->star != nullptr ? star_radius : barycentre_radius};

    float const last_x{
      row->planets.empty() ? lead_x : star_column + (static_cast<float>(row->planets.size()) - 0.5f) * column
    };
    if(not row->planets.empty())
      diagram.segments.push_back(
        overlay::segment_t{.x0 = lead_x, .y0 = line_y, .x1 = last_x, .y1 = line_y, .color = line_colour}
      );

    if(row->star != nullptr)
      {
      auto const & sd{std::get<star_details_t>(row->star->details)};
      diagram.discs.push_back(
        star_disc(sd, lead_x, line_y, star_radius)
      );
      diagram.discs.back().body = row->star->body_id;
      diagram.labels.push_back(
        overlay::label_t{
          .x = lead_x, .y = line_y - star_radius - 5.f, .text = row->star->name, .color = label_colour, .align = 0.5f
        }
      );
      mark_here(row->star, lead_x, line_y, star_radius);
      if(sd.parent_barycenter)
        star_pairs[*sd.parent_barycenter].push_back(line_y);

      // ports circling the star itself stack under it
      if(auto const it{ports.find(row->star->body_id)}; it != ports.end())
        {
        float py{line_y + star_radius + 4.f + port_size};
        star_x = lead_x;
        star_y = line_y;
        for(info::station_t const * station: it->second)
          {
          draw_port(*station, lead_x, py);
          bottom = std::max(bottom, py + port_size);
          py += port_step;
          }
        }
      }
    else
      {
      diagram.discs.push_back(
        overlay::disc_t{
          .x = lead_x, .y = line_y, .radius = barycentre_radius, .color = line_colour, .outline = true
        }
      );
      // the row's name is what its planets are called without their number - "BC" of "BC 1"
      std::string_view const first{row_name(row)};
      std::string_view const prefix{first.substr(0, std::min(first.size(), first.rfind(' ')))};
      diagram.labels.push_back(
        overlay::label_t{
          .x = lead_x, .y = line_y - star_radius - 5.f, .text = std::string{prefix}, .color = label_colour, .align = 0.5f
        }
      );
      }

    // planets sharing a barycentre of their own - one lower in the tree than the star - get a bracket
    // over them; a barycentre above the star is the star's own orbit and pairs nothing in this row
    std::map<body_id_t, std::vector<float>> planet_pairs;

    for(size_t ix{}; ix != row->planets.size(); ++ix)
      {
      body_t const * const planet{row->planets[ix]};
      planet_details_t const & pd{*planet_of(planet)};
      float const x{star_column + (static_cast<float>(ix) + 0.5f) * column};
      float const r{is_giant(pd.planet_class) ? giant_radius : planet_radius};

      star_x = lead_x;
      star_y = line_y;
      body_disc(planet, pd, x, line_y, r);
      diagram.labels.push_back(
        overlay::label_t{
          .x = x, .y = line_y - giant_radius - 9.f, .text = std::string{last_word(planet->name)}, .color = name_colour(planet, pd),
          .align = 0.5f
        }
      );
      mark_here(planet, x, line_y, r);

      if(row->star != nullptr and pd.parent_barycenter and *pd.parent_barycenter > row->star->body_id)
        planet_pairs[*pd.parent_barycenter].push_back(x);
      else if(row->star == nullptr and row->barycentre and pd.parent_barycenter and *pd.parent_barycenter != *row->barycentre)
        planet_pairs[*pd.parent_barycenter].push_back(x);

      // the moons in a column under the planet, a moon's own moons right after it and smaller
      float y{line_y + r + 5.f};
      float column_end{};
      auto const hang = [&](this auto const & self, body_id_t parent, int depth) -> void
      {
        auto const it{moons.find(parent)};
        if(it == moons.end())
          return;
        for(body_t const * moon: it->second)
          {
          planet_details_t const & md{*planet_of(moon)};
          float const mr{depth == 1 ? (is_giant(md.planet_class) ? planet_radius : moon_radius) : submoon_radius};
          y += mr;
          body_disc(moon, md, x, y, mr);
          diagram.labels.push_back(
            overlay::label_t{
              .x = x + mr + 5.5f, .y = y, .text = std::string{last_word(moon->name)}, .color = name_colour(moon, md), .align = 0.f
            }
          );
          mark_here(moon, x, y, mr);
          column_end = y;
          y += mr + moon_step - 2.f * moon_radius;
          self(moon->body_id, depth + 1);
          }
      };
      hang(planet->body_id, 1);

      // the ports after the moons, at the foot of the column
      if(auto const it{ports.find(planet->body_id)}; it != ports.end())
        for(info::station_t const * station: it->second)
          {
          y += port_size;
          draw_port(*station, x, y);
          column_end = y;
          y += port_size + port_step - 2.f * port_size;
          }

      if(column_end > 0.f)
        {
        diagram.segments.insert(
          diagram.segments.begin(),
          overlay::segment_t{.x0 = x, .y0 = line_y, .x1 = x, .y1 = column_end, .color = line_colour}
        );
        bottom = std::max(bottom, column_end + moon_radius);
        }
      }

    float const bracket_y{line_y - giant_radius - 3.f};
    for(auto const & [id, xs]: planet_pairs)
      {
      if(xs.size() < 2u)
        continue;
      diagram.segments.push_back(
        overlay::segment_t{.x0 = xs.front(), .y0 = bracket_y, .x1 = xs.back(), .y1 = bracket_y, .color = label_colour}
      );
      for(float const x: xs)
        diagram.segments.push_back(
          overlay::segment_t{.x0 = x, .y0 = bracket_y, .x1 = x, .y1 = line_y - planet_radius, .color = label_colour}
        );
      }

    widest = std::max(widest, last_x + column / 2.f + 6.f);
    top = bottom + row_gap;
    }

  // the tree's brackets, the inner ones nearest the discs
  for(tree_bracket_t const & b: tree_brackets)
    {
    constexpr float lead_x{star_column / 2.f};
    float const off{-star_radius - 4.f - 4.f * static_cast<float>(b.depth)};
    float const y0{row_lines[b.first].first};
    diagram.segments.push_back(
      overlay::segment_t{
        .x0 = off, .y0 = 0.f, .x1 = off, .y1 = row_lines[b.last].first - y0, .color = label_colour, .relative = true,
        .ax = lead_x, .ay = y0
      }
    );
    for(row_t const * row: b.ticks)
      diagram.segments.push_back(
        overlay::segment_t{
          .x0 = off, .y0 = 0.f, .x1 = -row_lines[row].second, .y1 = 0.f, .color = label_colour, .relative = true,
          .ax = lead_x, .ay = row_lines[row].first
        }
      );
    }

  // the stars pairing around a barycentre of their own, bracketed on the left as the game does it
  for(auto const & [id, ys]: star_pairs)
    {
    if(sm.nest_barycentres)
      break;
    if(ys.size() < 2u)
      continue;
    // measured from the discs rather than from the edge, so it stays against them when the band stretches
    constexpr float lead_x{star_column / 2.f};
    float const off{-star_radius - 4.f};
    diagram.segments.push_back(
      overlay::segment_t{
        .x0 = off, .y0 = 0.f, .x1 = off, .y1 = ys.back() - ys.front(), .color = label_colour, .relative = true,
        .ax = lead_x, .ay = ys.front()
      }
    );
    for(float const y: ys)
      diagram.segments.push_back(
        overlay::segment_t{
          .x0 = off, .y0 = 0.f, .x1 = -star_radius, .y1 = 0.f, .color = label_colour, .relative = true, .ax = lead_x,
          .ay = y
        }
      );
    }

  diagram.width = std::max(widest, star_column);
  diagram.height = top;
  // Artur's choice: as wide as the window above, twice the height of the rest - so a system of more stars is
  // simply taller, by the same amount for each row
  diagram.zoom = sm.zoom;
  // about as wide as the window at the top of the band, which is half of it
  diagram.share = sm.share;
  return diagram;
  }
