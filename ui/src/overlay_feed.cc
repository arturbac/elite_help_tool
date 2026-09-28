#include <overlay_feed.h>
#include <commodity_facts.h>
#include <construction_window.h>
#include <eht_settings.h>
#include <qformat.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <functional>
#include <cctype>
#include <format>
#include <map>
#include <set>
#include <ranges>

namespace
  {
///\brief the interfaces that take over the middle screen, from GuiFocus
///\detail 5 station services, 6 galaxy map, 7 system map, 8 orrery, 9 FSS, 10 surface scanner,
/// 11 codex. The cockpit panels below 5 leave the middle of the screen alone, and so may we
constexpr uint32_t first_fullscreen_interface{5u};

// What used to be constants here now comes from the settings file, read afresh at every use so that a
// saved change shows at the next refresh. Each accessor takes the snapshot in force at that moment
auto colour_heading() -> uint32_t { return eht::settings()->overlay.colours.heading.rgb; }
auto colour_plain() -> uint32_t { return eht::settings()->overlay.colours.plain.rgb; }
auto colour_alert() -> uint32_t { return eht::settings()->overlay.colours.alert.rgb; }
///\brief undiscovered by anyone - that is the case worth stopping for
auto colour_first() -> uint32_t { return eht::settings()->overlay.colours.first.rgb; }
auto colour_expiring() -> uint32_t { return eht::settings()->overlay.colours.expiring.rgb; }

///\brief the blocks go out when the tool falls silent - better no text than text from an hour ago
auto block_ttl_ms() -> uint32_t { return eht::settings()->overlay.refresh.block_ttl_ms; }
///\brief an unchanged picture has to be repeated anyway, or it expires on a player standing still
auto heartbeat() -> std::chrono::seconds { return std::chrono::seconds{eht::settings()->overlay.refresh.heartbeat_s}; }

///\brief below this threshold going down to a body does not pay for the time it takes
auto minimum_body_value() -> uint32_t { return eht::settings()->overlay.minimum_body_value; }
///\brief the side band has its limits, and a long list will not be read in flight anyway
auto listed_bodies() -> size_t { return eht::settings()->overlay.lists.bodies; }
auto listed_factions() -> size_t { return eht::settings()->overlay.lists.factions; }
auto listed_missions() -> size_t { return eht::settings()->overlay.lists.missions; }
///\brief how many rows of a list to show - one past the limit is shown too, since a "... and 1 more" row
/// would take the room of the row it stands for
auto rows_for(size_t total, size_t limit) noexcept -> size_t { return total == limit + 1u ? total : std::min(total, limit); }
auto listed_cargo() -> size_t { return eht::settings()->overlay.lists.cargo; }
auto listed_commodities() -> size_t { return eht::settings()->overlay.lists.commodities; }
///\brief below this much time in hand a mission is a problem, not a plan
auto expiry_warning() -> std::chrono::hours { return std::chrono::hours{eht::settings()->overlay.expiry_warning_h}; }
///\brief smaller departures from the galactic average are not worth showing
auto interesting_deviation() -> double { return eht::settings()->overlay.interesting_deviation; }
///\brief a percentage without an amount lies - 93% off a commodity worth 20 Cr saves nothing that matters
auto interesting_margin() -> uint32_t { return eht::settings()->overlay.interesting_margin; }
///\brief influence updates once a day; asking more often serves nothing
auto faction_refresh() -> std::chrono::seconds { return std::chrono::seconds{eht::settings()->overlay.refresh.factions_s}; }
///\brief a market can appear at any moment, as soon as the player opens it
auto market_refresh() -> std::chrono::seconds { return std::chrono::seconds{eht::settings()->overlay.refresh.market_s}; }
///\brief missions arrive rarely, and the query goes across both databases
auto supply_refresh() -> std::chrono::seconds { return std::chrono::seconds{eht::settings()->overlay.refresh.supply_s}; }
///\brief Status.json is rewritten whenever anything in it changes, so it is read often but cheaply
auto status_refresh() -> std::chrono::milliseconds
  { return std::chrono::milliseconds{eht::settings()->overlay.refresh.status_ms}; }
auto listed_sources() -> size_t { return eht::settings()->overlay.lists.sources; }
///\brief the band is not a mission log - beyond this the list stops being read at a glance
auto listed_settlement_work() -> size_t { return eht::settings()->overlay.lists.settlement_work; }
auto listed_trades() -> unsigned { return eht::settings()->overlay.lists.trades; }

///\brief how long a kill stays on the head-up display after it is made
auto kill_shown() -> std::chrono::seconds { return std::chrono::seconds{eht::settings()->overlay.kill_shown_s}; }

///\brief the rank ladder the game counts hired pilots on, the same one it counts the commander on
[[nodiscard]]
auto combat_rank_name(uint32_t rank) -> std::string_view
  {
  using namespace std::string_view_literals;
  constexpr std::array ladder{
    "Harmless"sv,
    "Mostly Harmless"sv,
    "Novice"sv,
    "Competent"sv,
    "Expert"sv,
    "Master"sv,
    "Dangerous"sv,
    "Deadly"sv,
    "Elite"sv
  };
  return rank < ladder.size() ? ladder[rank] : "unknown"sv;
  }

///\brief what the legal status means for the trigger finger
///\detail green is a payday, red is a crime, and the difference is the whole reason for scanning
[[nodiscard]]
auto legal_colour(std::string_view status) -> uint32_t
  {
  if(status == "Wanted")
    return colour_first();
  if(status == "Clean")
    return colour_expiring();
  return colour_plain();
  }

///\brief a port in space carries the most goods, a settlement the least - that is the order of worth
[[nodiscard]]
auto station_rank(std::string_view station_type) -> int
  {
  using namespace std::string_view_literals;
  constexpr std::array space{
    "Coriolis"sv,
    "Orbis"sv,
    "Ocellus"sv,
    "Dodec"sv,
    "AsteroidBase"sv,
    "MegaShip"sv,
    "Outpost"sv,
    "SpaceConstructionDepot"sv
  };
  constexpr std::array planetary{
    "CraterPort"sv, "CraterOutpost"sv, "SurfaceStation"sv, "PlanetaryConstructionDepot"sv, "DockablePlanetStation"sv
  };

  if(std::ranges::contains(space, station_type))
    return 0;
  if(std::ranges::contains(planetary, station_type))
    return 1;
  if(station_type == "OnFootSettlement")
    return 2;
  return 3;
  }

///\brief the same colours as in the reputation window - red federation, blue empire, green alliance
[[nodiscard]]
auto allegiance_colour(info::allegiance_e allegiance) -> uint32_t
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation: return 0xd9534fu;
    case empire:     return 0x4a90d9u;
    case alliance:   return 0x3cb371u;
    default:         return colour_plain();
    }
  }

///\brief the superpower's emblem, for the allegiances that have one
///\detail an independent faction answers to nobody, so it gets no emblem and the space stays empty -
/// a placeholder there would say something the game does not
[[nodiscard]]
auto allegiance_emblem(info::allegiance_e allegiance) -> overlay::emblem_e
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation: return overlay::emblem_e::federation;
    case empire:     return overlay::emblem_e::empire;
    case alliance:   return overlay::emblem_e::alliance;
    default:         return overlay::emblem_e::none;
    }
  }

///\brief separates factions that share an allegiance, without losing what the colour says
///\detail allegiance decides the hue, so four independents come out as one grey mass and their lines
/// cannot be followed. The hue stays - it is the part that says Federation or Empire - and only the
/// brightness steps down, far enough to tell two lines apart and not so far that the text stops
/// reading against the dark band
[[nodiscard]]
auto shade(uint32_t rgb, size_t step) -> uint32_t
  {
  constexpr std::array steps{1.0, 0.82, 0.68, 0.58, 0.50};
  double const factor{steps[std::min(step, steps.size() - 1u)]};

  auto const channel = [factor](uint32_t value) -> uint32_t
  { return static_cast<uint32_t>(std::lround(static_cast<double>(value) * factor)) & 0xffu; };

  return (channel((rgb >> 16u) & 0xffu) << 16u) | (channel((rgb >> 8u) & 0xffu) << 8u) | channel(rgb & 0xffu);
  }

[[nodiscard]]
auto same_content(overlay::frame_t const & left, overlay::frame_t const & right) -> bool
  {
  // The whole frame, not a chosen part of it: comparing only the text let a moved ring on the system
  // picture, a new colour or a new layout wait for the keep-alive before reaching the game. A frame
  // is a few kilobytes and is built a few times a second, so writing both out is nothing
  std::string a;
  std::string b;
  overlay::frame_t l{left};
  overlay::frame_t r{right};
  l.seq = 0u;
  r.seq = 0u;
  if(glz::write_json(l, a) or glz::write_json(r, b)) [[unlikely]]
    return false;
  return a == b;
  }

///\brief the game repeats the system's name inside a body's name - in a side band that is pure waste of room
[[nodiscard]]
auto short_body_name(std::string const & system_name, std::string const & body_name) -> std::string
  {
  if(body_name.size() > system_name.size() + 1u and body_name.starts_with(system_name))
    return body_name.substr(system_name.size() + 1u);
  return body_name;
  }

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

///\brief the last word of a body's name - "3" of "A 3", "a" of "A 3 a"; the row and column say the rest
[[nodiscard]]
auto last_word(std::string_view name) -> std::string_view
  {
  auto const at{name.rfind(' ')};
  return at == std::string_view::npos ? name : name.substr(at + 1);
  }

[[nodiscard]]
auto star_colour(std::string_view type) -> uint32_t
  {
  // the exotic ones first, because several of them begin with the letter of an ordinary class
  if(type.starts_with("TTS"))
    return 0xffb070u;
  if(type.starts_with("AeBe"))
    return 0xf0f0ffu;
  if(type == "H" or type.contains("BlackHole"))
    return 0x505050u;
  if(type == "N")
    return 0x9fd8ffu;
  if(type.starts_with('D'))
    return 0xe8f0ffu;
  if(type.starts_with('W'))
    return 0x8fa8ffu;
  if(type.starts_with('C') or type.starts_with('S') or type == "MS")
    return 0xd05030u;
  switch(type.empty() ? '?' : type.front())
    {
    case 'O': return 0x9bb0ffu;
    case 'B': return 0xaabfffu;
    case 'A': return 0xdbe4ffu;
    case 'F': return 0xfff6e0u;
    // the tints the game's own system map gives them - an M star is peach there, not red, and the red
    // is left to the brown dwarfs, darkening as they cool
    case 'G': return 0xfff0a0u;
    case 'K': return 0xffcf7au;
    case 'M': return 0xffa468u;
    case 'L': return 0xd83c6au;
    case 'T': return 0xa8306eu;
    case 'Y': return 0x74304cu;
    default:  return 0xccccccu;
    }
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

///\brief metal grey, rock yellow, ice white - and green over all of it where there is life to sample
[[nodiscard]]
auto planet_colour(planet_details_t const & details) -> uint32_t
  {
  if(has_bio(details))
    return eht::settings()->overlay.system_map.bio.rgb;
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
  }  // namespace system_map

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
  std::span<info::mission_t const> missions
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
    // drawn relative to the port's centre, so the outline keeps its shape however far the band stretches
    auto const line = [&](float x0, float y0, float x1, float y1)
    {
      diagram.segments.push_back(
        overlay::segment_t{
          .x0 = x0 - x, .y0 = y0 - y, .x1 = x1 - x, .y1 = y1 - y, .color = port_colour, .relative = true, .ax = x, .ay = y
        }
      );
    };
    auto const polygon = [&](int sides, float turn)
    {
      for(int i{}; i != sides; ++i)
        {
        float const a0{turn + 6.2831853f * static_cast<float>(i) / static_cast<float>(sides)};
        float const a1{turn + 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(sides)};
        line(x + s * std::cos(a0), y + s * std::sin(a0), x + s * std::cos(a1), y + s * std::sin(a1));
        }
    };
    switch(port_shape(station.station_type))
      {
      case port_e::coriolis: polygon(4, 0.7853982f); break;
      case port_e::dodec:    polygon(5, -1.5707963f); break;
      case port_e::orbis:
        diagram.discs.push_back(overlay::disc_t{.x = x, .y = y, .radius = s, .color = port_colour, .outline = true});
        diagram.discs.push_back(overlay::disc_t{.x = x, .y = y, .radius = 1.f, .color = port_colour});
        break;
      case port_e::ocellus:
        diagram.discs.push_back(overlay::disc_t{.x = x, .y = y, .radius = s, .color = port_colour, .outline = true});
        diagram.discs.push_back(
          overlay::disc_t{.x = x, .y = y, .radius = s * 0.45f, .color = port_colour, .outline = true}
        );
        break;
      case port_e::outpost:
        line(x - s * 0.5f, y - s, x - s * 0.5f, y + s);
        line(x - s * 0.5f, y + s, x + s * 0.8f, y + s);
        break;
      case port_e::asteroid: polygon(3, -1.5707963f); break;
      case port_e::none:     break;
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
  { return row->star != nullptr and row->planets.empty() and not ports.contains(row->star->body_id); };
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
          overlay::disc_t{.x = x, .y = line_y, .radius = star_radius, .color = star_colour(sd.star_type)}
        );
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
        overlay::disc_t{.x = lead_x, .y = line_y, .radius = star_radius, .color = star_colour(sd.star_type)}
      );
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

      diagram.discs.push_back(overlay::disc_t{.x = x, .y = line_y, .radius = r, .color = planet_colour(pd)});
      diagram.labels.push_back(
        overlay::label_t{
          .x = x, .y = line_y - giant_radius - 9.f, .text = std::string{last_word(planet->name)}, .color = label_colour,
          .align = 0.5f
        }
      );
      mark_here(planet, x, line_y, r);

      if(row->star != nullptr and pd.parent_barycenter and *pd.parent_barycenter > row->star->body_id)
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
          diagram.discs.push_back(overlay::disc_t{.x = x, .y = y, .radius = mr, .color = planet_colour(md)});
          diagram.labels.push_back(
            overlay::label_t{
              .x = x + mr + 5.5f, .y = y, .text = std::string{last_word(moon->name)}, .color = label_colour, .align = 0.f
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

  // the stars pairing around a barycentre of their own, bracketed on the left as the game does it
  for(auto const & [id, ys]: star_pairs)
    {
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

///\brief the internal name is readable but ugly - a capital letter is enough where there is no translation
[[nodiscard]]
auto readable_name(events::cargo_item_t const & item) -> std::string
  {
  if(not item.Name_Localised.empty())
    return item.Name_Localised;

  std::string name{item.Name};
  if(not name.empty())
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
  return name;
  }

///\brief cargo left on board blocks calling the ship down to a settlement's pad
[[nodiscard]]
auto describe_cargo(events::cargo_file_t const & cargo) -> std::vector<overlay::line_t>
  {
  if(cargo.timestamp == std::chrono::sys_seconds{})
    return {};

  std::vector<overlay::line_t> lines;

  if(cargo.Count == 0u)
    {
    lines.push_back(overlay::line_t{.text = "cargo: empty", .color = colour_plain()});
    return lines;
    }

  lines.push_back(overlay::line_t{.text = std::format("cargo: {} t", cargo.Count), .color = colour_alert()});

  auto sorted{cargo.Inventory};
  std::ranges::sort(sorted, std::ranges::greater{}, &events::cargo_item_t::Count);

  size_t const cargo_rows{rows_for(sorted.size(), listed_cargo())};
  for(events::cargo_item_t const & item: sorted | std::views::take(cargo_rows))
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {}  {} t{}",
          readable_name(item),
          item.Count,
          item.Stolen != 0u ? std::format("  {} stolen", item.Stolen) : ""
        ),
        .color = item.Stolen != 0u ? colour_expiring() : colour_plain()
      }
    );

  if(sorted.size() > cargo_rows)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", sorted.size() - cargo_rows), .color = colour_plain()}
    );

  return lines;
  }

///\brief less than an hour left is a different kind of news than a few days left
[[nodiscard]]
auto format_remaining(std::chrono::seconds left) -> std::string
  {
  if(left <= std::chrono::seconds::zero())
    return "expired";

  auto const days{std::chrono::duration_cast<std::chrono::days>(left)};
  auto const hours{std::chrono::duration_cast<std::chrono::hours>(left - days)};
  if(days.count() != 0)
    return std::format("{}d {}h", days.count(), hours.count());

  auto const minutes{std::chrono::duration_cast<std::chrono::minutes>(left - hours)};
  if(hours.count() != 0)
    return std::format("{}h {}m", hours.count(), minutes.count());

  return std::format("{}m", minutes.count());
  }

///\brief the game's own wording for a mission, as the journal hands it over in LocalisedName
///\detail "Exterminate Cartel of HIP 83983 members" is what the player reads on the board, while the
/// type behind it is a token meant for the game. The token is only a fallback for rows stored before
/// the field was kept
[[nodiscard]]
auto mission_wording(info::mission_t const & mission) -> std::string
  {
  std::string wording{
    mission.description.empty() ? info::transform_mission_name(mission.type) : mission.description
  };

  // Whose man is being killed. For a kill in space the journal does say it - "Assassinate Politician:
  // Rayner" comes with The Mercs of Mikunn attached - and it is the faction that pays for the deed in
  // influence, which the sentence itself never mentions. An extermination contract already names them
  // ("Exterminate Cartel of HIP 83983 members"), so it is added only where it is not there already
  if(not mission.target_faction.empty() and wording.find(mission.target_faction) == std::string::npos)
    wording += std::format("  ({})", mission.target_faction);

  return wording;
  }

///\brief the open missions, gathered under the place they are owed to
///
/// Flat, the list said the same system five times over and left the eye to group it. Missions are
/// travelled to system by system, so that is what the rows hang under; where a system holds a single
/// stop, its name joins the heading instead of repeating on every row. A redirected mission is done
/// and waits only to be handed in, which is a different errand from the rest and carries a mark.
[[nodiscard]]
auto describe_missions(
  std::vector<info::mission_t> const & missions,
  std::map<std::pair<std::string, std::string>, std::string> const & owners
) -> std::vector<overlay::line_t>
  {
  std::vector<info::mission_t const *> open;
  for(info::mission_t const & mission: missions)
    if(mission.status == info::mission_status_e::accepted or mission.status == info::mission_status_e::redirected)
      open.push_back(&mission);

  if(open.empty())
    return {};

  std::ranges::sort(open, {}, [](info::mission_t const * mission) { return mission->expiry; });

  auto const ready{std::ranges::count_if(
    open, [](info::mission_t const * mission) { return mission->status == info::mission_status_e::redirected; }
  )};

  auto const destination = owed_place;

  // Who holds the place a mission points at. The journal does not say: "Kill Casey Sanders" comes
  // with a settlement and a name and nothing else, yet the killing counts against the faction that
  // holds the settlement - which is the whole reason for going, or for not going
  auto const with_owner = [&owners, &destination](info::mission_t const & mission, std::string const & place)
    -> std::string
  {
    if(place.empty())
      return {};

    auto const it{owners.find(std::pair{destination(mission).first, place})};
    return it == owners.end() or it->second.empty() ? place : std::format("{} ({})", place, it->second);
  };

  struct group_t
    {
    std::string system;
    std::vector<info::mission_t const *> rows;
    };

  // the groups keep the order the missions came in, which is by expiry - so the system that runs out
  // first stands first
  std::vector<group_t> groups;
  for(info::mission_t const * mission: open)
    {
    auto const [system, place]{destination(*mission)};
    auto const key{system.empty() ? std::string{"no fixed destination"} : system};

    auto it{std::ranges::find(groups, key, &group_t::system)};
    if(it == groups.end())
      {
      groups.push_back(group_t{.system = key, .rows = {}});
      it = std::prev(groups.end());
      }
    it->rows.push_back(mission);
    }

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = ready == 0 ? std::format("missions: {} open", open.size())
                         : std::format("missions: {} open, {} to hand in", open.size(), ready),
      .color = colour_heading()
    }
  );

  size_t shown{};
  size_t const mission_rows{rows_for(open.size(), listed_missions())};
  for(group_t const & group: groups)
    {
    if(shown >= mission_rows)
      break;

    // one stop in the system means the place belongs in the heading, not on every row under it
    std::set<std::string> places;
    for(info::mission_t const * mission: group.rows)
      if(auto const place{destination(*mission).second}; not place.empty())
        places.insert(place);

    bool const single_place{places.size() == 1u};
    lines.push_back(
      overlay::line_t{
        .text = single_place ? std::format("{}  -  {}", group.system, with_owner(*group.rows.front(), *places.begin()))
                             : group.system,
        .color = colour_heading()
      }
    );

    for(info::mission_t const * mission: group.rows)
      {
      if(shown >= mission_rows)
        break;
      ++shown;

      auto const left{std::chrono::duration_cast<std::chrono::seconds>(mission->expiry - now)};
      bool const done{mission->status == info::mission_status_e::redirected};
      std::string const place{single_place ? std::string{} : destination(*mission).second};

      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}{}  {}{}  {}",
            done ? "> " : "  ",
            format_remaining(left),
            mission->faction,
            place.empty() ? std::string{} : std::format("  {}", with_owner(*mission, place)),
            mission_wording(*mission)
          ),
          // green is ready to hand in, red is about to be lost
          .color = left < expiry_warning() ? colour_expiring() : (done ? colour_first() : colour_plain())
        }
      );
      }
    }

  if(open.size() > shown)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", open.size() - shown), .color = colour_plain()}
    );

  return lines;
  }

[[nodiscard]]
auto describe_system(star_system_t const & system, bool with_controlling) -> std::vector<overlay::line_t>
  {
  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = system.name, .color = colour_heading()});

  // with influence at hand the controlling faction is marked there with a star, so repeating it serves nothing
  if(with_controlling and not system.controlling_faction.empty())
    lines.push_back(overlay::line_t{.text = system.controlling_faction, .color = colour_plain()});

  if(not system.economy.empty() or not system.government.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{} / {}", system.economy.empty() ? "?" : system.economy, system.government.empty() ? "?" : system.government
        ),
        .color = colour_plain()
      }
    );

  if(not system.security.empty())
    lines.push_back(overlay::line_t{.text = std::format("security: {}", system.security), .color = colour_plain()});

  // a system with the FSS unfinished is a reason to stay, not to fly on
  if(not system.fss_complete and not system.bodies.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format("FSS incomplete, {} bodies known", system.bodies.size()), .color = colour_alert()
      }
    );

  return lines;
  }

///\brief how far back the influence chart reaches
///\detail ten days left the days far apart in the band's width; twenty fit the same rectangle and
/// carry more of the story - a faction's climb usually takes longer than a week to show as a climb
auto chart_window() -> std::chrono::days { return std::chrono::days{eht::settings()->overlay.influence_chart.days}; }
///\brief the height of the plot itself, without the caption and the legend
auto chart_height() -> uint32_t { return eht::settings()->overlay.influence_chart.height; }
///\brief the shapes handed out in the order of the legend
///\detail three independent factions share one colour, so without distinct shapes their lines
/// could not be told apart
constexpr std::array chart_markers{
  overlay::marker_e::circle,
  overlay::marker_e::diamond,
  overlay::marker_e::triangle,
  overlay::marker_e::square,
  overlay::marker_e::cross
};

///\brief a faction as the chart needs it - who it is and how it is drawn, nothing more
struct charted_t
  {
  int64_t oid{-1};
  std::string name;
  uint32_t color{};
  overlay::marker_e marker{};
  };

///\brief the ten day influence chart, with the scale already applied
///
/// The layer receives nothing but numbers in 0..1 - the logarithm, the decades and the window all
/// happen here. A logarithmic scale is what makes the chart useful at all: a faction sitting at 2%
/// and one at 45% move by comparable fractions of themselves, and on a linear axis the small one

/// lies flat against the bottom.
[[nodiscard]]
auto build_influence_chart(
  std::span<info::faction_influence_t const> history, std::span<charted_t const> shown, std::chrono::sys_seconds now
) -> std::vector<overlay::chart_t>
  {
  using std::chrono::sys_seconds;

  sys_seconds const from{now - chart_window()};
  double const span{static_cast<double>((now - from).count())};
  if(span <= 0.0)
    return {};

  struct sample_t
    {
    double x{};
    double percent{};
    };

  std::vector<std::vector<sample_t>> gathered(shown.size());
  double lowest{std::numeric_limits<double>::max()};
  double highest{};

  for(size_t ix{}; ix != shown.size(); ++ix)
    {
    // influence is stored only when it changes, so a faction quiet for ten days has no row inside
    // the window at all - the last value before it is what the line has to start from
    std::optional<double> seed;
    std::vector<sample_t> & points{gathered[ix]};

    for(info::faction_influence_t const & entry: history)
      {
      if(entry.faction_oid != shown[ix].oid)
        continue;

      double const percent{entry.influence * 100.0};
      if(entry.timestamp <= from)
        seed = percent;
      else if(entry.timestamp <= now)
        points.push_back(
          sample_t{.x = static_cast<double>((entry.timestamp - from).count()) / span, .percent = percent}
        );
      }

    if(seed)
      points.insert(points.begin(), sample_t{.x = 0.0, .percent = *seed});

    // the value holds until the next tick, so the line runs on to the right edge instead of
    // stopping wherever the last change happened to fall
    if(not points.empty() and points.back().x < 1.0)
      points.push_back(sample_t{.x = 1.0, .percent = points.back().percent});

    for(sample_t const & point: points)
      if(point.percent > 0.0)
        {
        lowest = std::min(lowest, point.percent);
        highest = std::max(highest, point.percent);
        }
    }

  if(highest <= 0.0 or std::ranges::all_of(gathered, [](auto const & p) { return p.size() < 2u; }))
    return {};

  // Whole decades give a readable grid, and the floor is one per cent because that is the floor in
  // the game: a faction present in a system holds at least a point of it, and below that it is not
  // weakened but gone - it retreats and stops being present at all. A decade under that would be an
  // empty quarter of the chart squeezing the part where the fight actually happens
  double const low{std::max(1.0, std::pow(10.0, std::floor(std::log10(lowest))))};
  double const high{std::max(low * 10.0, std::pow(10.0, std::ceil(std::log10(highest))))};
  double const log_low{std::log10(low)};
  double const log_span{std::log10(high) - log_low};

  auto const map_y = [&](double percent) -> float
  { return static_cast<float>((std::log10(std::clamp(percent, low, high)) - log_low) / log_span); };

  overlay::chart_t chart{
    .caption = std::format("influence, {} days, log scale", chart_window().count()), .height = chart_height()
  };

  for(double decade{low}; decade <= high * 1.0001; decade *= 10.0)
    chart.grid.push_back(
      overlay::grid_line_t{
        .y = map_y(decade), .label = decade >= 1.0 ? std::format("{:.0f}%", decade) : std::format("{:.1f}%", decade)
      }
    );

  for(size_t ix{}; ix != shown.size(); ++ix)
    {
    if(gathered[ix].size() < 2u)
      continue;

    overlay::series_t series{
      .name = shown[ix].name, .color = shown[ix].color,
      .marker = shown[ix].marker
    };
    series.points.reserve(gathered[ix].size());
    for(sample_t const & point: gathered[ix])
      series.points.push_back(overlay::point_t{.x = static_cast<float>(point.x), .y = map_y(point.percent)});

    chart.series.push_back(std::move(series));
    }

  if(chart.series.empty())
    return {};

  return {std::move(chart)};
  }

///\brief how far back the tick chart reaches
auto tick_chart_window() -> std::chrono::days { return std::chrono::days{eht::settings()->overlay.tick_chart.days}; }
auto tick_chart_height() -> uint32_t { return eht::settings()->overlay.tick_chart.height; }
auto colour_tick_influence() -> uint32_t { return eht::settings()->overlay.colours.tick_influence.rgb; }
auto colour_tick_war() -> uint32_t { return eht::settings()->overlay.colours.tick_war.rgb; }

///\brief when the recalculation came, day by day, as the hour of the day it came at
///
/// A tick has no fixed hour - Frontier moves it every few days - so the useful picture is the drift:
/// the days along, the hour of the day up. Each wave is a vertical stroke from the first reading that
/// saw it to the last that had not, so a stroke's length is how little is known about that day, not
/// how long the tick took. Influence and war are separate clocks and each is drawn at the end that
/// matters for it: the start of an influence wave is the deadline for handing missions in, the end of
/// a war wave is when the bonds start selling the new way.
[[nodiscard]]
auto build_tick_chart(
  std::span<info::tick_fact_t const> influence, std::span<info::tick_fact_t const> war, std::chrono::sys_seconds now
) -> std::optional<overlay::chart_t>
  {
  using std::chrono::sys_seconds;

  sys_seconds const from{now - tick_chart_window()};
  double const span{static_cast<double>((now - from).count())};
  constexpr double day{86400.0};
  // a window read to the minute would be a dot too short to see, so every stroke gets at least this much
  constexpr float shortest{0.035f};

  overlay::chart_t chart{
    .caption = std::format("ticks, {} days, UTC hour, red war", tick_chart_window().count()),
    .height = tick_chart_height()
  };
  for(int hour{}; hour <= 24; hour += 6)
    chart.grid.push_back(
      overlay::grid_line_t{
        .y = static_cast<float>(hour) / 24.f, .label = hour == 0 or hour == 24 ? std::string{} : std::format("{:02}h", hour)
      }
    );

  auto const hour_of = [](sys_seconds t) -> float
  {
    auto const since_midnight{t - std::chrono::floor<std::chrono::days>(t)};
    return static_cast<float>(static_cast<double>(since_midnight.count()) / day);
  };

  auto const add_stroke = [&](float x, float y0, float y1, uint32_t colour)
  {
    if(y1 - y0 < shortest)
      {
      float const mid{(y0 + y1) / 2.f};
      y0 = std::max(0.f, mid - shortest / 2.f);
      y1 = std::min(1.f, y0 + shortest);
      }
    chart.series.push_back(
      overlay::series_t{
        .name = {},
        .color = colour,
        .marker = overlay::marker_e::none,
        .points = {overlay::point_t{.x = x, .y = y0}, overlay::point_t{.x = x, .y = y1}}
      }
    );
  };

  auto const add = [&](sys_seconds begin, sys_seconds end, uint32_t colour)
  {
    if(end < from or begin > now)
      return;
    // the stroke stands where the window's middle falls, since that is the best single guess of the day
    sys_seconds const mid{begin + (end - begin) / 2};
    float const x{static_cast<float>(static_cast<double>((mid - from).count()) / span)};
    float const y0{hour_of(begin)};
    float const y1{hour_of(end)};
    // a window across midnight is drawn in two pieces, the evening at the top and the small hours at the bottom
    if(end - begin >= std::chrono::days{1})
      add_stroke(x, 0.f, 1.f, colour);
    else if(y1 >= y0)
      add_stroke(x, y0, y1, colour);
    else
      {
      add_stroke(x, y0, 1.f, colour);
      add_stroke(x, 0.f, y1, colour);
      }
  };

  for(info::tick_fact_t const & wave: influence)
    add(wave.start_begin, wave.start_end, colour_tick_influence());
  for(info::tick_fact_t const & wave: war)
    add(wave.end_begin, wave.end_end, colour_tick_war());

  if(chart.series.empty())
    return std::nullopt;
  return chart;
  }
  }  // namespace

overlay_feed_t::overlay_feed_t(std::string socket_path, std::string db_path) :
    server_{std::make_unique<overlay::server_t>(std::move(socket_path))},
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("overlay feed: failed to open {}", db_path);

  if(server_->listening())
    spdlog::info("overlay feed listening on {}", server_->path());
  else
    // the commonest cause is a path longer than 107 characters - sockaddr_un has nowhere to put it
    spdlog::warn(
      "overlay feed could not listen on {} ({} chars), in-game overlay will stay empty",
      server_->path(),
      server_->path().size()
    );
  }

[[nodiscard]]
auto overlay_feed_t::listening() const noexcept -> bool
  { return server_->listening(); }

[[nodiscard]]
auto overlay_feed_t::clients() const noexcept -> unsigned
  { return server_->clients(); }

auto overlay_feed_t::refresh_factions(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  bool const same_system{state.current_system_address_ == factions_system_};

  // influence moves once a day, so asking the database every frame would be a waste
  if(same_system and now - factions_loaded_ < faction_refresh())
    return;

  factions_system_ = state.current_system_address_;
  factions_loaded_ = now;
  faction_lines_.clear();
  conflict_lines_.clear();
  bool war_running{};
  faction_charts_.clear();

  if(factions_system_ == 0u)
    return;

  // one line fits in the side band, so what is left is the hour alone and whether the wave has reached
  // here - the spread across systems and the statistics are visible in the system window
  auto const wall_clock{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto const tick_line = [&](info::tick_kind_e kind, std::string_view caption) -> overlay::line_t
  {
    tick_view_t const view{describe_tick(db_, factions_system_, kind, wall_clock)};
    return overlay::line_t{
      .text = std::format(
        "{} {}{}",
        caption,
        view.here,
        not view.awaiting ? ""
        : view.seen_since ? "  (wave started, unchanged here since)"
                          : "  (wave started, not seen here since)"
      ),
      // unchanged at a visit after the wave began is no warning: the tick may have come and moved nothing
      .color = view.awaiting and not view.seen_since ? colour_alert() : colour_plain()
    };
  };

  faction_lines_.push_back(tick_line(info::tick_kind_e::influence, "BGS tick"));

  if(auto conflicts{db_.load_conflicts(factions_system_)}; conflicts)
    {
    // the database holds the whole history of rows, and the screen is to show each pair's present state
    std::map<std::pair<std::string, std::string>, info::conflict_t const *> latest_conflict;
    for(info::conflict_t const & conflict: *conflicts)
      {
      auto & slot{latest_conflict[{conflict.faction1, conflict.faction2}]};
      if(slot == nullptr or slot->timestamp < conflict.timestamp)
        slot = &conflict;
      }

    for(auto const & [pair, entry]: latest_conflict)
      {
      info::conflict_t const & conflict{*entry};
      // the finished ones and the merely announced ones change nothing about what to do now
      if(conflict.status != "active")
        continue;
      // a war's score and stakes stand at the head of its own block, with the settlements fought over
      if(conflict.war_type != "election")
        {
        war_running = true;
        continue;
        }

      conflict_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "{}: {} {} - {} {}",
            conflict.war_type,
            conflict.faction1,
            conflict.won_days1,
            conflict.won_days2,
            conflict.faction2
          ),
          .color = colour_alert()
        }
      );

      if(not conflict.stake1.empty() or not conflict.stake2.empty())
        conflict_lines_.push_back(
          overlay::line_t{
            .text = std::format(
              "  stake: {} / {}",
              conflict.stake1.empty() ? "-" : conflict.stake1,
              conflict.stake2.empty() ? "-" : conflict.stake2
            ),
            .color = colour_plain()
          }
        );
      }
    }

  // the war clock is shown only while something is running - it is what bonds are sold by
  if(not conflict_lines_.empty() or war_running)
    {
    // a conflict is settled at the fourth day won, so the end can be counted down without knowing when
    // the recalculation falls - and that is exactly when bonds are worth the most
    if(auto countdown{db_.load_war_countdown(factions_system_)}; countdown)
      for(info::war_countdown_t const & war: *countdown)
        {
        if(not war.active)
          continue;

        bool const decides_now{war.ticks_left == 0u};
        conflict_lines_.insert(
          conflict_lines_.begin(),
          overlay::line_t{
            .text = decides_now ? std::format("{}: decided at the next tick - have bonds ready", war.war_type)
                                : std::format("{}: {} more war ticks", war.war_type, war.ticks_left),
            .color = decides_now ? colour_alert() : colour_plain()
          }
        );
        }

    conflict_lines_.insert(conflict_lines_.begin(), tick_line(info::tick_kind_e::war, "war tick"));
    }

  auto history{db_.load_influence_history(factions_system_)};
  if(not history)
    {
    spdlog::error("overlay feed: failed to load influence for {}", factions_system_);
    return;
    }

  // a faction thrown out of the system stops appearing in the readings, but its last influence row stays
  // behind - which is why the list is narrowed to the ones seen at the newest reading
  std::set<int64_t> present;
  if(auto refs{db_.load_present_factions(factions_system_)}; refs)
    for(info::faction_ref_t const & ref: *refs)
      present.insert(ref.faction_oid);

  // each faction's last row is its present state in the system
  std::map<int64_t, info::faction_influence_t const *> latest;
  for(info::faction_influence_t const & entry: *history)
    {
    // an empty set means we have no trace of presence for this system yet - then everything is shown
    if(not present.empty() and not present.contains(entry.faction_oid))
      continue;
    latest[entry.faction_oid] = &entry;
    }

  struct presence_t
    {
    int64_t oid;
    overlay::trend_e trend{overlay::trend_e::unknown};
    std::string name;
    info::allegiance_e allegiance;
    std::string active;
    double influence;
    };

  std::vector<presence_t> presence;
  presence.reserve(latest.size());
  for(auto const & [oid, entry]: latest)
    {
    presence_t item{
      .oid = oid,
      .name = {},
      .allegiance = info::allegiance_e::unknown,
      .active = not entry->active_states.empty() ? entry->active_states
                : entry->faction_state == "None" ? std::string{}
                                                 : entry->faction_state,
      .influence = entry->influence
    };

    if(
      auto it{std::ranges::find(state.known_factions, oid, &info::faction_info_t::oid)};
      it != state.known_factions.end()
    )
      {
      item.name = it->name;
      item.allegiance = it->allegiance;
      }

    if(not item.name.empty())
      presence.emplace_back(std::move(item));
    }

  // an uninhabited system has no background simulation at all, so the tick hours would only take room
  if(presence.empty())
    {
    faction_lines_.clear();
    conflict_lines_.clear();
    return;
    }

  // Which way each faction went at the last recalculation, rather than at its last change - those
  // are different questions. A faction that did not move writes no row, so silence after a wave we
  // have actually seen means it held its ground; silence because we have not been here since the
  // wave means we simply do not know, and the two must not look the same.
  if(auto waves{db_.load_recent_ticks(info::tick_kind_e::influence, 30u)}; waves and not waves->empty())
    {
    std::chrono::sys_seconds const wave{waves->front().start_begin};
    bool const seen_since{
      std::ranges::any_of(*history, [wave](info::faction_influence_t const & e) { return e.timestamp >= wave; })
    };

    if(seen_since)
      for(presence_t & item: presence)
        {
        std::optional<double> before;
        std::optional<double> after;
        for(info::faction_influence_t const & entry: *history)
          if(entry.faction_oid == item.oid)
            {
            if(entry.timestamp < wave)
              before = entry.influence;
            else
              after = entry.influence;
            }

        if(not before)
          continue;
        if(not after)
          {
          item.trend = overlay::trend_e::flat;
          continue;
          }

        item.trend = *after > *before  ? overlay::trend_e::up
                     : *after < *before ? overlay::trend_e::down
                                        : overlay::trend_e::flat;
        }
    }

  std::ranges::sort(presence, std::ranges::greater{}, &presence_t::influence);

  // the line and the chart series are given the same colour and the same shape in one place, so
  // there is no way for them to drift apart
  std::vector<charted_t> charted;
  std::map<uint32_t, size_t> shades_used;
  size_t marker_ix{};
  for(presence_t const & item: presence | std::views::take(listed_factions()))
    {
    uint32_t const base{allegiance_colour(item.allegiance)};
    uint32_t const colour{shade(base, shades_used[base]++)};
    overlay::marker_e const marker{chart_markers[marker_ix % chart_markers.size()]};
    ++marker_ix;

    charted.push_back(charted_t{.oid = item.oid, .name = item.name, .color = colour, .marker = marker});

    faction_lines_.push_back(
      overlay::line_t{
        // the star marks the controlling faction, because that one decides the system's face
        .text = std::format(
          "{}{}  {:.1f}%",
          item.name == state.system.controlling_faction ? "* " : "  ",
          item.name,
          item.influence * 100.0
        ),
        .color = colour,
        // the same shape the faction's line wears on the chart below, so the two read as one
        .marker = marker,
        .emblem = allegiance_emblem(item.allegiance),
        // the mark goes between the value and the states, because it speaks about the value
        .trend = item.trend,
        .suffix = item.active
      }
    );
    }

  faction_charts_ = build_influence_chart(*history, charted, wall_clock);

  // the ticks are the same for every system, but they belong under the chart whose steps they explain
  auto influence_waves{db_.load_recent_ticks(info::tick_kind_e::influence, tick_chart_window().count())};
  auto war_waves{db_.load_recent_ticks(info::tick_kind_e::war, tick_chart_window().count())};
  if(
    auto chart{build_tick_chart(
      influence_waves ? std::span<info::tick_fact_t const>{*influence_waves} : std::span<info::tick_fact_t const>{},
      war_waves ? std::span<info::tick_fact_t const>{*war_waves} : std::span<info::tick_fact_t const>{},
      wall_clock
    )};
    chart
  )
    faction_charts_.push_back(std::move(*chart));
  }

auto overlay_feed_t::refresh_market(
  uint64_t market_id, uint32_t cargo_capacity, uint64_t destination, std::string_view destination_name
) -> void
  {
  // the place alone will not do as a key: we are at a settlement from the moment we enter, but the goods
  // are known only once the player opens the market, so asking once on a change of place always found nothing
  auto const now{std::chrono::steady_clock::now()};
  // choosing a destination changes what is worth buying here, so it is part of the key - the answer comes at once
  if(market_id == market_id_ and destination == market_destination_ and now - market_loaded_ < market_refresh())
    return;

  market_id_ = market_id;
  market_destination_ = destination;
  market_loaded_ = now;
  market_lines_.clear();
  station_name_.clear();
  station_faction_.clear();
  station_type_.clear();

  if(market_id == 0u)
    return;

  auto station{db_.load_station(market_id)};
  std::string const name{station and *station ? (*station)->name : std::format("market {}", market_id)};
  // the owner of the place decides whose influence grows from missions handed in here - without it a
  // port's name on its own says little when planning work for a faction
  std::string const owner{station and *station ? (*station)->controlling_faction : std::string{}};

  // kept for the settlement block, which asks the same reading a different question
  if(station and *station)
    {
    station_name_ = (*station)->name;
    station_faction_ = owner;
    station_type_ = (*station)->station_type;
    }

  auto entries{db_.load_market_entries(market_id)};
  if(not entries or entries->empty())
    {
    // the Market event appears only once the commodities screen is opened - silence here would look like
    // there being no opportunity, when it means nothing but that we had nothing to record
    market_lines_.push_back(overlay::line_t{.text = std::format("{}: no market data", name), .color = colour_alert()});
    if(not owner.empty())
      market_lines_.push_back(overlay::line_t{.text = std::format("  {}", owner), .color = colour_first()});
    market_lines_.push_back(overlay::line_t{.text = "  open the commodity market to record it", .color = colour_plain()});
    return;
    }

  // the departure from the galactic average is the only number saying whether a price is a bargain
  auto const sell_gain{
    [](info::market_entry_t const & entry) -> double
    {
      if(entry.mean_price == 0u or entry.demand == 0u)
        return 0.0;
      return (double(entry.sell_price) - double(entry.mean_price)) / double(entry.mean_price);
    }
  };
  auto const buy_gain{
    [](info::market_entry_t const & entry) -> double
    {
      if(entry.mean_price == 0u or entry.stock == 0u or entry.buy_price == 0u)
        return 0.0;
      return (double(entry.mean_price) - double(entry.buy_price)) / double(entry.mean_price);
    }
  };

  // a market read before the flags were added has zeros everywhere - then the flags cannot be trusted
  bool const flags_known{
    std::ranges::any_of(*entries, [](info::market_entry_t const & entry) { return entry.producer or entry.consumer; })
  };

  std::vector<info::market_entry_t const *> sells;
  std::vector<info::market_entry_t const *> buys;
  for(info::market_entry_t const & entry: *entries)
    {
    // the game gives a price for commodities the station does not trade in as well - without these flags
    // Platinum at Scott View looked like an opportunity, though it was neither sold nor bought there.
    // markets stored before the flags were added have them at zero, so for those we fall back on stock and demand
    bool const buys_it{flags_known ? entry.consumer : entry.demand > 0u};
    bool const sells_it{flags_known ? entry.producer : entry.stock > 0u};

    if(
      buys_it and sell_gain(entry) >= interesting_deviation() and entry.sell_price > entry.mean_price + interesting_margin()
    )
      sells.push_back(&entry);
    if(
      sells_it and buy_gain(entry) >= interesting_deviation() and entry.buy_price + interesting_margin() < entry.mean_price
    )
      buys.push_back(&entry);
    }

  // the hold has a fixed capacity, so what decides the choice is the amount per tonne, not the percentage
  std::ranges::sort(
    sells, std::ranges::greater{}, [](info::market_entry_t const * e) { return e->sell_price - e->mean_price; }
  );
  std::ranges::sort(
    buys, std::ranges::greater{}, [](info::market_entry_t const * e) { return e->mean_price - e->buy_price; }
  );

  size_t on_sale{};
  size_t wanted{};
  for(info::market_entry_t const & entry: *entries)
    {
    on_sale += entry.stock > 0u ? 1u : 0u;
    wanted += entry.demand > 0u ? 1u : 0u;
    }

  // a port is either a supplier or a buyer - these two numbers say which at first glance
  market_lines_.push_back(
    overlay::line_t{.text = std::format("{}: {} on sale, {} wanted", name, on_sale, wanted), .color = colour_heading()}
  );
  if(not owner.empty())
    market_lines_.push_back(overlay::line_t{.text = std::format("  {}", owner), .color = colour_first()});

  // a trader will not bring in a raw material nobody sells - a station may pay splendidly for it and it
  // stays a dead end all the same, so it goes at the end and under a heading of its own
  auto const mined{[](info::market_entry_t const * entry) { return info::is_mining_only(entry->name); }};
  auto const tradeable{std::ranges::partition(sells, std::not_fn(mined))};
  std::vector<info::market_entry_t const *> const dug_up(tradeable.begin(), tradeable.end());
  sells.erase(tradeable.begin(), tradeable.end());

  if(not sells.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "pays above average:", .color = colour_plain()});
    for(info::market_entry_t const * entry: sells | std::views::take(listed_commodities()))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} over avg",
            entry->name,
            format_credits_value(entry->sell_price),
            format_credits_value(entry->sell_price - entry->mean_price)
          ),
          .color = colour_first()
        }
      );
    }

  if(not dug_up.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "pays well, but mining only:", .color = colour_plain()});
    for(info::market_entry_t const * entry: dug_up | std::views::take(2u))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} over avg",
            entry->name,
            format_credits_value(entry->sell_price),
            format_credits_value(entry->sell_price - entry->mean_price)
          ),
          .color = colour_plain()
        }
      );
    }

  if(not buys.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "sells below average:", .color = colour_plain()});
    for(info::market_entry_t const * entry: buys | std::views::take(listed_commodities()))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} under avg",
            entry->name,
            format_credits_value(entry->buy_price),
            format_credits_value(entry->mean_price - entry->buy_price)
          ),
          .color = colour_plain()
        }
      );
    }

  // the galactic average says whether a price is good, but the earnings come from the difference between markets
  auto const add_trades{
    [this, cargo_capacity](bool bring_here, std::string const & heading, uint64_t in_system) -> bool
    {
      auto trades{db_.load_trade_options(market_id_, listed_trades(), bring_here, in_system)};
      if(not trades or trades->empty())
        return false;

      market_lines_.push_back(overlay::line_t{.text = heading, .color = colour_plain()});

      for(info::trade_option_t const & trade: *trades)
        {
        auto const margin{trade.sell_price - trade.buy_price};
        // one run is as many tonnes as the hold takes, provided the goods and the demand suffice
        auto const tonnes{std::min({cargo_capacity != 0u ? cargo_capacity : trade.stock, trade.stock, trade.demand})};
        auto const run{uint64_t{margin} * tonnes};

        market_lines_.push_back(
          overlay::line_t{
            .text = std::format("  {}  {} Cr/t", trade.commodity, format_credits_value(margin)), .color = colour_first()
          }
        );
        market_lines_.push_back(
          overlay::line_t{
            .text = std::format(
              "    {} t = {} Cr  {} {}{}{}",
              format_credits_value(tonnes),
              format_credits_value(static_cast<uint32_t>(std::min<uint64_t>(run, UINT32_MAX))),
              bring_here ? "from" : "to",
              trade.station,
              // under a heading that already names the destination the system would only repeat it
              trade.system.empty() or in_system != 0u ? "" : ", ",
              in_system != 0u ? std::string_view{} : std::string_view{trade.system}
            ),
            .color = colour_plain()
          }
        );
        }
      return true;
    }
  };

  // with a route plotted the question is no longer what pays anywhere, but what pays where we are going -
  // and what is there that this place wants, for the way back. The game names only the system, not the
  // station picked in it, so every market known there takes part
  if(destination != 0u)
    {
    bool const any_there{add_trades(false, std::format("take to {}:", destination_name), destination)};
    bool const any_back{add_trades(true, std::format("bring back from {}:", destination_name), destination)};
    if(any_there or any_back)
      return;
    // silence would read as nothing being worth it, when mostly it means none of its markets were ever opened
    market_lines_.push_back(
      overlay::line_t{.text = std::format("{}: no known market trades with here", destination_name), .color = colour_alert()}
    );
    }

  add_trades(true, "bring here, best known:", 0u);
  add_trades(false, "take from here, best known:", 0u);
  }

auto overlay_feed_t::refresh_supply() -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(now - supply_loaded_ < supply_refresh())
    return;

  supply_loaded_ = now;
  needs_.clear();
  options_.clear();
  producers_.clear();

  if(auto needs{db_.load_cargo_needs()}; needs)
    needs_ = std::move(*needs);

  if(needs_.empty())
    return;

  if(auto options{db_.load_supply_options()}; options)
    options_ = std::move(*options);

  if(auto producers{db_.load_producers()}; producers)
    producers_ = std::move(*producers);
  }

///\brief what the open missions can be advanced with without flying anywhere
///
/// Two kinds of work meet at a settlement and the game words them differently. One names the place:
/// a theft, a download, an assassination at a settlement given by name. The other names a faction
/// and no place at all - exterminating that faction's members counts at any settlement it holds,
/// this one included, and nothing in the mission says so. Standing on the pad both are the same
///\brief who holds the places the open missions point at
///
/// A mission names its destination with two strings and no address, and never names the faction that
/// holds it - yet that faction is the one the work counts against. Resolved only when the set of
/// open missions changes, because it never changes between two jumps and the answer costs a join.
auto overlay_feed_t::refresh_mission_places(current_state_t const & state) -> void
  {
  uint64_t signature{1469598103934665603ull};
  for(info::mission_t const & mission: state.active_missions)
    {
    signature ^= mission.mission_id + static_cast<uint64_t>(mission.status);
    signature *= 1099511628211ull;
    }

  if(signature == missions_signature_)
    return;

  missions_signature_ = signature;
  place_owner_.clear();

  auto const resolve = [this](std::string const & system, std::string const & place)
  {
    if(system.empty() or place.empty() or place_owner_.contains(std::pair{system, place}))
      return;

    if(auto owner{db_.load_place_owner(system, place)}; owner and *owner)
      place_owner_.emplace(std::pair{system, place}, **owner);
  };

  for(info::mission_t const & mission: state.active_missions)
    {
    if(
      mission.status != info::mission_status_e::accepted and mission.status != info::mission_status_e::redirected
    )
      continue;

    resolve(mission.destination_system, std::string{mission.destination_place()});
    resolve(
      mission.redirected_system,
      mission.redirected_settlement.empty() ? mission.redirected_station : mission.redirected_settlement
    );
    }
  }

///\brief the ship under the crosshairs, read off without looking away from it
///
/// The scan uncovers it in stages and each line appears as the game gives it: the hull first, then
/// who flies it, then how hurt they are, and only at the end the faction and the price on their
///\brief which interface the game has open, which only Status.json says
///
/// The overlay could look at the screen it draws on and work out what is there, but it sits on the
/// game's frame path and reading pixels back off the card means waiting for the card. It does not
/// have to: the game writes what it is showing into Status.json beside the journals, and rewrites it
/// whenever it changes.
auto overlay_feed_t::refresh_construction(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(construction_changes_ != state.construction_changes_ or now - construction_read_ > std::chrono::seconds{30})
    {
    construction_changes_ = state.construction_changes_;
    construction_read_ = now;
    auto loaded{db_.load_construction_sites()};
    construction_sites_ = loaded ? std::move(*loaded) : std::vector<info::construction_site_t>{};
    if(auto categories{db_.load_commodity_categories()}; categories)
      commodity_categories_ = std::move(*categories);
    }
  if(carrier_cargo_changes_ != state.carrier_changes_ or construction_read_ == now)
    {
    carrier_cargo_changes_ = state.carrier_changes_;
    auto totals{db_.load_carrier_cargo_totals()};
    carrier_cargo_ = totals ? std::move(*totals) : std::map<std::string, int64_t>{};
    cargo_by_carrier_.clear();
    carrier_callsigns_.clear();
    if(auto carriers{db_.load_carriers()}; carriers)
      for(info::carrier_t const & carrier: *carriers)
        if(carrier.tracked or carrier.carrier_type == "SquadronCarrier")
          {
          carrier_callsigns_[carrier.market_id] = carrier.carrier_id;
          if(auto cargo{db_.load_carrier_cargo(carrier.market_id)}; cargo)
            for(info::carrier_cargo_t const & item: *cargo)
              cargo_by_carrier_[carrier.market_id][item.key] += item.count;
          }
    }
  // the port's market is read again when the port changes, or with the sites
  uint64_t const port{state.settlement_market_id_};
  if(port != construction_port_ or construction_read_ == now)
    {
    construction_port_ = port;
    construction_port_market_.clear();
    if(port != 0u)
      if(auto entries{db_.load_market_entries(port)}; entries)
        construction_port_market_ = std::move(*entries);
    }
  }

namespace
  {
///\brief the colours of the economies producing a commodity, as colonisation planners draw them
[[nodiscard]]
auto economy_colours(commodity_facts::economy_e produced_by) -> std::vector<uint32_t>
  {
  using commodity_facts::economy_e;
  constexpr std::array<std::pair<economy_e, uint32_t>, 6> palette{{
    {economy_e::agriculture, 0x7acc00u},
    {economy_e::high_tech, 0x00ccccu},
    {economy_e::industrial, 0x999900u},
    {economy_e::military, 0xbb00bbu},
    {economy_e::refinery, 0xcc6600u},
    {economy_e::extraction, 0xcc3333u},
  }};
  std::vector<uint32_t> colours;
  for(auto const & [economy, colour]: palette)
    if((uint8_t(produced_by) & uint8_t(economy)) != 0u)
      colours.push_back(colour);
  return colours;
  }
  }  // namespace

auto overlay_feed_t::build_construction_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  if(construction_sites_.empty())
    return {};

  // which site: the one chosen in the window, the one docked at, or the destination in this system -
  // a destination elsewhere is only a system to the game, so it cannot name a site
  auto const by_market = [&](uint64_t market) -> info::construction_site_t const *
  {
    auto const it{std::ranges::find(construction_sites_, market, [](auto const & s) { return s.depot.market_id; })};
    return it == construction_sites_.end() ? nullptr : &*it;
  };
  info::construction_site_t const * site{by_market(construction_focus_)};
  if(site == nullptr and state.settlement_market_id_ != 0u)
    site = by_market(state.settlement_market_id_);
  if(site == nullptr and status_destination_ and status_destination_->System == state.current_system_address_)
    for(info::construction_site_t const & s: construction_sites_)
      if(s.depot.system_address == state.current_system_address_ and not s.name.empty()
         and s.name == status_destination_->Name)
        site = &s;
  if(site == nullptr)
    return {};

  std::map<std::string, uint32_t> hold;
  for(events::cargo_item_t const & item: state.cargo.Inventory)
    hold[info::commodity_key(item.Name)] += item.Count;
  std::map<std::string, info::market_entry_t const *> here;
  if(construction_port_ != site->depot.market_id)
    for(info::market_entry_t const & entry: construction_port_market_)
      if(entry.stock > 0u and entry.buy_price > 0u)
        here[entry.key.empty() ? info::commodity_key(entry.name) : entry.key] = &entry;

  std::vector<overlay::line_t> lines;
  uint64_t left_total{};
  size_t wanted{};
  for(info::construction_need_t const & need: site->needs)
    if(need.required > need.provided)
      {
      left_total += need.required - need.provided;
      ++wanted;
      }
  // two short lines - one long one would widen the whole block into the middle of the screen
  lines.push_back(
    overlay::line_t{.text = std::format("construction: {}", info::shown_name(*site)), .color = colour_heading()}
  );
  lines.push_back(
    overlay::line_t{
      .text = std::format("{:.0f}%  {} t left  {}", site->depot.progress * 100.0, left_total, site->system),
      .color = colour_plain()
    }
  );

  size_t const limit{rows_for(
    wanted,
    eht::settings()->overlay.lists.construction == 0u ? site->needs.size() : eht::settings()->overlay.lists.construction
  )};
  // by type, then by name - the way the game's own list reads, each type under a line of its own
  auto const category_of = [&](info::construction_need_t const & need) -> std::string
  {
    if(auto const found{commodity_categories_.find(need.key)}; found != commodity_categories_.end())
      return found->second;
    if(auto const known{commodity_facts::category_of(need.key)}; known)
      return std::string{*known};
    return "Other";
  };
  std::vector<info::construction_need_t const *> needed;
  for(info::construction_need_t const & need: site->needs)
    if(need.required > need.provided)
      needed.push_back(&need);
  std::ranges::sort(
    needed,
    [&](info::construction_need_t const * a, info::construction_need_t const * b)
    { return info::needs_before(category_of(*a), a->commodity, category_of(*b), b->commodity); }
  );

  // the carrier the site is supplied from, when one was chosen in the Construction window - its callsign
  // heads its column; without one the column is all our carriers together
  uint64_t const supplier{construction_supplier(site->depot.market_id)};
  static std::map<std::string, int64_t> const nothing;
  auto const callsign{carrier_callsigns_.find(supplier)};
  bool const one_carrier{supplier != 0u and callsign != carrier_callsigns_.end()};
  auto const supplier_cargo{cargo_by_carrier_.find(supplier)};
  std::map<std::string, int64_t> const & on_carrier{
    one_carrier ? (supplier_cargo != cargo_by_carrier_.end() ? supplier_cargo->second : nothing) : carrier_cargo_
  };
  std::string const carrier_heading{one_carrier ? callsign->second : std::string{"carriers"}};
  // what is on its way counts towards the carrier as well; docked at that carrier its cargo is booked only
  // on leaving, so the hold as it was on docking stands in for the hold now, or it would count twice
  std::map<std::string, int64_t> on_board;
  if(state.carrier_visit_ and (one_carrier ? state.carrier_visit_->carrier_id == supplier : true))
    for(auto const & [key, item]: state.carrier_visit_->hold)
      on_board[key] += item.second;
  else
    for(auto const & [key, count]: hold)
      on_board[key] += count;

  // the amounts stand in columns after the longest name, under a heading of their own
  size_t name_width{};
  for(info::construction_need_t const * need: needed)
    name_width = std::max(name_width, need->commodity.size());
  size_t const carrier_width{std::max<size_t>(7u, carrier_heading.size() + 3u)};
  {
  std::string heading(name_width, ' ');
  heading += std::format("{:>7}{:>7}{:>7}{:>{}}   here", "left", "diff", "hold", carrier_heading, carrier_width);
  lines.push_back(overlay::line_t{.text = std::move(heading), .color = colour_heading(), .swatch_space = true});
  }

  size_t shown{};
  int64_t to_bring{};
  std::string last_category;
  for(info::construction_need_t const * need: needed)
    {
    if(shown == limit)
      break;
    ++shown;
    if(std::string const category{category_of(*need)}; category != last_category)
      {
      last_category = category;
      lines.push_back(overlay::line_t{.text = category, .color = colour_heading()});
      }
    uint32_t const left{need->required - need->provided};
    std::string text{need->commodity};
    text.resize(std::max<size_t>(text.size(), name_width), ' ');
    text += std::format("{:>7}", left);
    // the carrier and the hold against what is left: below zero is still to be brought, above is to spare
    int64_t diff{-int64_t(left)};
    if(auto const c{on_carrier.find(need->key)}; c != on_carrier.end())
      diff += c->second;
    if(auto const b{on_board.find(need->key)}; b != on_board.end())
      diff += b->second;
    to_bring += std::max<int64_t>(-diff, 0);
    // red what is lacking, green what is there - the same as in the window
    overlay::span_t const diff_span{
      .from = uint32_t(text.size()), .length = 7u, .color = diff < 0 ? 0xff6666u : 0x66dd66u
    };
    text += diff > 0 ? std::format("{:>7}", std::format("+{}", diff)) : std::format("{:>7}", diff);
    auto const h{hold.find(need->key)};
    text += h != hold.end() ? std::format("{:>7}", h->second) : std::string(7u, ' ');
    auto const c{on_carrier.find(need->key)};
    text += c != on_carrier.end() and c->second > 0 ? std::format("{:>{}}", c->second, carrier_width)
                                                     : std::string(carrier_width, ' ');
    auto const m{here.find(need->key)};
    if(m != here.end())
      text += std::format("   {} @ {}", m->second->stock, m->second->buy_price);
    // blanks left at the end by empty columns would only widen the block
    while(not text.empty() and text.back() == ' ')
      text.pop_back();
    // what can be loaded right here stands out; the square says which economies produce it - grey when
    // the table of facts does not know, so that the names still stand in a column
    overlay::line_t line{.text = std::move(text), .color = m != here.end() ? colour_first() : colour_plain()};
    line.spans.push_back(diff_span);
    if(auto const fact{commodity_facts::find(need->key)}; fact)
      {
      line.swatch = economy_colours(fact->produced_by);
      line.swatch_dot = fact->surface;
      }
    else
      line.swatch = {0x707070u};
    lines.push_back(std::move(line));
    }
  if(wanted > shown)
    lines.push_back(overlay::line_t{.text = std::format("  ... and {} more", wanted - shown), .color = colour_plain()});
  if(to_bring > 0)
    lines.push_back(
      overlay::line_t{.text = std::format("still to bring: {} t", to_bring), .color = colour_heading(), .swatch_space = true}
    );
  return lines;
  }

auto overlay_feed_t::refresh_wars(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  uint64_t const here{state.current_system_address_};
  // a kill writes its bond at once, and the settlements change slowly - half a minute, or a new system
  if(here != wars_system_ or bond_changes_ != state.bond_changes_ or now - wars_read_ > std::chrono::seconds{30})
    {
    wars_system_ = here;
    wars_read_ = now;
    auto loaded{db_.load_war_views(here)};
    wars_ = loaded ? std::move(*loaded) : std::vector<info::war_view_t>{};
    }
  if(wars_.empty())
    return;
  // the bonds are read back from the journals - on a kill or a hand-in, and now and then
  if(bond_changes_ != state.bond_changes_ or now - bonds_read_ > std::chrono::minutes{5})
    {
    bond_changes_ = state.bond_changes_;
    bonds_read_ = now;
    unsold_bonds_ = unsold_bonds(state.journal_dir_path_, state.owner_fid_);
    }
  }

namespace
  {
///\brief a faction's name cut to fit a column - the long ones run past thirty characters
[[nodiscard]]
auto cut_name(std::string_view name, size_t most) -> std::string
  {
  if(name.size() <= most)
    return std::string{name};
  size_t end{most - 3u};
  while(end != 0u and (static_cast<unsigned char>(name[end]) & 0xc0u) == 0x80u)
    --end;
  return std::string{name.substr(0u, end)} + "...";
  }

///\brief what is known of a settlement's zone: confirmed in this war, a lower bound from an earlier one,
/// or nothing - the intensity never falls from one war to the next
[[nodiscard]]
auto intensity_text(info::war_settlement_t const & s) -> std::string
  {
  auto const name = [](info::cz_intensity_e i) -> std::string_view
  {
    switch(i)
      {
      case info::cz_intensity_e::low:    return "Low";
      case info::cz_intensity_e::medium: return "Medium";
      case info::cz_intensity_e::high:   return "High";
      default:                           return "unknown";
      }
  };
  if(s.now != info::cz_intensity_e::unknown)
    return std::string{name(s.now)};
  if(s.before != info::cz_intensity_e::unknown)
    return std::string{name(s.before)} + "?";
  return "unknown";
  }
  }  // namespace

auto overlay_feed_t::build_war_blocks() const -> std::vector<std::vector<overlay::line_t>>
  {
  std::vector<std::vector<overlay::line_t>> blocks;
  for(info::war_view_t const & war: wars_)
    {
    info::conflict_t const & c{war.conflict};
    std::vector<overlay::line_t> lines;
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{} {}  {} : {}  {}  ({})",
          c.war_type == "civilwar" ? "civil war" : "war",
          c.faction1,
          c.won_days1,
          c.won_days2,
          c.faction2,
          c.status
        ),
        .color = colour_heading()
      }
    );
    if(not c.stake1.empty() or not c.stake2.empty())
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "stake: {}  /  {}", c.stake1.empty() ? "-" : c.stake1, c.stake2.empty() ? "-" : c.stake2
          ),
          .color = colour_plain()
        }
      );
    // A hand-in pays about 3.3 times what the kills did - the median over 386 hand-ins of two accounts,
    // the same at every mercenary rank; the zones won make the rest of the spread (1.5 - 6.4)
    constexpr double typical_payout{3.3};
    auto const bonds_of = [&](std::string const & faction) -> std::string
    {
      auto const it{unsold_bonds_.find(faction)};
      if(it == unsold_bonds_.end() or it->second == 0u)
        return "-";
      return std::format(
        "{} (~{})",
        overlay_exploration::short_credits(it->second),
        overlay_exploration::short_credits(uint64_t(double(it->second) * typical_payout))
      );
    };
    // Only what the kills themselves paid. A hand-in pays some 2.25 times that plus a sum for every zone
    // won (about 3M for a high one), and the game writes neither down until the hand-in - so the line says
    // what it is rather than pretend to be the payout.
    // Zones reached by dropship are paid by Frontline Solutions, not by the side fought for
    std::string const frontline{bonds_of("$faction_FrontlineSolutions;")};
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "bonds (kills only): {} {}  /  {} {}{}",
          cut_name(c.faction1, 20),
          bonds_of(c.faction1),
          cut_name(c.faction2, 20),
          bonds_of(c.faction2),
          frontline == "-" ? std::string{} : std::format("  /  Frontline {}", frontline)
        ),
        .color = colour_plain()
      }
    );

    // the text is monospaced - the columns are names padded with spaces
    size_t name_width{10u};
    for(info::war_settlement_t const & s: war.settlements)
      name_width = std::max(name_width, std::min<size_t>(s.name.size(), 30u));
    constexpr size_t owner_width{24u};
    for(info::war_settlement_t const & s: war.settlements)
      {
      std::string text{"  " + cut_name(s.name, 30u)};
      text.resize(2u + name_width + 2u, ' ');
      text += cut_name(s.owner_before, owner_width);
      text.resize(2u + name_width + 2u + owner_width + 2u, ' ');
      text += intensity_text(s);
      lines.push_back(overlay::line_t{.text = std::move(text), .color = colour_plain()});
      }
    if(war.settlements.empty())
      lines.push_back(overlay::line_t{.text = "  no settlements of either side known here", .color = colour_plain()});
    blocks.push_back(std::move(lines));
    }
  return blocks;
  }

auto overlay_feed_t::refresh_unsold(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  // a sample analysed, a bounty earned, a sale, a hand-in or a death changes it at once; the cartography
  // growing with every scan is caught within the minute
  if(
    unsold_scans_ == state.organic_scans_seen_ and unsold_bounty_at_ == state.last_bounty_at
    and unsold_changes_ == state.at_risk_changes_ and now - unsold_read_ < std::chrono::seconds{60}
  )
    return;
  unsold_scans_ = state.organic_scans_seen_;
  unsold_changes_ = state.at_risk_changes_;
  unsold_bounty_at_ = state.last_bounty_at;
  unsold_read_ = now;
  at_risk_ = bio::at_risk(state.journal_dir_path_, state.owner_fid_);
  auto const wall{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  legal_ = legal_standing(
    state.journal_dir_path_, state.owner_fid_, wall - std::chrono::days{eht::settings()->overlay.bounty_days}, wall
  );
  }

auto overlay_feed_t::refresh_species_history(current_state_t const & state) -> void
  {
  // our history grows only when we take a sample; another account's whenever they do, which we cannot see
  auto const now{std::chrono::steady_clock::now()};
  bool const sampled{history_scans_ != state.organic_scans_seen_};
  if(not sampled and now - history_read_ < std::chrono::minutes{5})
    return;
  history_scans_ = state.organic_scans_seen_;
  history_read_ = now;

  if(auto history{db_.load_species_history()}; history)
    species_history_ = std::move(*history);
  else
    spdlog::error("failed to read the species found before");
  size_t const own{species_history_.size()};
  for(std::string const & galaxy: eht::settings()->exploration.shared_galaxies)
    if(auto shared{database_storage_t::load_species_history_from(galaxy)}; shared)
      bio::merge_history(species_history_, std::move(*shared));
  if(species_history_.size() != own)
    spdlog::debug("species history: {} own finds, {} from the shared galaxies", own, species_history_.size() - own);

  // the codex is this commander's alone and changes only with their own samples
  if(sampled)
    codex_.write_page(db_);
  }

auto overlay_feed_t::refresh_status(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(now - status_read_ < status_refresh())
    return;

  status_read_ = now;
  if(auto status{load_status(state.journal_dir_path_)}; status)
    {
    gui_focus_ = status->GuiFocus;
    status_flags_ = status->Flags;
    legal_state_ = status->LegalState;
    status_flags2_ = status->Flags2;
    surface_ = overlay_exploration::surface_view_t{
      .body_name = status->BodyName,
      .here = status->Latitude and status->Longitude
                ? std::optional{bio::surface_point_t{*status->Latitude, *status->Longitude}}
                : std::nullopt,
      .planet_radius = status->PlanetRadius.value_or(0.0),
      .sampler_in_hand = status->SelectedWeapon.contains("sampletool")
    };
    status_body_ = std::move(status->BodyName);
    status_destination_ = std::move(status->Destination);
    }
  }

auto overlay_feed_t::build_settlement_owners() const -> std::vector<overlay::line_t>
  {
  // On foot at a place with mission boards - a port's concourse or hangar, or a settlement. Inside a port
  // the game's own flags say so; at a settlement only its market, known from the approach, does. A taxi
  // is not a place to take work from
  constexpr uint64_t on_foot_flag{1u << 0u};
  constexpr uint64_t taxi_flag{1u << 1u};
  constexpr uint64_t in_port_flags{(1u << 3u) | (1u << 13u) | (1u << 14u)};
  if((status_flags2_ & on_foot_flag) == 0u or (status_flags2_ & taxi_flag) != 0u)
    return {};
  if(market_id_ == 0u and (status_flags2_ & in_port_flags) == 0u)
    return {};
  // on foot in a port Status.json names the port itself in place of a body
  std::string const & here{station_name_.empty() ? status_body_ : station_name_};

  // ports in space are not what a job "at a settlement" means, nor are building sites and carriers
  using namespace std::string_view_literals;
  static constexpr std::array not_settlements{
    "Coriolis"sv,
    "Orbis"sv,
    "Ocellus"sv,
    "Outpost"sv,
    "Bernal"sv,
    "Dodec"sv,
    "AsteroidBase"sv,
    "MegaShip"sv,
    "FleetCarrier"sv,
    "SpaceConstructionDepot"sv,
    "PlanetaryConstructionDepot"sv
  };
  // the faction, then its settlements, both in alphabetical order - std::map keeps it so; the economy
  // rides along, since it says what the settlement's missions and its market are about
  std::map<std::string, std::map<std::string, std::string>> by_owner;
  size_t count{};
  for(info::station_t const & station: stations_)
    {
    // An approach records a place with no type, so building sites come in by their names too. A name the
    // game never localised - the colonisation ship, a megaship's operations - is no settlement either
    if(std::ranges::contains(not_settlements, std::string_view{station.station_type})
       or station.name.starts_with('$') or station.name.starts_with("Planetary Construction Site:")
       or station.name.starts_with("Orbital Construction Site:"))
      continue;
    by_owner[station.controlling_faction.empty() ? std::string{"owner unknown"} : station.controlling_faction]
      .emplace(station.name, station.economy);
    ++count;
    }
  if(by_owner.empty())
    return {};

  // The tree read top to bottom - a faction, then its settlements indented, where we stand marked in the
  // indent - flows down a column and on into the next one when it reaches the floor
  struct entry_t
    {
    std::string text;
    ///\brief the faction a settlement belongs to - repeated at the top of a column it spills into
    std::string const * owner;
    bool faction;
    };

  constexpr std::string_view continued{" (cont.)"};
  // a long name is cut, the economy after it being what matters; the cut keeps whole UTF-8 characters
  constexpr std::string_view cut_mark{"..."};
  size_t const name_chars{std::max<size_t>(eht::settings()->overlay.lists.settlement_name_chars, cut_mark.size() + 4u)};
  auto const shortened = [&](std::string const & name) -> std::string
  {
    if(name.size() <= name_chars)
      return name;
    size_t end{name_chars - cut_mark.size()};
    while(end != 0u and (static_cast<unsigned char>(name[end]) & 0xc0u) == 0x80u)
      --end;
    std::string result{name.substr(0u, end)};
    while(not result.empty() and result.back() == ' ')
      result.pop_back();
    return result + std::string{cut_mark};
  };
  // the economies stand in a column of their own, after the longest name
  size_t longest_name{};
  for(auto const & [owner, names]: by_owner)
    for(auto const & [name, economy]: names)
      longest_name = std::max(longest_name, shortened(name).size());

  std::vector<entry_t> entries;
  size_t widest{};
  for(auto const & [owner, names]: by_owner)
    {
    entries.push_back(entry_t{.text = owner, .owner = &owner, .faction = true});
    widest = std::max(widest, owner.size() + continued.size());
    for(auto const & [name, economy]: names)
      {
      std::string text{(name == here ? "> " : "  ") + shortened(name)};
      if(not economy.empty())
        {
        text.resize(2u + longest_name + 2u, ' ');
        text += economy;
        }
      entries.push_back(entry_t{.text = std::move(text), .owner = &owner, .faction = false});
      }
    }
  for(entry_t const & entry: entries)
    widest = std::max(widest, entry.text.size());

  auto const cfg{eht::settings()};
  size_t const rows{std::max<size_t>(cfg->overlay.lists.settlement_rows, 2u)};
  // the text is monospaced, so a column is its widest line and two spaces; three at most
  size_t const pitch{widest + 2u};
  size_t const most_columns{std::clamp<size_t>(cfg->overlay.lists.settlement_line_chars / pitch, 1u, 3u)};

  std::vector<std::vector<std::string>> columns(1u);
  size_t shown{};
  for(entry_t const & entry: entries)
    {
    // a faction never stands alone at the foot of a column, away from its settlements
    bool const full{columns.back().size() >= rows or (entry.faction and columns.back().size() + 1u >= rows)};
    if(full and not columns.back().empty())
      {
      if(columns.size() == most_columns)
        break;
      columns.emplace_back();
      // a faction's list going on in the new column says whose it still is
      if(not entry.faction)
        columns.back().push_back(*entry.owner + std::string{continued});
      }
    columns.back().push_back(entry.text);
    shown += entry.faction ? 0u : 1u;
    }

  std::vector<overlay::line_t> lines;
  // only the places visited or flown close to are known - the game lists no others, and nothing is downloaded
  lines.push_back(
    overlay::line_t{.text = std::format("settlements known here: {}", count), .color = colour_heading()}
  );
  size_t const height{std::ranges::max(columns, {}, &std::vector<std::string>::size).size()};
  for(size_t row{}; row != height; ++row)
    {
    std::string text;
    for(size_t column{}; column != columns.size(); ++column)
      {
      if(row >= columns[column].size())
        continue;
      text.resize(column * pitch, ' ');
      text += columns[column][row];
      }
    lines.push_back(overlay::line_t{.text = std::move(text), .color = colour_plain()});
    }
  if(shown != count)
    lines.push_back(overlay::line_t{.text = std::format("... and {} more", count - shown), .color = colour_plain()});
  return lines;
  }

/// head. Nothing here is remembered - the moment the target is let go there is nothing to show.
auto overlay_feed_t::build_target_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  auto const & target{state.target};

  std::vector<overlay::line_t> lines;

  // A kill stands for a few seconds after it is made, because the money it paid is the answer to the
  // question the scan asked. How long ago it was is counted on our own clock rather than by
  // subtracting the game's stamp from this machine's time: the two need not agree even when both
  // call themselves UTC, and a few minutes of drift would either hide every kill or never let one go
  if(
    state.last_bounty.TotalReward != 0u and state.last_bounty_at != std::chrono::steady_clock::time_point{}
    and std::chrono::steady_clock::now() - state.last_bounty_at < kill_shown()
  )
    {
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "killed {}  +{} Cr",
          state.last_bounty.PilotName_Localised.empty() ? state.last_bounty.VictimFaction
                                                        : state.last_bounty.PilotName_Localised,
          format_credits_value(uint32_t(std::min<uint64_t>(state.last_bounty.TotalReward, 0xffffffffull)))
        ),
        .color = colour_first()
      }
    );

    for(events::bounty_reward_t const & reward: state.last_bounty.Rewards)
      lines.push_back(
        overlay::line_t{
          .text = std::format("  {}  {} Cr", reward.Faction, format_credits_value(uint32_t(reward.Reward))),
          .color = colour_plain()
        }
      );
    }

  // nothing is asked of the clock here: what was replayed from before the tool started was dropped
  // the moment the reader reached the present, so a lock still standing is one that was made since
  if(not target.TargetLocked)
    return lines;

  std::string const pilot{target.PilotName_Localised.empty() ? target.PilotName : target.PilotName_Localised};
  std::string const ship{target.Ship_Localised.empty() ? target.Ship : target.Ship_Localised};

  if(not pilot.empty())
    lines.push_back(
      overlay::line_t{
        .text = target.PilotRank.empty() ? pilot : std::format("{}  {}", pilot, target.PilotRank),
        .color = colour_heading()
      }
    );

  if(not ship.empty())
    lines.push_back(overlay::line_t{.text = ship, .color = colour_plain()});

  if(not target.Faction.empty())
    lines.push_back(overlay::line_t{.text = target.Faction, .color = colour_plain()});

  if(not target.LegalStatus.empty())
    {
    std::string status{target.LegalStatus};
    if(target.Bounty != 0u)
      status += std::format("   {} Cr", format_credits_value(uint32_t(std::min<uint64_t>(target.Bounty, 0xffffffffull))));

    lines.push_back(overlay::line_t{.text = std::move(status), .color = legal_colour(target.LegalStatus)});
    }

  // the health arrives from the second stage on, as whole per cent rather than a fraction
  if(target.ScanStage >= 2)
    lines.push_back(
      overlay::line_t{
        .text = std::format("hull {:.0f}%   shield {:.0f}%", target.HullHealth, target.ShieldHealth),
        .color = target.HullHealth < 25.0 ? colour_expiring() : colour_plain()
      }
    );

  if(not target.Subsystem_Localised.empty() or not target.Subsystem.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}  {:.0f}%",
          target.Subsystem_Localised.empty() ? target.Subsystem : target.Subsystem_Localised,
          target.SubsystemHealth
        ),
        .color = colour_plain()
      }
    );

  return lines;
  }

///\brief the hired pilot and the fighter they fly
auto overlay_feed_t::build_crew_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  using fighter_e = current_state_t::fighter_e;

  if(state.crew_name.empty() and state.fighter == fighter_e::stowed)
    return {};

  std::vector<overlay::line_t> lines;

  if(not state.crew_name.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format("{}  {}", state.crew_name, combat_rank_name(state.crew_combat_rank)),
        .color = colour_heading()
      }
    );

  switch(state.fighter)
    {
    case fighter_e::deployed:
      lines.push_back(
        overlay::line_t{
          .text = state.fighter_crewed ? "fighter out, crew flying" : "fighter out, you are flying",
          .color = colour_first()
        }
      );
      break;

    case fighter_e::destroyed:
      // the hangar builds another one, so this says wait rather than mourn
      lines.push_back(overlay::line_t{.text = "fighter destroyed", .color = colour_expiring()});
      break;

    case fighter_e::stowed: lines.push_back(overlay::line_t{.text = "fighter in the bay", .color = colour_plain()}); break;
    }

  return lines;
  }

/// question, which is why they are answered together and above everything else on this side.
auto overlay_feed_t::build_settlement_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  if(station_name_.empty())
    return {};

  // the faction match holds only where one can walk up to them; in a starport the same mission
  // would be a suggestion to do something the game does not allow
  bool const on_foot{station_type_ == "OnFootSettlement"};

  std::vector<info::mission_t const *> hand_in;
  std::vector<info::mission_t const *> here;
  std::vector<info::mission_t const *> fits;

  for(info::mission_t const & mission: state.active_missions)
    {
    bool const done{mission.status == info::mission_status_e::redirected};
    if(mission.status != info::mission_status_e::accepted and not done)
      continue;

    // A redirected mission keeps the settlement it was done at in its destination, and that place is
    // now finished business - only the handing in is left. Matching it by destination would send the
    // player back to a job already done.
    if(done)
      {
      if(mission.redirected_settlement == station_name_ or mission.redirected_station == station_name_)
        hand_in.push_back(&mission);
      }
    else if(not station_name_.empty() and mission.destination_place() == station_name_)
      here.push_back(&mission);
    else if(
      on_foot and not station_faction_.empty() and mission.target_faction == station_faction_
      and mission.destination_settlement.empty()
    )
      fits.push_back(&mission);
    }

  if(hand_in.empty() and here.empty() and fits.empty())
    return {};

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = station_name_, .color = colour_heading()});
  if(not station_faction_.empty())
    lines.push_back(overlay::line_t{.text = std::format("  {}", station_faction_), .color = colour_plain()});

  auto const add = [&](std::vector<info::mission_t const *> const & group, char const * caption, uint32_t colour)
  {
    if(group.empty())
      return;

    lines.push_back(overlay::line_t{.text = caption, .color = colour_heading()});
    size_t const work_rows{rows_for(group.size(), listed_settlement_work())};
    for(info::mission_t const * mission: group | std::views::take(work_rows))
      {
      auto const left{std::chrono::duration_cast<std::chrono::seconds>(mission->expiry - now)};
      auto const count{mission->mission_count()};

      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}{}  {}",
            mission_wording(*mission),
            count > 1u ? std::format("  x{}", count) : std::string{},
            format_remaining(left)
          ),
          .color = left < expiry_warning() ? colour_expiring() : colour
        }
      );
      }

    if(group.size() > work_rows)
      lines.push_back(
        overlay::line_t{.text = std::format("  ... and {} more", group.size() - work_rows), .color = colour_plain()}
      );
  };

  add(hand_in, "hand in here:", colour_first());
  add(here, "do here:", colour_plain());
  add(fits, "any settlement of this faction:", colour_plain());

  return lines;
  }

auto overlay_feed_t::build_supply_lines(events::cargo_file_t const & cargo) const -> std::vector<overlay::line_t>
  {
  if(needs_.empty())
    return {};

  // the names in the hold are internal, in missions readable - we compare by letters and digits alone
  auto const key{[](std::string_view text)
                 {
                   std::string out;
                   for(char const c: text)
                     if(std::isalnum(static_cast<unsigned char>(c)))
                       out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                   return out;
                 }};

  std::map<std::string, uint32_t> aboard;
  for(events::cargo_item_t const & item: cargo.Inventory)
    {
    auto const internal{key(item.Name)};
    aboard[internal] += item.Count;

    // both names can come down to the same key, and then the quantity would be counted twice
    if(auto const localised{key(item.Name_Localised)}; not localised.empty() and localised != internal)
      aboard[localised] += item.Count;
    }

  auto const held{
    [&](std::string const & commodity) -> uint32_t
    {
      auto const it{aboard.find(key(commodity))};
      return it != aboard.end() ? it->second : 0u;
    }
  };

  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = "mission cargo:", .color = colour_heading()});

  std::set<std::string> missing;
  for(info::cargo_need_t const & need: needs_)
    {
    auto const have{held(need.commodity)};
    bool const known{std::ranges::any_of(
      options_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
    )};

    // a market may trade in it and happen to be empty - that is quite different news from having no source
    auto const seller{std::ranges::find_if(
      producers_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
    )};
    bool const sold_somewhere{seller != producers_.end()};

    if(have < need.count)
      missing.insert(need.commodity);

    // the number in brackets is what the hold holds - it is what makes a missing item stand out
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {} x{} ({}){}",
          need.commodity,
          need.count,
          have,
          have >= need.count or known ? std::string{}
          : sold_somewhere            ? std::format("   {} sells it, out of stock", seller->station)
                                      : std::string{"   no source known"}
        ),
        .color = have >= need.count ? colour_first() : (known ? colour_plain() : colour_alert())
      }
    );
    }

  if(missing.empty())
    {
    lines.push_back(overlay::line_t{.text = "all aboard", .color = colour_first()});
    return lines;
    }

  // one place for several missing goods saves a whole run; only after that does the kind of port count
  struct place_t
    {
    std::string station;
    std::string system;
    int rank{};
    std::vector<info::supply_option_t const *> items;
    };

  std::map<uint64_t, place_t> places;
  for(info::supply_option_t const & option: options_)
    {
    if(not missing.contains(option.commodity))
      continue;

    place_t & place{places[option.market_id]};
    if(place.items.empty())
      {
      place.station = option.station.empty() ? std::format("market {}", option.market_id) : option.station;
      place.system = option.system;
      place.rank = station_rank(option.station_type);
      }
    place.items.push_back(&option);
    }

  std::vector<place_t const *> ranked;
  ranked.reserve(places.size());
  for(auto const & [market_id, place]: places)
    ranked.push_back(&place);

  std::ranges::sort(
    ranked,
    [](place_t const * left, place_t const * right)
    {
      if(left->items.size() != right->items.size())
        return left->items.size() > right->items.size();
      return left->rank < right->rank;
    }
  );

  for(place_t const * place: ranked | std::views::take(listed_sources()))
    {
    lines.push_back(
      overlay::line_t{
        .text = std::format("{}{}{}", place->station, place->system.empty() ? "" : "  ", place->system),
        .color = colour_first()
      }
    );

    for(info::supply_option_t const * item: place->items | std::views::take(listed_commodities()))
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} in stock  {} Cr",
            item->commodity,
            format_credits_value(item->stock),
            format_credits_value(item->buy_price)
          ),
          .color = colour_plain()
        }
      );
    }

  return lines;
  }

auto overlay_feed_t::build_route_lines(current_state_t const & state, plotted_route_t const & plotted) const
  -> std::vector<overlay::line_t>
  {
  // a route can run to forty jumps and the side band holds a dozen or so lines - the further ones change
  // nothing about what is being done right now
  size_t const listed_hops{eht::settings()->overlay.lists.route_hops};

  ///\brief at which stars one can refuel
  ///\detail KGBFOAM; the class sometimes comes with a subtype, so what counts is the first letter
  auto const scoopable = [](std::string_view star_class) -> bool
  {
    return not star_class.empty() and std::string_view{"KGBFOAM"}.find(star_class.front()) != std::string_view::npos;
  };

  std::vector<overlay::line_t> lines;

  if(not plotted.waypoints.empty())
    {
    // numerator and denominator both count waypoints - mixing them with jumps gave "8 of 7"
    auto const left{plotted.waypoints.size() - std::min(plotted.reached, plotted.waypoints.size())};
    lines.push_back(
      overlay::line_t{
        .text = std::format("plotted route: {} of {} stops left", left, plotted.waypoints.size()),
        .color = colour_heading()
      }
    );

    for(size_t ix{plotted.reached}; ix < plotted.waypoints.size() and ix < plotted.reached + listed_hops; ++ix)
      {
      info::neutron_waypoint_t const & waypoint{plotted.waypoints[ix]};
      lines.push_back(
        overlay::line_t{
          // a neutron star does not refuel, and the file gives no class for the others
          .text = std::format(
            "  {}. {}  {}  {:.0f} ly",
            ix - plotted.reached + 1u,
            waypoint.system,
            waypoint.neutron ? "N no fuel" : "?",
            waypoint.distance
          ),
          .color = ix == plotted.reached ? colour_first() : colour_plain()
        }
      );
      }
    }

  auto pending{state.route_ | std::views::filter([](info::route_item_t const & item) { return not item.visited; })};
  if(std::ranges::distance(pending) != 0)
    {
    lines.push_back(
      overlay::line_t{
        .text = std::format("game route: {} jumps", std::ranges::distance(pending)), .color = colour_heading()
      }
    );

    size_t shown{};
    for(info::route_item_t const & item: pending | std::views::take(listed_hops))
      {
      bool const fuel{scoopable(item.star_class)};
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}. {}  {}{}  {:.0f} ly",
            ++shown,
            item.system,
            item.star_class.empty() ? "?" : item.star_class,
            fuel ? " fuel" : "",
            item.distance
          ),
          .color = fuel ? colour_first() : colour_plain()
        }
      );
      }
    }

  return lines;
  }

namespace
  {
///\brief after a jump a carrier cannot jump again for five minutes
constexpr std::chrono::minutes carrier_cooldown{5};

///\brief a carrier in one line: where it is, or where it goes, when it leaves and when it is ready again
[[nodiscard]]
auto describe_carrier(info::carrier_state_t const & c, std::chrono::sys_seconds now) -> overlay::line_t
  {
  std::string const who{
    c.name.empty() ? std::format("carrier {}", c.carrier_id)
                   : (c.callsign.empty() ? c.name : std::format("{} ({})", c.name, c.callsign))
  };
  if(not c.jumping)
    return overlay::line_t{.text = std::format("  {}: {}", who, c.system.empty() ? "?" : c.system), .color = colour_plain()};

  auto const minutes = [](auto d) { return std::chrono::duration_cast<std::chrono::minutes>(d).count(); };
  // five minutes after the arrival the next jump can be ordered; before it the arrival is reckoned
  auto const ready{c.arrival + carrier_cooldown};
  if(now < c.departure)
    return overlay::line_t{
      .text = std::format(
        "  {}: {} -> {}, leaves {:%H:%M} UTC (in {} min), ready ~{:%H:%M}",
        who,
        c.from.empty() ? "?" : c.from,
        c.to_body.empty() ? c.to : c.to_body,
        c.departure,
        minutes(c.departure - now) + 1,
        ready
      ),
      .color = colour_first()
    };
  return overlay::line_t{
    .text = std::format(
      "  {}: {} {} -> {} at {:%H:%M} UTC, ready {:%H:%M} (in {} min)",
      who,
      c.arrived ? "arrived" : "jumping",
      c.from.empty() ? "?" : c.from,
      c.to_body.empty() ? c.to : c.to_body,
      c.arrived ? c.arrival : c.departure,
      ready,
      minutes(ready - now) + 1
    ),
    .color = colour_first()
  };
  }
  }  // namespace

auto overlay_feed_t::build_logistics_lines() const -> std::vector<overlay::line_t>
  {
  auto & db{const_cast<database_storage_t &>(db_)};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;

  // The last port decides where an escape pod sends you - settlements and carriers do not count, and
  // after a few stops it is easy to lose track of which of them was a port
  if(auto port{db.load_last_port()}; port and *port)
    lines.push_back(
      overlay::line_t{
        .text = std::format("last port: {}, {} ({})", (*port)->name, (*port)->system, (*port)->station_type),
        .color = colour_plain()
      }
    );

  // one's own carrier and the squadron's: where each is, or where it goes and when it can jump again
  if(auto carriers{db.load_carrier_states(now, carrier_cooldown)}; carriers and not carriers->empty())
    {
    lines.push_back(overlay::line_t{.text = "carriers:", .color = colour_heading()});
    for(info::carrier_state_t const & c: *carriers)
      lines.push_back(describe_carrier(c, now));
    }

  auto pending{db.load_transfers_in_flight(now)};
  if(not pending or pending->empty())
    return lines;

  lines.push_back(overlay::line_t{.text = "ships in transit:", .color = colour_heading()});
  for(info::ship_transfer_t const & transfer: *pending)
    {
    auto const left{std::chrono::duration_cast<std::chrono::minutes>(transfer.arrives - now)};
    auto const station{db.load_station(transfer.to_market_id)};

    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {} -> {}  in {}h {:02}min",
          transfer.ship_type,
          station and *station ? (*station)->name : std::format("market {}", transfer.to_market_id),
          left.count() / 60,
          left.count() % 60
        ),
        // the last quarter of an hour is when it pays to be there
        .color = left < std::chrono::minutes{15} ? colour_first() : colour_plain()
      }
    );
    }

  return lines;
  }

auto overlay_feed_t::refresh_fleet(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  // carriers jump and transfers arrive with no change of the fleet, so the list is read now and then anyway
  if(
    fleet_changes_ == state.fleet_changes_ and fleet_system_ == state.system.system_address
    and now - fleet_read_ < std::chrono::seconds{30}
  )
    return;
  fleet_changes_ = state.fleet_changes_;
  fleet_system_ = state.system.system_address;
  fleet_read_ = now;

  auto placed{fleet::locate(
    db_,
    state.system.name,
    state.system.system_location,
    std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())
  )};
  fleet_ = placed ? std::move(*placed) : std::vector<fleet::placed_ship_t>{};
  }

auto overlay_feed_t::build_fleet_lines() const -> std::vector<overlay::line_t>
  {
  double const radius{eht::settings()->overlay.ships_radius_ly};
  if(radius <= 0.0)
    return {};

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  uint32_t const flown_days{eht::settings()->overlay.ships_flown_days};
  auto const flown_since{now - std::chrono::days{flown_days}};

  std::vector<fleet::placed_ship_t const *> nearby;
  for(fleet::placed_ship_t const & placed: fleet_)
    if(
      not placed.ship.current and placed.distance_ly and *placed.distance_ly <= radius
      and (flown_days == 0u or placed.ship.flown >= flown_since)
    )
      nearby.push_back(&placed);
  if(nearby.empty())
    return {};

  size_t const shown{rows_for(nearby.size(), eht::settings()->overlay.lists.ships)};
  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = flown_days == 0u ? std::format("ships within {:.0f} ly:", radius)
                               : std::format("ships within {:.0f} ly, flown in {} days:", radius, flown_days),
      .color = colour_heading()
    }
  );
  for(fleet::placed_ship_t const * placed: nearby | std::views::take(shown))
    {
    std::string where{
      placed->station.empty() ? placed->system : std::format("{}, {}", placed->system, placed->station)
    };
    std::string distance{
      *placed->distance_ly < 0.05 ? std::string{"here"} : std::format("{:.1f} ly", *placed->distance_ly)
    };
    std::string note;
    if(placed->travelling and placed->ship.arrives != std::chrono::sys_seconds{})
      {
      auto const left{std::chrono::duration_cast<std::chrono::minutes>(placed->ship.arrives - now)};
      note = std::format("  (arrives in {}h {:02}min)", left.count() / 60, left.count() % 60);
      }
    else if(placed->travelling)
      note = "  (in transit)";
    std::string const type{
      placed->ship.name.empty() or placed->ship.name == placed->ship.type_name
        ? std::string{}
        : std::format(" ({})", placed->ship.type_name)
    };
    lines.push_back(
      overlay::line_t{
        .text = std::format("  {}{}  {}  {}{}", fleet::shown_name(placed->ship), type, distance, where, note),
        .color = colour_plain()
      }
    );
    }
  if(nearby.size() > shown)
    lines.push_back(overlay::line_t{.text = std::format("  +{} more", nearby.size() - shown), .color = colour_plain()});
  return lines;
  }

auto overlay_feed_t::publish(current_state_t const & state, plotted_route_t const & plotted) -> void
  {
  // the codex does not need the game - its page is written whether anyone is drawing or not
  if(state.organic_scans_seen_ != pictured_scans_)
    {
    pictured_scans_ = state.organic_scans_seen_;
    codex_.ask_for_picture(state.last_organic_scan_);
    }
  if(codex_.collect())
    codex_.write_page(db_);
  // The scanner's own interface is the tenth. Its views are worth keeping only from a ship in supercruise
  // round the planet - opened from the ground or a Nomad it looks at the sky
  constexpr uint64_t supercruise_flag{1u << 4u};
  constexpr uint64_t main_ship_flag{1u << 24u};
  bool const scanner_on_planet{
    gui_focus_ == 10u and (status_flags_ & supercruise_flag) != 0u and (status_flags_ & main_ship_flag) != 0u
  };
  scanner_.collect(scanner_on_planet);
  scanner_.observe(scanner_on_planet, state.scanner_body_);
  screenshots_.collect();
  refresh_species_history(state);

  if(not server_->listening())
    return;

  refresh_factions(state);
  refresh_status(state);
  refresh_mission_places(state);
  // the route's last system is where we are going; the first entry is where it was plotted from
  info::route_item_t const * const destination{
    state.route_.size() > 1u and state.route_.back().system_address != state.system.system_address
      ? &state.route_.back()
      : nullptr
  };
  refresh_market(
    state.settlement_market_id_,
    state.ship_loadout.CargoCapacity,
    destination != nullptr ? destination->system_address : 0u,
    destination != nullptr ? std::string_view{destination->system} : std::string_view{}
  );
  refresh_supply();

  overlay::frame_t frame{};

  if(not state.system.name.empty())
    {
    auto lines{describe_system(state.system, faction_lines_.empty())};
    lines.insert(lines.end(), faction_lines_.begin(), faction_lines_.end());
    lines.insert(lines.end(), conflict_lines_.begin(), conflict_lines_.end());
    frame.blocks.push_back(
      overlay::block_t{
        // the chart goes under the text, because the names and the current values are what one
        // reads at a glance; the shape of the last ten days is what one studies when there is time
        .corner = overlay::corner_e::top_left,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(lines),
        .charts = faction_charts_
      }
    );
    }

  // a construction site in view - what it still needs, what the hold carries, what this port sells
  refresh_construction(state);
  auto construction{build_construction_lines(state)};
  // flying to a site, the load is the colony's - the trading hints would only be in the way
  bool const construction_shown{not construction.empty()};
  if(construction_shown)
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_right,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(construction),
        .charts = {},
        .text = overlay::text_e::small
      }
    );

  // the wars here - their official state and the settlements fought over, under the system and its factions
  refresh_wars(state);
  for(auto & war: build_war_blocks())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_left,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(war),
        .charts = {},
        .text = overlay::text_e::small
      }
    );

  // exploration in the order the work goes: is the system new, what to map, where to land
  if(auto arrival{overlay_exploration::describe_arrival(state.system)}; not arrival.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms(), .lines = std::move(arrival)}
    );
  if(auto mapping{overlay_exploration::describe_mapping(state.system)}; not mapping.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms(), .lines = std::move(mapping)}
    );
  if(auto life{overlay_exploration::describe_life(state.system, species_history_)}; not life.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_right,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(life),
        .charts = {},
        // a genus a line, read one by one when deciding where to go down
        .text = overlay::text_e::small
      }
    );

  // the least urgent thing on this side, so it goes last - into the corner itself
  // the ports change only with the system, and with a docking that records a new one
  if(
    state.system.system_address != stations_system_
    or std::chrono::steady_clock::now() - stations_loaded_ > std::chrono::seconds{30}
  )
    {
    stations_system_ = state.system.system_address;
    stations_loaded_ = std::chrono::steady_clock::now();
    auto loaded{db_.load_stations(stations_system_)};
    stations_ = loaded ? std::move(*loaded) : std::vector<info::station_t>{};
    }
  if(auto map{build_system_diagram(state.system, stations_, status_body_, status_destination_, state.active_missions)}; map)
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms(), .diagrams = {std::move(*map)}}
    );

  // what can be done here comes first: it is the only thing on this side that asks nothing of the
  // player but to turn around, and the trading below it will still be there afterwards
  // Head-up, flanking the middle of the centre screen: what is read without looking away from the
  // fight. The corridor between them is left clear, because that is where the fight is.
  // Both give way to an interface that takes over that screen - the galaxy map is not a fight, and
  // whatever is written over it is in the way. The side bands stay: they are on other screens
  bool const interface_open{gui_focus_ >= first_fullscreen_interface};

  // the sampling from the ship only in the analysis mode - in the combat one the ship is flown for something
  // else. On foot the sampler in hand does too, a weapon in hand means a fight. An SRV has no such mode
  constexpr uint64_t in_srv_flag{1u << 26u};
  constexpr uint64_t analysis_mode_flag{1u << 27u};
  constexpr uint64_t on_foot_flag{1u << 0u};
  bool const on_foot{(status_flags2_ & on_foot_flag) != 0u};
  bool const exobio_mode{
    (status_flags_ & analysis_mode_flag) != 0u or (on_foot and surface_.sampler_in_hand)
    or (not on_foot and (status_flags_ & in_srv_flag) != 0u)
  };

  if(auto aimed{interface_open ? std::vector<overlay::line_t>{} : build_target_lines(state)}; not aimed.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::centre_top_left, .ttl_ms = block_ttl_ms(), .lines = std::move(aimed)}
    );
  // on the ground there is rarely a target, and the same place serves the sampling: the distance to the
  // last sample is read while walking, eyes on the ground ahead
  else if(
    auto sampling{
      interface_open or not exobio_mode
        ? std::vector<overlay::line_t>{}
        : overlay_exploration::describe_sampling(state.system, state.sampling, surface_, species_history_)
    };
    not sampling.empty()
  )
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::centre_top_left, .ttl_ms = block_ttl_ms(), .lines = std::move(sampling)
      }
    );

  if(auto crew{interface_open ? std::vector<overlay::line_t>{} : build_crew_lines(state)}; not crew.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::centre_top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(crew)}
    );

  if(auto settlement{build_settlement_lines(state)}; not settlement.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(settlement)}
    );

  if(auto supply{build_supply_lines(state.cargo)}; not supply.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(supply)}
    );

  // the market underneath, because it is longer and less urgent than what the missions still lack
  if(not market_lines_.empty() and not construction_shown)
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_right,
        .ttl_ms = block_ttl_ms(),
        .lines = market_lines_,
        .charts = {},
        // the longest list of all, and every row of it a price
        .text = overlay::text_e::small
      }
    );

  if(auto logistics{build_logistics_lines()}; not logistics.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms(), .lines = std::move(logistics)}
    );

  // our ships nearby - which one to go and take, or have brought over
  refresh_fleet(state);
  if(auto ships{build_fleet_lines()}; not ships.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(ships),
        .charts = {},
        // a list read when choosing, not glanced at in flight
        .text = overlay::text_e::small,
        .diagrams = {},
        .pictures = {},
        .picture_columns = 3u,
        // the stack on the left reaches up to the faction block already, so it goes beside it
        .beside = true
      }
    );

  // the route on the left, above the hold - in flight it is the thing one looks at
  if(auto route{build_route_lines(state, plotted)}; not route.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms(), .lines = std::move(route)}
    );

  refresh_unsold(state);
  if(not at_risk_.samples.empty() or at_risk_.bounties != 0u or at_risk_.cartography != 0u)
    {
    uint64_t bio_total{};
    for(bio::unsold_t const & sample: at_risk_.samples)
      bio_total += sample.value + sample.bonus;
    // what a death would cost - worth knowing before a hard landing or a fight. Combat bonds survive it
    std::vector<std::string> parts;
    if(not at_risk_.samples.empty())
      parts.push_back(
        std::format("bio {} ({} samples)", overlay_exploration::short_credits(bio_total), at_risk_.samples.size())
      );
    if(at_risk_.cartography != 0u)
      parts.push_back(std::format("cartography ~{}", overlay_exploration::short_credits(at_risk_.cartography)));
    if(at_risk_.bounties != 0u)
      parts.push_back(std::format("bounties {}", overlay_exploration::short_credits(at_risk_.bounties)));
    std::string text{"lost on death:"};
    for(auto const & [index, part]: parts | std::views::enumerate)
      text += std::format("{}{}", index == 0 ? " " : " + ", part);
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms(),
        .lines = {overlay::line_t{.text = std::move(text), .color = colour_alert()}}
      }
    );

    // when the danger is now, the same sum goes where the eyes are - heat, an interdiction, the game's own
    // warning of danger
    constexpr uint64_t overheating_flag{1u << 20u};
    constexpr uint64_t in_danger_flag{1u << 22u};
    constexpr uint64_t interdicted_flag{1u << 23u};
    if(
      not interface_open and (status_flags_ & (overheating_flag | in_danger_flag | interdicted_flag)) != 0u
    )
      frame.blocks.push_back(
        overlay::block_t{
          .corner = overlay::corner_e::centre_top_right,
          .ttl_ms = block_ttl_ms(),
          .lines = {overlay::line_t{
            .text = std::format(
              "at stake: {}",
              overlay_exploration::short_credits(bio_total + at_risk_.bounties + at_risk_.cartography)
            ),
            .color = colour_alert()
          }}
        }
      );
    }

  // Who probably holds a bounty on the commander - worth knowing before flying to their port, since the
  // legal state below is only the jurisdiction of this system's controlling faction
  if(not legal_.holders.empty() or legal_.notoriety != 0u)
    {
    std::vector<overlay::line_t> lines;
    std::string text{legal_.holders.empty() ? "no bounty probably" : "bounty probably with:"};
    for(bounty_holder_t const & holder: legal_.holders)
      text += std::format("  {} ({:%d.%m})", holder.faction, holder.last_crime);
    // while notoriety lasts the squadron's decay goes on clearing bounties; once it runs out it stops -
    // the game writes it only at login
    if(legal_.notoriety != 0u)
      text += std::format("   notoriety {} at {:%H:%M}", legal_.notoriety, legal_.notoriety_at);
    lines.push_back(overlay::line_t{.text = std::move(text), .color = colour_alert()});

    // a port of one of them as the destination or the place docked at - pay first
    std::string const & here{station_faction_};
    std::string destination_owner;
    if(status_destination_ and status_destination_->System == state.current_system_address_)
      for(info::station_t const & station: stations_)
        if(station.name == status_destination_->Name)
          destination_owner = station.controlling_faction;
    for(bounty_holder_t const & holder: legal_.holders)
      if(holder.faction == destination_owner or (state.settlement_market_id_ != 0u and holder.faction == here))
        lines.push_back(
          overlay::line_t{
            .text = std::format("  {} holds a bounty on you here - pay it before docking", holder.faction),
            .color = colour_expiring()
          }
        );
    frame.blocks.push_back(overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms(), .lines = std::move(lines)});
    }

  // Wanted here, or worse - the game's own word, and the only one: the sums of bounties are in no file,
  // and a squadron's Notoriety Decay changes them without a trace in the journal
  if(not legal_state_.empty() and legal_state_ != "Clean")
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms(),
        .lines = {overlay::line_t{.text = std::format("legal state here: {}", legal_state_), .color = colour_alert()}}
      }
    );

  if(auto cargo{describe_cargo(state.cargo)}; not cargo.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms(), .lines = std::move(cargo)}
    );

  if(auto missions{describe_missions(state.active_missions, place_owner_)}; not missions.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(missions),
        .charts = {},
        // a list read line by line, not glanced at
        .text = overlay::text_e::small
      }
    );

  if(auto owners{build_settlement_owners()}; not owners.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(owners),
        .charts = {},
        // looked up before taking a job, not glanced at in flight
        .text = overlay::text_e::small
      }
    );

  if(not state.next_target.Name.empty() and state.next_target.Name != state.system.name)
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms(),
        .lines = {overlay::line_t{
          .text = std::format(
            "next: {} ({})",
            state.next_target.Name,
            state.next_target.StarClass.empty() ? "?" : state.next_target.StarClass
          ),
          .color = colour_plain()
        }}
      }
    );

  // the layout rides with every frame: the layer keeps nothing of its own, so a saved settings file shows
  // in the game at the next frame
  frame.layout = eht::settings()->overlay.layout;
  if(auto const & asked{codex_.capture_request()}; asked.id != codex_capture_id_)
    {
    codex_capture_id_ = asked.id;
    capture_ = asked;
    }
  if(auto const & asked{scanner_.capture_request()}; asked.id != scanner_capture_id_)
    {
    scanner_capture_id_ = asked.id;
    capture_ = asked;
    }
  frame.capture = capture_;
  frame.screenshot.key = eht::settings()->screenshots.key;

  // In the scanner the band says what the last picture did: a new view is the sign that the filter is in
  // and the next one may be chosen
  if(gui_focus_ == 10u and eht::settings()->exploration.scanner_pictures and not state.scanner_body_.empty())
    {
    std::vector<overlay::line_t> lines{overlay::line_t{
      .text = std::format(
        "scanner views of {}: {} kept",
        short_body_name(state.system.name, state.scanner_body_),
        scanner_.view_count(state.scanner_body_)
      ),
      .color = colour_heading()
    }};
    if(auto const & news{scanner_.news()}; news and news->body == state.scanner_body_)
      {
      auto const ago{std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - news->at)};
      lines.push_back(
        overlay::line_t{
          .text = std::format("view {} {}  -  {} s ago", news->view, news->fresh ? "new" : "refreshed", ago.count()),
          // a new view stands out for a few seconds, the time it takes to see it and switch the filter
          .color = news->fresh and ago < std::chrono::seconds{4} ? colour_first() : colour_plain()
        }
      );
      }
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms(), .lines = std::move(lines)}
    );
    }

  // On the ground the scanner's views come back: its filters showed from orbit where each genus grows.
  // Not in the scanner itself, where they are being taken and the middle of the screen is its own
  if(gui_focus_ != 10u and surface_.here and not surface_.body_name.empty())
    if(auto pictures{scanner_.pictures(surface_.body_name)}; not pictures.empty())
      frame.blocks.push_back(
        overlay::block_t{
          .corner = overlay::corner_e::bottom_left,
          .ttl_ms = block_ttl_ms(),
          .lines = {overlay::line_t{
            .text = std::format("scanner views of {}", short_body_name(state.system.name, surface_.body_name)),
            .color = colour_heading()
          }},
          .pictures = std::move(pictures),
          .picture_columns = eht::settings()->exploration.scanner_columns
        }
      );

  // the game gets a frame when it has changed or when the keep-alive time has passed - not on every journal event
  auto const now{std::chrono::steady_clock::now()};
  if(same_content(frame, last_) and now - last_sent_ < heartbeat())
    return;

  frame.seq = ++sequence_;
  server_->publish(frame);
  last_ = std::move(frame);
  last_sent_ = now;
  }
