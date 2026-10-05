#include <overlay_feed.h>
#include <biology.h>
#include <territory.h>
#include <data/bgs.h>
#include <data/carrier.h>
#include <data/colonisation.h>
#include <data/market.h>
#include <data/missions.h>
#include <data/navigation.h>
#include <data/ships.h>
#include <data/station.h>
#include <data/war.h>
#include <backup.h>
#include <evidence_log.h>
#include <netstate.h>
#include <picture_records.h>
#include <port_model.h>
#include <commodity_facts.h>
#include <construction_window.h>
#include <eht_settings.h>
#include <qformat.h>

#include <spdlog/spdlog.h>
#include <world_target.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <functional>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <ranges>
#include <format_credits.h>
#include <orbit.h>

namespace
  {
///\brief the interfaces that take over the middle screen, from GuiFocus
///\detail 5 station services, 6 galaxy map, 7 system map, 8 orrery, 9 FSS, 10 surface scanner,
/// 11 codex. The cockpit panels below 5 leave the middle of the screen alone, and so may we
constexpr uint32_t first_fullscreen_interface{5u};
constexpr uint32_t galaxy_map_focus{6u};

// What used to be constants here now comes from the settings file, read afresh at every use so that a
// saved change shows at the next refresh. Each accessor takes the snapshot in force at that moment
auto colour_heading() -> uint32_t { return eht::settings()->overlay.colours.heading.rgb; }
auto colour_plain() -> uint32_t { return eht::settings()->overlay.colours.plain.rgb; }
auto colour_alert() -> uint32_t { return eht::settings()->overlay.colours.alert.rgb; }
///\brief a market whose controlling faction may have turned over since we last read it
auto colour_market_stale() -> uint32_t { return eht::settings()->overlay.colours.market_stale.rgb; }
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

///\brief the allegiance as edworld counts it (overlay::world::allegiance_code)
[[nodiscard]]
auto edworld_allegiance(info::allegiance_e allegiance) noexcept -> uint8_t
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation:  return 1u;
    case empire:      return 2u;
    case alliance:    return 3u;
    case independent: return 4u;
    case unknown:     return 0u;
    default:          return 5u;
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
  overlay::frame_t l{left};
  overlay::frame_t r{right};
  l.seq = 0u;
  r.seq = 0u;
  std::string const a{overlay::to_json(l)};
  std::string const b{overlay::to_json(r)};
  if(a.empty() or b.empty()) [[unlikely]]
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
    return 0.24f;
  if(air.contains("thin"))
    return 0.1f;
  return 0.16f;
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
  std::span<info::mission_t const> missions,
  planet_faces_t & faces
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

  // where the star of the row being drawn stands - the light comes from there
  float star_x{};
  float star_y{};
  // a planet or moon as a ball, with its face once the scanner has shown it; a face hides the colour
  // that said there is life, so a ring around the ball says it instead
  auto const body_disc = [&](body_t const * body, planet_details_t const & details, float x, float y, float r)
  {
    std::string const full{body->name.empty() ? system.name : system.name + " " + body->name};
    std::string face{faces.face(system.name, full, class_colour(details))};
    bool const ringed{not face.empty() and has_bio(details)};
    diagram.discs.push_back(planet_disc(details, x, y, r, star_x, star_y, std::move(face)));
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
          star_disc(sd, x, line_y, star_radius)
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
        star_disc(sd, lead_x, line_y, star_radius)
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
  // a line every six hours, but a label only as often as one fits above the next - in a low chart the
  // labels would stand one on another. A label wants the height of a line of text, 13 pixels at scale 1
  constexpr uint32_t label_room{20u};
  int const label_every{chart.height / 4u >= label_room ? 6 : chart.height / 2u >= label_room ? 12 : 24};
  for(int hour{}; hour <= 24; hour += 6)
    chart.grid.push_back(
      overlay::grid_line_t{
        .y = static_cast<float>(hour) / 24.f,
        .label = hour == 0 or hour == 24 or hour % label_every != 0 ? std::string{} : std::format("{:02}h", hour)
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
    db_{db_path},
    faces_{db_.live_db_path_}
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

auto overlay_feed_t::refresh_factions(
  current_state_t const & state, uint64_t system_address, std::string_view controlling, system_factions_t & view
) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  bool const same_system{system_address == view.system};

  // influence moves once a day, so asking the database every frame would be a waste
  if(same_system and now - view.loaded < faction_refresh())
    return;

  view.system = system_address;
  view.loaded = now;
  view.lines.clear();
  view.conflicts.clear();
  bool war_running{};
  view.charts.clear();
  view.factions.clear();

  if(view.system == 0u)
    return;

  // one line fits in the side band, so what is left is the hour alone and whether the wave has reached
  // here - the spread across systems and the statistics are visible in the system window
  auto const wall_clock{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto const tick_line = [&](info::tick_kind_e kind, std::string_view caption) -> overlay::line_t
  {
    tick_view_t const tick{describe_tick(db_, view.system, kind, wall_clock)};
    return overlay::line_t{
      .text = std::format(
        "{} {}{}",
        caption,
        tick.here,
        not tick.awaiting ? ""
        : tick.seen_since ? "  (wave started, unchanged here since)"
                          : "  (wave started, not seen here since)"
      ),
      // unchanged at a visit after the wave began is no warning: the tick may have come and moved nothing
      .color = tick.awaiting and not tick.seen_since ? colour_alert() : colour_plain()
    };
  };

  view.lines.push_back(tick_line(info::tick_kind_e::influence, "BGS tick"));

  if(auto conflicts{db_.load_conflicts(view.system)}; conflicts)
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

      view.conflicts.push_back(
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
        view.conflicts.push_back(
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
  if(not view.conflicts.empty() or war_running)
    {
    // a conflict is settled at the fourth day won, so the end can be counted down without knowing when
    // the recalculation falls - and that is exactly when bonds are worth the most
    if(auto countdown{db_.load_war_countdown(view.system)}; countdown)
      for(info::war_countdown_t const & war: *countdown)
        {
        if(not war.active)
          continue;

        bool const decides_now{war.ticks_left == 0u};
        view.conflicts.insert(
          view.conflicts.begin(),
          overlay::line_t{
            .text = decides_now ? std::format("{}: decided at the next tick - have bonds ready", war.war_type)
                                : std::format("{}: {} more war ticks", war.war_type, war.ticks_left),
            .color = decides_now ? colour_alert() : colour_plain()
          }
        );
        }

    view.conflicts.insert(view.conflicts.begin(), tick_line(info::tick_kind_e::war, "war tick"));
    }

  auto history{db_.load_influence_history(view.system)};
  if(not history)
    {
    spdlog::error("overlay feed: failed to load influence for {}", view.system);
    return;
    }

  // a faction thrown out of the system stops appearing in the readings, but its last influence row stays
  // behind - which is why the list is narrowed to the ones seen at the newest reading
  std::set<int64_t> present;
  if(auto refs{db_.load_present_factions(view.system)}; refs)
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
    info::government_e government;
    std::string active;
    ///\brief the states the faction is recovering from, an Expansion's cooldown among them - not an
    /// alert, just a lower-urgency note shown when there is no active state to report instead
    std::string recovering;
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
      .government = info::government_e::unknown,
      // the states of this system only. FactionState is a state of the faction taken from one of its
      // systems - a retreat from where it is weak shows in every system it is in, at 20% as well - and the one
      // state the game does give every system of the faction, Expansion, is in ActiveStates already
      .active = entry->active_states,
      .recovering = entry->recovering_states,
      .influence = entry->influence
    };

    if(
      auto it{std::ranges::find(state.known_factions, oid, &info::faction_info_t::oid)};
      it != state.known_factions.end()
    )
      {
      item.name = it->name;
      item.allegiance = it->allegiance;
      item.government = it->government;
      }

    if(not item.name.empty())
      presence.emplace_back(std::move(item));
    }

  // an uninhabited system has no background simulation at all, so the tick hours would only take room
  if(presence.empty())
    {
    view.lines.clear();
    view.conflicts.clear();
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

  // The game shows each faction's economy and security as a bar with a trend, and writes neither to the
  // journal. What it does write is which way each handed-in mission pushed them - counted from the last
  // wave, that is the direction of one's own work on the bar the next tick moves. Others' work is not in it
  std::map<std::string, info::state_effort_t> state_effort;
  if(auto effort{db_.load_state_effort(view.system)}; effort)
    for(info::state_effort_t & row: *effort)
      state_effort.emplace(row.faction, std::move(row));
  auto const pushes = [](int32_t up, int32_t down) -> std::string
  {
    if(up != 0 and down != 0)
      return std::format("+{}/-{}", up, down);
    return up != 0 ? std::format("+{}", up) : std::format("-{}", down);
  };
  auto const state_pushes = [&](presence_t const & item) -> std::vector<std::string>
  {
    auto const it{state_effort.find(item.name)};
    if(it == state_effort.end())
      return {};
    info::state_effort_t const & effort{it->second};
    std::vector<std::string> shown;
    if(effort.economy_up != 0 or effort.economy_down != 0)
      shown.push_back(std::format("EP {}", pushes(effort.economy_up, effort.economy_down)));
    // an anarchy's security stays in the middle whatever is done to it
    if(item.government != info::government_e::anarchy and (effort.security_up != 0 or effort.security_down != 0))
      shown.push_back(std::format("SP {}", pushes(effort.security_up, effort.security_down)));
    return shown;
  };

  // what goes on in the faction: its states first, then the pushes on the bars that lead to them
  auto const states_of = [&](presence_t const & item) -> std::string
  {
    std::string states{
      not item.active.empty() ? item.active
      : not item.recovering.empty() ? std::format("{} (recovering)", item.recovering)
                                     : std::string{}
    };
    for(std::string const & push: state_pushes(item))
      states += (states.empty() ? "" : "  ") + push;
    return states;
  };

  for(presence_t const & item: presence)
    view.factions.push_back(
      listed_faction_t{
        .name = item.name,
        .states = states_of(item),
        .influence = item.influence,
        .allegiance = edworld_allegiance(item.allegiance),
        .trend = item.trend,
        .controlling = item.name == controlling
      }
    );

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

    std::string suffix{states_of(item)};

    view.lines.push_back(
      overlay::line_t{
        // the star marks the controlling faction, because that one decides the system's face
        .text = std::format(
          "{}{}  {:.1f}%",
          item.name == controlling ? "* " : "  ",
          item.name,
          item.influence * 100.0
        ),
        .color = colour,
        // the same shape the faction's line wears on the chart below, so the two read as one
        .marker = marker,
        .emblem = allegiance_emblem(item.allegiance),
        // the mark goes between the value and the states, because it speaks about the value
        .trend = item.trend,
        // the pushes on the bars after the states, which they lead to
        .suffix = std::move(suffix)
      }
    );
    }

  view.charts = build_influence_chart(*history, charted, wall_clock);

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
    view.charts.push_back(std::move(*chart));
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
  market_unknown_ = false;
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
    market_unknown_ = true;
    market_lines_.push_back(overlay::line_t{.text = std::format("{}: no market data", name), .color = colour_alert()});
    if(not owner.empty())
      market_lines_.push_back(overlay::line_t{.text = std::format("  {}", owner), .color = colour_first()});
    market_lines_.push_back(overlay::line_t{.text = "  open the commodity market to record it", .color = colour_plain()});
    return;
    }

  // Market.json itself never says whose the place is; a takeover since our last reading can flip
  // what is legal to sell here, and the game gives no warning of its own
  if(auto info{db_.load_market_info(market_id)}; info and *info)
    if(std::string const & seen{(*info)->controlling_faction}; not seen.empty() and not owner.empty() and seen != owner)
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format("{}: controlling faction changed since last reading - reopen the market", name),
          .color = colour_market_stale()
        }
      );

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

namespace
  {
///\brief the names in the hold are internal, in missions readable - compared by letters and digits alone
auto supply_key(std::string_view text) -> std::string
  {
  std::string out;
  for(char const c: text)
    if(std::isalnum(static_cast<unsigned char>(c)))
      out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  return out;
  }
}  // namespace

auto overlay_feed_t::refresh_supply() -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(now - supply_loaded_ < supply_refresh())
    return;

  supply_loaded_ = now;
  needs_.clear();
  options_.clear();
  producers_.clear();
  micro_resource_commodities_.clear();

  if(auto needs{db_.load_cargo_needs()}; needs)
    needs_ = std::move(*needs);

  if(needs_.empty())
    return;

  if(auto options{db_.load_supply_options()}; options)
    options_ = std::move(*options);

  if(auto producers{db_.load_producers()}; producers)
    producers_ = std::move(*producers);

  if(auto data{db_.load_micro_resource_commodities()}; data)
    for(std::string const & name: *data)
      micro_resource_commodities_.insert(supply_key(name));
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
    heading_ = status->Heading;
    if(surface_.here and surface_.planet_radius > 0.0)
      ground_speed_.push(now, *surface_.here, surface_.planet_radius);
    else
      ground_speed_.clear();
    status_body_ = std::move(status->BodyName);
    status_destination_ = std::move(status->Destination);
    }
  }

auto overlay_feed_t::build_surface_nav_lines() const -> std::vector<overlay::line_t>
  {
  if(not surface_target_)
    return {};
  nav::target_t const & target{*surface_target_};
  std::string const name{target.label.empty() ? std::string{"target"} : target.label};

  // near another body the target means nothing yet - but it is worth a word, as the way there is the game's
  if(status_body_ != target.body or not surface_.here or surface_.planet_radius <= 0.0)
    {
    if(status_body_.empty())
      return {};
    return {overlay::line_t{.text = std::format("{} is on {}", name, target.body), .color = colour_plain()}};
    }

  nav::guidance_t const way{nav::guide(*surface_.here, heading_, target.point, surface_.planet_radius)};
  // close enough to see it with one's own eyes - from there it is the ground that leads, not the numbers
  constexpr double arrived_m{25.0};
  if(way.distance_m < arrived_m)
    return {overlay::line_t{.text = std::format("{} reached", name), .color = colour_first()}};

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = std::format("{}  {}", name, nav::format_distance(way.distance_m)),
      .color = colour_heading(),
      .pointer = way.turn_deg ? std::optional{static_cast<float>(*way.turn_deg)} : std::nullopt
    }
  );

  std::string detail{std::format("course {:.0f}\u00b0", way.bearing_deg)};
  if(way.turn_deg)
    detail += std::format("  {}", nav::format_turn(*way.turn_deg));
  // the time only when we are going somewhere - and towards the target rather than away from it
  constexpr double moving_mps{0.5};
  if(auto const speed{ground_speed_.metres_per_second(std::chrono::steady_clock::now())};
     speed and *speed > moving_mps and (not way.turn_deg or std::abs(*way.turn_deg) < 90.0))
    detail += std::format(
      "  ~{}", nav::format_duration(std::chrono::seconds{static_cast<int64_t>(way.distance_m / *speed)})
    );
  lines.push_back(overlay::line_t{.text = std::move(detail), .color = colour_plain()});
  return lines;
  }

auto overlay_feed_t::refresh_neutron_route(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(now - neutron_route_loaded_ >= std::chrono::seconds{5})
    {
    neutron_route_loaded_ = now;
    if(auto route{db_.load_neutron_route()}; route)
      {
      // a route replaced or cleared starts the progress over - matching by name would still break on
      // a route flown a second time, so a size change is the simplest sign it is not the same one
      if(route->size() != neutron_route_.size())
        neutron_reached_ = 0u;
      neutron_route_ = std::move(*route);
      }
    }

  if(neutron_route_.empty())
    return;

  // the same walk-forward the Route window itself does, independent of whether that window is open
  if(state.current_system_address_ != neutron_progress_system_)
    {
    neutron_progress_system_ = state.current_system_address_;
    auto const ahead{neutron_route_ | std::views::drop(neutron_reached_)};
    if(auto const here{
         std::ranges::find(ahead, state.current_system_address_, &info::neutron_waypoint_t::system_address)
       };
       here != ahead.end())
      neutron_reached_ += size_t(std::ranges::distance(ahead.begin(), here)) + 1u;
    }
  }

auto overlay_feed_t::build_neutron_checklist_lines(current_state_t const & state, plotted_route_t const & plotted) const
  -> std::vector<overlay::line_t>
  {
  // a route loaded once in the Route window is never remembered, yet it is the one being flown
  std::span<info::neutron_waypoint_t const> const route{plotted.waypoints.empty() ? neutron_route_ : plotted.waypoints};
  size_t const reached{plotted.waypoints.empty() ? neutron_reached_ : plotted.reached};
  if(route.empty() or reached >= route.size())
    return {};

  // only in flight, supercruise, the main ship - a docked or on-foot state has nothing to do with the
  // next jump, and would only clutter what actually matters there instead
  constexpr uint64_t supercruise_flag{1u << 4u};
  constexpr uint64_t main_ship_flag{1u << 24u};
  constexpr uint64_t hyperdrive_charging_flag{1u << 19u};
  if((status_flags_ & supercruise_flag) == 0u or (status_flags_ & main_ship_flag) == 0u)
    return {};

  info::neutron_waypoint_t const & next{route[reached]};
  info::neutron_waypoint_t const * const here{
    reached > 0u and route[reached - 1u].system_address == state.current_system_address_ ? &route[reached - 1u]
                                                                                          : nullptr
  };
  bool const supercharged{state.supercharged_in_ != 0u and state.supercharged_in_ == state.current_system_address_};
  bool const plotted_to_next{
    (not state.route_.empty() and state.route_.back().system_address == next.system_address)
    or (status_destination_ and status_destination_->System == next.system_address)
  };

  std::string step;
  if((status_flags2_ & hyperdrive_charging_flag) != 0u)
    step = std::format("jumping to {}...", next.system);
  else if(here != nullptr and here->neutron and not supercharged)
    step = "supercharge: fly into the neutron star's cone";
  else if(plotted_to_next)
    step = std::format("{}jump to {}", supercharged ? "supercharged - " : "", next.system);
  else if(not extension_hint_.empty())
    step = extension_hint_;
  else
    step = std::format(
      "{}open the galaxy map and plot {}", supercharged ? "supercharged - " : "", next.system
    );

  return {
    overlay::line_t{.text = "neutron highway:", .color = colour_heading()},
    overlay::line_t{.text = std::format("  {}", step), .color = colour_plain()}
  };
  }

auto overlay_feed_t::build_settlement_owners(star_system_t const & system) const -> std::vector<overlay::line_t>
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
  // the settlement we stand at now, if any - what every other one's distance is measured from
  std::optional<uint32_t> here_body_id;
  std::optional<bio::surface_point_t> here_point;
  if(not station_name_.empty())
    if(auto it{std::ranges::find(stations_, station_name_, &info::station_t::name)}; it != stations_.end())
      {
      here_body_id = it->body_id;
      if(it->latitude and it->longitude)
        here_point = bio::surface_point_t{.latitude = *it->latitude, .longitude = *it->longitude};
      }

  // each scanned body's position now, keyed by body_id - empty when we stand nowhere a settlement could
  // be measured from, so the walk below costs nothing when it would go unused
  std::unordered_map<events::body_id_t, events::body_location_t> positions_now;
  if(here_body_id)
    {
    std::vector<body_t const *> scans;
    scans.reserve(system.bodies.size());
    for(body_t const & b: system.bodies)
      scans.push_back(&b);
    auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
    for(events::body_location_t const & loc: body_positions_now(system.bary_centre, scans, now))
      positions_now.emplace(loc.body_id, loc);
    }
  auto const here_pos{
    here_body_id ? [&]() -> events::body_location_t const *
    {
      auto const it{positions_now.find(*here_body_id)};
      return it != positions_now.end() ? &it->second : nullptr;
    }()
                 : nullptr
  };

  // the faction, then its settlements, both in alphabetical order - std::map keeps it so; the economy
  // rides along, since it says what the settlement's missions and its market are about
  struct settlement_info_t
    {
    std::string economy;
    std::optional<uint32_t> body_id;
    std::optional<double> latitude;
    std::optional<double> longitude;
    };
  std::map<std::string, std::map<std::string, settlement_info_t>> by_owner;
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
      .emplace(
        station.name,
        settlement_info_t{
          .economy = station.economy,
          .body_id = station.body_id,
          .latitude = station.latitude,
          .longitude = station.longitude
        }
      );
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
    ///\brief the settlement we stand at - painted green as well as marked, so the eye finds it at once
    bool here{};
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
    for(auto const & [name, info]: names)
      longest_name = std::max(longest_name, shortened(name).size());

  std::vector<entry_t> entries;
  size_t widest{};
  for(auto const & [owner, names]: by_owner)
    {
    entries.push_back(entry_t{.text = owner, .owner = &owner, .faction = true});
    widest = std::max(widest, owner.size() + continued.size());
    for(auto const & [name, info]: names)
      {
      std::string text{(name == here ? "> " : "  ") + shortened(name)};
      if(not info.economy.empty())
        {
        text.resize(2u + longest_name + 2u, ' ');
        text += info.economy;
        }
      // on the body we already stand on, a real surface distance means more than a Ls figure sized for
      // between stars; off it, the distance is between two scanned bodies' own positions right now
      if(name == here)
        ;
      else if(here_point and info.body_id and info.body_id == here_body_id and info.latitude and info.longitude)
        {
        if(auto const body_it{std::ranges::find(system.bodies, *info.body_id, body_body_id_proj)};
           body_it != system.bodies.end())
          {
          double const metres{bio::surface_distance_m(
            *here_point, bio::surface_point_t{.latitude = *info.latitude, .longitude = *info.longitude},
            body_it->radius
          )};
          text += std::format("  {}", nav::format_distance(metres));
          }
        }
      else if(here_pos and info.body_id and info.body_id != here_body_id)
        if(auto const it{positions_now.find(*info.body_id)}; it != positions_now.end())
          {
          double const dx{it->second.x - here_pos->x};
          double const dy{it->second.y - here_pos->y};
          double const dz{it->second.z - here_pos->z};
          double const ls{std::sqrt(dx * dx + dy * dy + dz * dz) / info::light_speed_mps};
          text += std::format("  {:.0f} Ls", ls);
          }
      entries.push_back(entry_t{.text = std::move(text), .owner = &owner, .faction = false, .here = name == here});
      }
    }
  for(entry_t const & entry: entries)
    widest = std::max(widest, entry.text.size());

  auto const cfg{eht::settings()};
  size_t const rows{std::max<size_t>(cfg->overlay.lists.settlement_rows, 2u)};
  // the text is monospaced, so a column is its widest line and two spaces; three at most
  size_t const pitch{widest + 2u};
  size_t const most_columns{std::clamp<size_t>(cfg->overlay.lists.settlement_line_chars / pitch, 1u, 3u)};

  struct cell_t
    {
    std::string text;
    bool here;
    };
  struct layout_t
    {
    std::vector<std::vector<cell_t>> columns;
    size_t shown;
    };
  auto const lay_out = [&](size_t column_rows) -> layout_t
  {
    layout_t layout{.columns = std::vector<std::vector<cell_t>>(1u), .shown = 0u};
    for(entry_t const & entry: entries)
      {
      auto & column{layout.columns.back()};
      // a faction never stands alone at the foot of a column, away from its settlements
      bool const full{column.size() >= column_rows or (entry.faction and column.size() + 1u >= column_rows)};
      if(full and not column.empty())
        {
        if(layout.columns.size() == most_columns)
          break;
        layout.columns.emplace_back();
        // a faction's list going on in the new column says whose it still is
        if(not entry.faction)
          layout.columns.back().push_back(cell_t{.text = *entry.owner + std::string{continued}, .here = false});
        }
      layout.columns.back().push_back(cell_t{.text = entry.text, .here = entry.here});
      layout.shown += entry.faction ? 0u : 1u;
      }
    return layout;
  };

  // Filled to the full height, a list a few rows too long spills one settlement into a column of its own
  // and stands as tall as it can, over whatever is above it. So the columns it takes at the full height
  // are levelled instead: the lowest height that still fits in as many
  layout_t layout{lay_out(rows)};
  size_t const needed{layout.columns.size()};
  if(layout.shown == count)
    for(size_t lower{(entries.size() + needed - 1u) / needed}; lower < rows; ++lower)
      if(layout_t levelled{lay_out(lower)}; levelled.shown == count and levelled.columns.size() <= needed)
        {
        layout = std::move(levelled);
        break;
        }
  auto const & columns{layout.columns};
  size_t const shown{layout.shown};

  std::vector<overlay::line_t> lines;
  // only the places visited or flown close to are known - the game lists no others, and nothing is downloaded
  lines.push_back(
    overlay::line_t{.text = std::format("settlements known here: {}", count), .color = colour_heading()}
  );
  size_t const height{std::ranges::max(columns, {}, &std::vector<cell_t>::size).size()};
  for(size_t row{}; row != height; ++row)
    {
    overlay::line_t line{.color = colour_plain()};
    for(size_t column{}; column != columns.size(); ++column)
      {
      if(row >= columns[column].size())
        continue;
      cell_t const & cell{columns[column][row]};
      line.text.resize(column * pitch, ' ');
      if(cell.here)
        line.spans.push_back(
          overlay::span_t{
            .from = uint32_t(line.text.size()), .length = uint32_t(cell.text.size()), .color = colour_first()
          }
        );
      line.text += cell.text;
      }
    lines.push_back(std::move(line));
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
    // the faction the victim served and the ones paying for it are different ones, and a bare faction
    // name under the kill reads as the victim's - so both are named for what they are
    auto const & bounty{state.last_bounty};
    std::string victim{bounty.PilotName_Localised.empty() ? bounty.VictimFaction : bounty.PilotName_Localised};
    if(not bounty.PilotName_Localised.empty() and not bounty.VictimFaction.empty())
      victim += std::format(" ({})", bounty.VictimFaction);
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "killed {}  +{} Cr", victim,
          format_credits_value(uint32_t(std::min<uint64_t>(bounty.TotalReward, 0xffffffffull)))
        ),
        .color = colour_first()
      }
    );

    for(events::bounty_reward_t const & reward: bounty.Rewards)
      lines.push_back(
        overlay::line_t{
          .text = std::format("  paid by {}  {} Cr", reward.Faction, format_credits_value(uint32_t(reward.Reward))),
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

auto overlay_feed_t::build_supply_lines(
  events::cargo_file_t const & cargo, events::ship_locker_t const & locker, events::backpack_t const & backpack
) const -> std::vector<overlay::line_t>
  {
  if(needs_.empty())
    return {};

  auto const & key{supply_key};

  std::map<std::string, uint32_t> aboard;
  for(events::cargo_item_t const & item: cargo.Inventory)
    {
    auto const internal{key(item.Name)};
    aboard[internal] += item.Count;

    // both names can come down to the same key, and then the quantity would be counted twice
    if(auto const localised{key(item.Name_Localised)}; not localised.empty() and localised != internal)
      aboard[localised] += item.Count;
    }

  // what a mission on foot hands out or asks for - a virus to upload, data off a terminal - is never in the
  // hold but in the locker aboard or in the backpack, two inventories that never list the same thing twice
  auto const count_on_foot{
    [&](events::locker_item_t const & item, std::string_view)
    {
      auto const internal{key(item.Name)};
      aboard[internal] += item.Count;
      if(auto const localised{key(item.Name_Localised)}; not localised.empty() and localised != internal)
        aboard[localised] += item.Count;
    }
  };
  events::for_each_item(locker, count_on_foot);
  events::for_each_item(backpack, count_on_foot);

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
    // a micro resource (Data downloaded off a terminal, Goods or Assets picked up or stolen) is never
    // bought at a station's market - flagging it as having no source is not news, since it could
    // never have had one
    bool const known{
      std::ranges::any_of(
        options_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
      )
      or micro_resource_commodities_.contains(key(need.commodity))
    };

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
auto describe_carrier(info::carrier_state_t const & c, std::chrono::sys_seconds now, std::string const & away)
  -> overlay::line_t
  {
  std::string const who{
    c.name.empty() ? std::format("carrier {}", c.carrier_id)
                   : (c.callsign.empty() ? c.name : std::format("{} ({})", c.name, c.callsign))
  };
  if(not c.jumping)
    return overlay::line_t{
      .text = std::format("  {}: {}{}", who, c.system.empty() ? "?" : c.system, away), .color = colour_plain()
    };

  auto const minutes = [](auto d) { return std::chrono::duration_cast<std::chrono::minutes>(d).count(); };
  // five minutes after the arrival the next jump can be ordered; before it the arrival is reckoned
  auto const ready{c.arrival + carrier_cooldown};
  if(now < c.departure)
    return overlay::line_t{
      .text = std::format(
        "  {}: {} -> {}{}, leaves {:%H:%M} UTC (in {} min), ready ~{:%H:%M}",
        who,
        c.from.empty() ? "?" : c.from,
        c.to_body.empty() ? c.to : c.to_body,
        away,
        c.departure,
        minutes(c.departure - now) + 1,
        ready
      ),
      .color = colour_first()
    };
  return overlay::line_t{
    .text = std::format(
      "  {}: {} {} -> {}{} at {:%H:%M} UTC, ready {:%H:%M} (in {} min)",
      who,
      c.arrived ? "arrived" : "jumping",
      c.from.empty() ? "?" : c.from,
      c.to_body.empty() ? c.to : c.to_body,
      away,
      c.arrived ? c.arrival : c.departure,
      ready,
      minutes(ready - now) + 1
    ),
    .color = colour_first()
  };
  }
  }  // namespace

auto overlay_feed_t::build_logistics_lines(std::array<double, 3> const & here) const -> std::vector<overlay::line_t>
  {
  auto & db{const_cast<database_storage_t &>(db_)};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;

  // The last port decides where an escape pod sends you - settlements and carriers do not count, and
  // after a few stops it is easy to lose track of which of them was a port
  if(auto port{db.load_last_port()}; port and *port)
    {
    std::string away;
    bool const here_known{here[0] != 0.0 or here[1] != 0.0 or here[2] != 0.0};
    if(here_known)
      if(auto positions{db.load_system_positions(std::vector{(*port)->system})};
         positions and not positions->empty())
        {
        double const dx{positions->begin()->second[0] - here[0]};
        double const dy{positions->begin()->second[1] - here[1]};
        double const dz{positions->begin()->second[2] - here[2]};
        double const ly{std::sqrt(dx * dx + dy * dy + dz * dz)};
        away = ly < 0.05 ? std::string{", here"} : std::format(", {:.1f} ly", ly);
        }
    lines.push_back(
      overlay::line_t{
        .text
        = std::format("last port: {}, {} ({}){}", (*port)->name, (*port)->system, (*port)->station_type, away),
        .color = colour_plain()
      }
    );
    }

  // one's own carrier and the squadron's: where each is, or where it goes and when it can jump again
  if(auto carriers{db.load_carrier_states(now, carrier_cooldown)}; carriers and not carriers->empty())
    {
    lines.push_back(overlay::line_t{.text = "carriers:", .color = colour_heading()});
    // how far the carrier is, or will be once its jump is done - out in deep space it is the way home
    bool const here_known{here[0] != 0.0 or here[1] != 0.0 or here[2] != 0.0};
    std::vector<std::string> names;
    for(info::carrier_state_t const & c: *carriers)
      names.push_back(c.jumping ? c.to : c.system);
    auto const positions{here_known ? db.load_system_positions(names) : decltype(db.load_system_positions(names)){}};
    for(size_t ix{}; ix != carriers->size(); ++ix)
      {
      std::string away;
      if(positions)
        if(auto const it{positions->find(names[ix])}; it != positions->end())
          {
          double const dx{it->second[0] - here[0]};
          double const dy{it->second[1] - here[1]};
          double const dz{it->second[2] - here[2]};
          double const ly{std::sqrt(dx * dx + dy * dy + dz * dz)};
          away = ly < 0.05 ? std::string{", here"} : std::format(", {:.1f} ly", ly);
          }
      lines.push_back(describe_carrier((*carriers)[ix], now, away));
      }
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

auto overlay_feed_t::refresh_territory() -> void
  {
  // read only while it is shown - a dozen systems of queries have no business in every frame of a fight
  auto const settings{eht::settings()};
  if(not settings->bgs.on_galaxy_map or settings->bgs.own_factions.empty() or gui_focus_ != galaxy_map_focus)
    return;
  auto const now{std::chrono::steady_clock::now()};
  if(not territory_.empty() and now - territory_read_ < std::chrono::seconds{30})
    return;
  territory_read_ = now;

  auto systems{db_.load_territory(settings->bgs.own_factions)};
  auto wave{db_.newest_influence_wave()};
  if(not systems or not wave)
    {
    spdlog::error("overlay feed: failed to load the territory");
    return;
    }
  territory_ = std::move(*systems);
  territory_wave_ = *wave;
  }

auto overlay_feed_t::build_territory_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  auto const settings{eht::settings()};
  if(not settings->bgs.on_galaxy_map or gui_focus_ != galaxy_map_focus or territory_.empty())
    return {};
  std::vector<std::string> const & own{settings->bgs.own_factions};
  std::string const sector{territory::common_sector(territory_)};

  // nearest first - the map is open to choose where to go, and what is near is what is chosen
  std::array<double, 3> const & location{state.system.system_location};
  std::optional<std::array<double, 3>> const here{
    location[0] != 0.0 or location[1] != 0.0 or location[2] != 0.0 ? std::optional{location} : std::nullopt
  };
  std::vector<std::pair<std::optional<double>, territory::system_t const *>> placed;
  for(territory::system_t const & system: territory_)
    placed.emplace_back(territory::distance(here, system.position), &system);
  std::ranges::stable_sort(
    placed,
    [](auto const & l, auto const & r)
    {
      if(l.first.has_value() != r.first.has_value())
        return l.first.has_value();
      return l.first.value_or(0.0) < r.first.value_or(0.0);
    }
  );

  std::vector<overlay::line_t> lines;
  size_t const read{static_cast<size_t>(std::ranges::count_if(
    territory_,
    [this](territory::system_t const & system)
    { return territory::tick_seen(system, territory_wave_) != territory::tick_seen_e::not_seen; }
  ))};
  lines.push_back(
    overlay::line_t{
      .text = territory_wave_ ? std::format("territory - {} of {} read since the tick", read, territory_.size())
                              : std::string{"territory"},
      .color = colour_heading()
    }
  );

  for(territory::standing_t const & standing: territory::standings(territory_, own))
    {
    std::string text{std::format("{}: {} of {}", standing.faction, standing.controls, standing.present)};
    if(standing.thinnest_lead)
      text += std::format(
        ", thinnest lead {:.1f} in {}", *standing.thinnest_lead, territory::short_name(standing.thinnest_system, sector)
      );
    if(standing.closest_gap)
      text += std::format(
        ", nearest to take {} {:.1f} behind", territory::short_name(standing.closest_system, sector), *standing.closest_gap
      );
    lines.push_back(overlay::line_t{.text = std::move(text), .color = colour_plain()});
    }

  // a column for each faction of one's own, headed by its first word - the lines above name them in full
  std::vector<std::string> labels;
  for(std::string const & name: own)
    {
    std::string label{name.substr(0u, name.find(' '))};
    if(std::ranges::count_if(own, [&label](std::string const & n) { return n.starts_with(label + " ") or n == label; }) > 1)
      label = name.substr(0u, 10u);
    labels.push_back(std::move(label));
    }

  size_t const shown{rows_for(placed.size(), settings->bgs.overlay_systems)};
  size_t name_width{6u};
  for(auto const & [distance, system]: placed | std::views::take(shown))
    name_width = std::max(name_width, std::min<size_t>(territory::short_name(system->name, sector).size(), 24u));
  constexpr size_t faction_width{12u};

  std::string heading{std::format("{:>6}  {:<{}}{:>7}", "ly", "system", name_width, "lead")};
  for(std::string const & label: labels)
    heading += std::format("{:>{}}", label, faction_width);
  lines.push_back(overlay::line_t{.text = std::move(heading), .color = colour_heading()});

  constexpr uint32_t up_colour{0x66dd66u};
  constexpr uint32_t down_colour{0xff6666u};
  for(auto const & [distance, system]: placed | std::views::take(shown))
    {
    overlay::line_t line{.color = colour_plain()};
    std::string name{territory::short_name(system->name, sector)};
    if(name.size() > name_width)
      name.resize(name_width);
    line.text = std::format(
      "{:>6}  {:<{}}", distance ? std::format("{:.1f}", *distance) : std::string{"?"}, name, name_width
    );

    // overtaken is a conflict for control on its way, thin is work to be done before one comes
    std::string lead_text;
    uint32_t lead_colour{colour_plain()};
    if(auto const held{territory::lead(*system)}; held)
      {
      lead_text = std::format("{:.1f}", held->margin);
      lead_colour = held->margin < 0.0                      ? down_colour
                    : held->margin < settings->bgs.thin_lead ? colour_alert()
                                                             : colour_plain();
      }
    line.spans.push_back(overlay::span_t{.from = uint32_t(line.text.size()), .length = 7u, .color = lead_colour});
    line.text += std::format("{:>7}", lead_text);

    for(std::string const & name_of_own: own)
      {
      auto const faction{std::ranges::find(system->factions, name_of_own, &territory::faction_t::name)};
      if(faction == system->factions.end())
        {
        line.text += std::string(faction_width, ' ');
        continue;
        }
      std::string const value{std::format("{}{:.1f}", name_of_own == system->controlling ? "*" : "", faction->influence)};
      std::string const move{faction->moved ? std::format(" {:+.1f}", *faction->moved) : std::string{}};
      std::string const cell{std::format("{:>{}}", value + move, faction_width)};
      if(faction->moved and *faction->moved != 0.0)
        line.spans.push_back(
          overlay::span_t{
            .from = uint32_t(line.text.size() + cell.size() - move.size()),
            .length = uint32_t(move.size()),
            .color = *faction->moved > 0.0 ? up_colour : down_colour
          }
        );
      line.text += cell;
      }

    // the lead is then someone else's, and whose it is says who is to be beaten
    if(not system->controlling.empty() and not std::ranges::contains(own, system->controlling))
      line.text += std::format("  held by {}", system->controlling);

    // the result of the tick still to be seen - that is a reason to fly there on its own
    switch(territory::tick_seen(*system, territory_wave_))
      {
      case territory::tick_seen_e::known: break;
      case territory::tick_seen_e::unchanged: line.text += "  unchanged"; break;
      case territory::tick_seen_e::not_seen:
        {
        std::string const note{
          system->seen ? std::format("  not seen since, {:%d.%m}", *system->seen) : std::string{"  never read"}
        };
        line.spans.push_back(
          overlay::span_t{.from = uint32_t(line.text.size()), .length = uint32_t(note.size()), .color = colour_alert()}
        );
        line.text += note;
        break;
        }
      }
    // the spans must stand in order and not overlap - the lead's comes first, before any of the cells
    std::ranges::sort(line.spans, {}, &overlay::span_t::from);
    lines.push_back(std::move(line));

    for(std::string const & note: territory::notes(*system, own, settings->bgs.retreat_below))
      lines.push_back(overlay::line_t{.text = std::format("{:>8}{}", "", note), .color = colour_alert()});
    }
  if(shown != placed.size())
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", placed.size() - shown), .color = colour_plain()}
    );
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

auto overlay_feed_t::scanner_target(current_state_t const & state) const -> std::string
  {
  if(status_destination_ and status_destination_->System == state.current_system_address_
     and status_destination_->Body != 0u)
    return status_destination_->Name;
  return {};
  }

namespace
  {
///\brief where the approaches are written, in the directory the tool runs in - one file an account
constexpr std::string_view hot_drop_file{"hot_drop.jsonl"};

[[nodiscard]]
auto now_ms() -> uint64_t
  {
  return uint64_t(
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
  );
  }
  }  // namespace

auto overlay_feed_t::hot_drop_port(current_state_t const & state) const -> std::optional<hot_drop::context_t>
  {
  constexpr uint64_t supercruise_flag{1u << 4u};
  constexpr uint64_t taxi_flag{1u << 1u};
  constexpr uint64_t multicrew_flag{1u << 2u};
  if(
    (status_flags_ & supercruise_flag) == 0u or (status_flags2_ & (taxi_flag | multicrew_flag)) != 0u
    or not status_destination_ or status_destination_->System != state.current_system_address_
    or status_destination_->Name.empty()
  )
    return std::nullopt;
  std::string const & name{status_destination_->Name};

  hot_drop::context_t port{
    .system = state.system.name, .system_address = state.current_system_address_, .station = name
  };
  // a port docked at before is known with its type and distance from the star; one never docked at only by its
  // signal - a carrier, mostly. A star, a planet or a settlement on the ground is no port for a hot drop
  info::station_t const * known{};
  for(info::station_t const & station: stations_)
    if(station.name == name)
      known = &station;
  if(known != nullptr)
    {
    if(info::is_ground_settlement(*known))
      return std::nullopt;
    port.market_id = known->market_id;
    port.station_type = known->station_type;
    }
  else
    {
    auto const signal{std::ranges::find_if(
      state.system.system_signals, [&name](system_signal_t const & s) { return s.is_station and s.name == name; }
    )};
    if(signal == state.system.system_signals.end())
      return std::nullopt;
    port.station_type = signal->signal_type;
    }

  // the body it circles, as the system map attaches it: at the same distance from the star, else the star
  if(known != nullptr and known->dist_from_star_ls > 0.0)
    {
    double const ls{known->dist_from_star_ls};
    auto const nearest_of = [&](bool stars_only) -> std::pair<body_t const *, double>
    {
      body_t const * nearest{};
      double gap{std::numeric_limits<double>::max()};
      for(body_t const & body: state.system.bodies)
        if(stars_only and body.body_type() != body_type_e::star)
          continue;
        else if(double const g{std::abs(body.distance_from_arrival_ls - ls)}; g < gap)
          {
          gap = g;
          nearest = &body;
          }
      return {nearest, gap};
    };
    auto [body, gap]{nearest_of(false)};
    if(body == nullptr or gap > std::max(5.0, ls * 0.015))
      std::tie(body, gap) = nearest_of(true);
    if(body != nullptr)
      {
      port.body = body->name;
      port.body_radius_km = body->radius / 1000.0;
      port.port_from_body_ls = gap;
      if(auto const * planet{std::get_if<planet_details_t>(&body->details)}; planet != nullptr)
        {
        port.body_kind = planet->parent_planet ? "moon" : "planet";
        port.body_mass_em = planet->mass_em;
        port.body_gravity_g = planet->surface_gravity / 9.80665;
        }
      else if(auto const * star{std::get_if<star_details_t>(&body->details)}; star != nullptr)
        {
        constexpr double earths_in_sun{332946.0};
        port.body_kind = "star";
        port.body_mass_em = star->stellar_mass * earths_in_sun;
        }
      }
    }

  ship_loadout_t const & ship{state.ship_loadout};
  port.ship = ship.Ship;
  port.ship_id = ship.ShipID;
  port.ship_name = ship.ShipName;
  if(ship.UnladenMass > 0.f)
    port.ship_mass_t = double(ship.UnladenMass) + double(ship.FuelLevel) + double(ship.CargoUsed);
  return port;
  }

auto overlay_feed_t::hot_drop_done(hot_drop::attempt_t const & attempt) -> void
  {
  hot_drop::record_t record{hot_drop::record_of(attempt)};
  spdlog::info(
    "hot drop: {} in {} - {}, overspeed {} from {:.2f} Ls (went in {}x, first at {:.2f} Ls) at least {} s, "
    "{:.1f} Mm/s at the end, {} readings",
    attempt.where.station,
    attempt.where.ship,
    hot_drop::outcome_name(attempt.outcome),
    record.summary.overspeed,
    record.summary.overspeed_from_ls,
    record.summary.overspeed_entries,
    record.summary.first_overspeed_ls,
    record.summary.least_seconds,
    record.summary.end_speed_mm_s,
    attempt.readings.size()
  );
  if(std::ofstream out{std::string{hot_drop_file}, std::ios::app}; out)
    out << hot_drop::attempt_line(attempt) << '\n';
  else
    spdlog::error("hot drop: cannot write {}", hot_drop_file);
  hot_drop_records_.push_back(record);
  hot_drop_last_ = std::move(record);
  hot_drop_last_at_ = std::chrono::steady_clock::now();
  }

auto overlay_feed_t::track_hot_drop(current_state_t const & state) -> void
  {
  auto const & cfg{eht::settings()->hot_drop};
  if(not cfg.record)
    return;
  if(not hud_reader_t::available())
    {
    if(not hot_drop_unreadable_told_)
      spdlog::warn("hot drop: built without tesseract - the HUD cannot be read, nothing is recorded");
    hot_drop_unreadable_told_ = true;
    return;
    }
  if(not hud_reader_)
    hud_reader_ = std::make_unique<hud_reader_t>();
  if(not hot_drop_loaded_)
    {
    hot_drop_loaded_ = true;
    std::ifstream in{std::string{hot_drop_file}};
    for(std::string line; std::getline(in, line);)
      if(auto record{hot_drop::parse_attempt_line(line)}; record)
        hot_drop_records_.push_back(std::move(*record));
    }

  uint64_t const now{now_ms()};
  // the journal's word of a drop at the destination ends the approach first - the status file says only that
  // supercruise is over, and may say it before
  if(state.destination_drops_seen_ != hot_drop_drops_seen_)
    {
    hot_drop_drops_seen_ = state.destination_drops_seen_;
    auto const & drop{state.last_destination_drop_};
    std::string_view const name{drop.Type_Localised.empty() ? drop.Type : drop.Type_Localised};
    if(auto done{hot_drop_.dropped(now, name, drop.MarketID)}; done)
      hot_drop_done(*done);
    }
  for(hud_reader_t::result_t const & read: hud_reader_->take())
    if(read.label and hot_drop_.port() != nullptr and hot_drop_.port()->station == read.target)
      if(auto done{hot_drop_.reading(
           hot_drop::reading_t{
             .ms = read.ms,
             .distance_ls = read.label->distance_ls,
             .seconds = read.label->seconds ? int32_t(*read.label->seconds) : -1
           }
         )};
         done)
        hot_drop_done(*done);
  if(auto done{hot_drop_.approach(now, hot_drop_port(state))}; done)
    hot_drop_done(*done);

  if(not hot_drop_.active())
    return;
  auto const last{hot_drop_.last_distance()};
  uint64_t const every{last and *last > cfg.near_ls ? cfg.far_every_ms : cfg.near_every_ms};
  if(now - hot_drop_asked_ms_ < every)
    return;
  hot_drop_asked_ms_ = now;
  std::filesystem::path const path{
    std::filesystem::path{overlay::default_spool_path()} / std::format("{}_hud_{}.ppm", overlay::socket_stem(), now)
  };
  capture_ = overlay::capture_t{
    .id = now * 2u + 1u, .path = path.string(), .size = cfg.size, .delay_ms = 0u, .quiet = true, .aspect = 16.f / 9.f
  };
  hud_reader_->submit(hud_reader_t::job_t{.path = path, .ms = now, .target = hot_drop_.port()->station});
  }

auto overlay_feed_t::build_hot_drop_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  std::vector<overlay::line_t> lines;
  if(not eht::settings()->hot_drop.record)
    return lines;
  auto const seconds_text = [](int32_t s) { return s < 0 ? std::string{"?"} : std::format("0:{:02}", s); };
  if(hot_drop::context_t const * port{hot_drop_.port()}; port != nullptr and hot_drop_.active())
    {
    hot_drop::advice_t const advice{hot_drop::advise(hot_drop_records_, port->station, port->market_id, port->ship)};
    std::string head{std::format("hot drop {}", port->station)};
    if(not port->body.empty())
      head += std::format(" ({} {}, {:.2f} g)", port->body_kind, port->body, port->body_gravity_g);
    lines.push_back(overlay::line_t{.text = std::move(head), .color = colour_heading()});
    // the furthest that worked, as long as nothing nearer passed the port
    if(advice.dropped_from_ls and (not advice.overshot_from_ls or *advice.dropped_from_ls < *advice.overshot_from_ls))
      lines.push_back(
        overlay::line_t{
          .text = std::format("  overspeed from {:.1f} Ls or nearer", *advice.dropped_from_ls), .color = colour_first()
        }
      );
    if(advice.dropped_from_ls)
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  dropped from {:.1f} Ls at {} ({}x)", *advice.dropped_from_ls, seconds_text(advice.dropped_seconds), advice.dropped
          ),
          .color = colour_plain()
        }
      );
    if(advice.overshot_from_ls)
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  overshot from {:.1f} Ls at {} ({}x)",
            *advice.overshot_from_ls,
            seconds_text(advice.overshot_seconds),
            advice.overshot
          ),
          .color = colour_alert()
        }
      );
    if(not advice.dropped_from_ls and not advice.overshot_from_ls)
      lines.push_back(overlay::line_t{.text = std::format("  no hot drop here in {} yet", port->ship), .color = colour_plain()});
    if(auto const last{hot_drop_.last_distance()}; last)
      lines.push_back(overlay::line_t{.text = std::format("  read: {:.2f} Ls", *last), .color = colour_plain()});
    }
  else if(hot_drop_last_ and std::chrono::steady_clock::now() - hot_drop_last_at_ < std::chrono::seconds{30})
    {
    hot_drop::summary_t const & s{hot_drop_last_->summary};
    std::string text{std::format("hot drop {}: {}", hot_drop_last_->station, hot_drop::outcome_name(hot_drop_last_->outcome))};
    if(s.overspeed)
      {
      text += std::format(", overspeed from {:.1f} Ls at {}", s.overspeed_from_ls, seconds_text(s.least_seconds));
      if(s.overspeed_entries > 1u)
        text += std::format(" (went in {}x, first at {:.1f} Ls)", s.overspeed_entries, s.first_overspeed_ls);
      }
    else
      text += ", no overspeed";
    lines.push_back(
      overlay::line_t{
        .text = std::move(text),
        .color = hot_drop_last_->outcome == hot_drop::outcome_e::overshot ? colour_alert() : colour_plain()
      }
    );
    }
  (void)state;
  return lines;
  }

auto overlay_feed_t::publish_edworld_target(current_state_t const & state) -> void
  {
  std::string const & dir{eht::settings()->edworld.dir};
  if(dir.empty())
    return;
  if(dir != edworld_dir_)
    {
    if(edworld_target_)
      ::munmap(edworld_target_, sizeof(edworld::target_t));
    edworld_target_ = nullptr;
    edworld_dir_ = dir;
    edworld_system_ = 0u;
    edworld_failed_ = false;
    }
  if(not edworld_target_)
    {
    if(edworld_failed_)
      return;
    // a directory of tmpfs: the proxy under Wine opens the same file as Z:\dev\shm\..., and the mapping
    // it makes of it is this one's pages
    ::mkdir(dir.c_str(), 0755);
    std::string const path{dir + "/target"};
    int const fd{::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644)};
    void * view{MAP_FAILED};
    if(fd >= 0 and ::ftruncate(fd, sizeof(edworld::target_t)) == 0)
      view = ::mmap(nullptr, sizeof(edworld::target_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    int const err{errno};
    if(fd >= 0)
      ::close(fd);
    if(view == MAP_FAILED)
      {
      edworld_failed_ = true;
      spdlog::warn("edworld: {} could not be mapped ({}), the proxy will ask EDSM instead", path, std::strerror(err));
      return;
      }
    edworld_target_ = static_cast<edworld::target_t *>(view);
    spdlog::info("edworld: telling the proxy the jump destination in {}", path);
    }

  uint64_t const target{state.next_target.SystemAddress};
  if(target == 0u or target == edworld_system_)
    return;
  edworld_system_ = target;
  bool known{};
  std::string allegiance;
  std::string controlling;
  if(auto system{db_.load_system(target)}; system and *system)
    {
    known = true;
    allegiance = (*system)->allegiance;
    controlling = (*system)->controlling_faction;
    }
  // the list under the panel is edworld's to draw: the same view the overlay keeps of the destination
  std::vector<overlay::world::faction_entry_t> factions;
  bool factions_known{};
  if(known)
    {
    refresh_factions(state, target, controlling, jump_);
    factions_known = not jump_.factions.empty();
    for(listed_faction_t const & faction: jump_.factions)
      factions.push_back(
        overlay::world::faction_entry_t{
          .name = faction.name,
          .states = faction.states,
          .influence = faction.influence,
          .allegiance = faction.allegiance,
          .trend = static_cast<uint8_t>(faction.trend),
          .controlling = faction.controlling
        }
      );
    }
  auto const now_ms{
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
  };
  overlay::world::write_target(
    *edworld_target_,
    overlay::world::make_target(target, known, allegiance, state.next_target.Name, now_ms, factions_known, factions)
  );
  spdlog::info(
    "edworld: destination {} told: {}, {} faction(s){}",
    state.next_target.Name,
    known ? "known" : "not known",
    factions.size(),
    known and not factions_known ? " (no influence readings, edworld asks EDSM)" : ""
  );
  }

auto overlay_feed_t::publish(current_state_t const & state, plotted_route_t const & plotted) -> void
  {
  codex_.set_journal_dir(state.journal_dir_path_);
  sky_.set_journal_dir(state.journal_dir_path_);
  sky_.open();
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
  scanner_.observe(scanner_on_planet, scanner_target(state));

  // Out of the jump the ship faces the arrival star; after the scanner it still faces the planet. Both are
  // taken then, quietly, once per body - in the ship's own view in supercruise, nothing else open
  {
  auto const cfg{eht::settings()};
  bool const ship_view{
    gui_focus_ == 0u and (status_flags_ & supercruise_flag) != 0u and (status_flags_ & main_ship_flag) != 0u
    and not state.in_witchspace_
  };
  sky_.collect();
  if(state.current_system_address_ != 0u and state.current_system_address_ != sky_system_ and not state.in_witchspace_)
    {
    sky_system_ = state.current_system_address_;
    // the system the tool started in was not jumped into just now - the ship may face anything
    if(bool const started{std::exchange(sky_started_, true)}; started and state.system.population == 0u)
      {
      sky_arrival_ = std::chrono::steady_clock::now();
      std::vector<std::chrono::milliseconds> delays;
      for(uint32_t const delay: cfg->exploration.sky_star_delays_ms)
        delays.emplace_back(delay);
      sky_.set_moment(state.jump_info.timestamp);
      spdlog::info("sky: arrival at {}, ship_view {}, star photo asked", state.system.name, ship_view);
      sky_.ask_series(
        sky_album_t::entry_t{.kind = "star", .system = state.system.name, .body = state.system.name}, delays
      );
      }
    else if(started)
      spdlog::info("sky: arrival at {}, population {}, star photo skipped", state.system.name, state.system.population);
    }
  if(sky_arrival_)
    {
    // the star's own scan comes some seconds after the jump, later than the first pictures - they are taken
    // under the system's name and told what they show once it is in
    body_t const * star{};
    for(body_t const & body: state.system.bodies)
      if(body.body_type() == body_type_e::star and (star == nullptr or body.distance_from_arrival_ls < star->distance_from_arrival_ls))
        star = &body;
    if(star != nullptr)
      {
      auto const & details{std::get<star_details_t>(star->details)};
      sky_.describe(
        state.system.name,
        sky_album_t::entry_t{
          .kind = "star",
          .system = state.system.name,
          .body = star->name.empty() ? state.system.name : std::format("{} {}", state.system.name, star->name),
          .detail = pictures::star_detail(
            details.star_type,
            details.sub_class,
            details.luminosity,
            details.stellar_mass,
            details.surface_temperature,
            star->radius
          ),
          .first = not star->was_discovered
        }
      );
      sky_arrival_.reset();
      }
    else if(std::chrono::steady_clock::now() - *sky_arrival_ > std::chrono::seconds{30})
      sky_arrival_.reset();
    }
  // a planet of this system by its full name, described for the album - none for a star or a body not scanned
  auto const planet_entry = [&](std::string const & name) -> std::optional<sky_album_t::entry_t>
  {
    for(body_t const & body: state.system.bodies)
      if(name == body.name or name == std::format("{} {}", state.system.name, body.name))
        if(auto const * const planet{std::get_if<planet_details_t>(&body.details)}; planet != nullptr)
          return sky_album_t::entry_t{
            .kind = "planet",
            .system = state.system.name,
            .body = name,
            .detail = pictures::planet_detail(
              planet->planet_class, planet->atmosphere, planet->surface_gravity, planet->surface_temperature
            ),
            .first = not body.was_discovered
          };
    return std::nullopt;
  };

  // Standing before a planet in analysis mode, the one set as the destination, the view is taken now and
  // then and kept aside; the scanner opening holds the last one, and the mapping puts it into the album -
  // the view just before the scanner, with the planet in front and nothing of the scanner's blue on it
  constexpr uint64_t analysis_flag{1u << 27u};
  std::string const destination{
    status_destination_ and status_destination_->System == state.current_system_address_ and status_destination_->Body != 0u
      ? status_destination_->Name
      : std::string{}
  };
  if(ship_view and (status_flags_ & analysis_flag) != 0u and not destination.empty())
    if(auto entry{planet_entry(destination)}; entry)
      sky_.offer(std::move(*entry));
  // flying at a planet set as the destination, in any mode, the view is judged for the planet's face
  approach_.observe(ship_view, state.system.name, planet_entry(destination) ? destination : std::string{});
  approach_.tick(faces_);
  if(sky_focus_ != 10u and gui_focus_ == 10u)
    {
    spdlog::info("sky: the scanner opened, destination {}", destination.empty() ? std::string{"none"} : destination);
    sky_.hold(destination);
    }
  if(state.scanner_at_ != sky_scanner_at_)
    {
    sky_scanner_at_ = state.scanner_at_;
    if(sky_started_ and not state.scanner_body_.empty() and sky_.keep_held(state.scanner_body_, state.scanner_at_))
      spdlog::info("sky: {} mapped, the view before the scanner kept", state.scanner_body_);
    }
  // no view held - the scanner opened on another body, or the picture came late: the view after it, then
  if(sky_focus_ == 10u and gui_focus_ != 10u and (status_flags_ & supercruise_flag) != 0u and not state.scanner_body_.empty())
    {
    spdlog::info("sky: the scanner closed on {}", state.scanner_body_);
    if(auto entry{planet_entry(state.scanner_body_)}; entry)
      {
      sky_.set_moment(state.scanner_at_);
      sky_.ask(std::move(*entry), std::chrono::milliseconds{cfg->exploration.sky_planet_delay_ms});
      }
    }
  sky_focus_ = gui_focus_;
  sky_.tick(ship_view);
  }

  // On foot inside a settlement's buildings, with nothing open - where the white glare of some rooms comes
  {
  constexpr uint64_t on_foot_flag{1u << 0u};
  constexpr uint64_t taxi_flag{1u << 1u};
  constexpr uint64_t on_planet_flag{1u << 4u};
  constexpr uint64_t exterior_flag{1u << 15u};
  bool const inside_settlement{
    (status_flags2_ & on_foot_flag) != 0u and (status_flags2_ & on_planet_flag) != 0u
    and (status_flags2_ & (exterior_flag | taxi_flag)) == 0u and gui_focus_ == 0u
  };
  netstate_.set_journal_dir(state.journal_dir_path_);
  if(auto const now{std::chrono::steady_clock::now()}; now - netlog_looked_ > std::chrono::minutes{1})
    {
    netlog_looked_ = now;
    auto const cfg{eht::settings()};
    std::error_code ec;
    std::filesystem::path cwd;
    if(auto const pid{netstate_.game()}; pid)
      cwd = std::filesystem::read_symlink(std::format("/proc/{}/cwd", *pid), ec);
    netlog_dir_ = not cfg->evidence.netlog_dir.empty() ? backup::expand_home(cfg->evidence.netlog_dir)
                                                       : evidence::find_netlog_dir(state.journal_dir_path_, cwd);
    }
  glare_.collect(
    glare::game_t{
      .system = state.system.name,
      .body = status_body_,
      .settlement = station_name_,
      .journal_ts = std::format("{:%FT%T}Z", state.last_event_)
    },
    glare_watch_t::places_t{.journal_dir = state.journal_dir_path_, .netlog_dir = netlog_dir_}
  );
  glare_.observe(inside_settlement);
  }
  screenshots_.collect();
  refresh_species_history(state);

  if(not server_->listening())
    return;

  refresh_factions(state, state.current_system_address_, state.system.controlling_faction, here_);
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
  refresh_neutron_route(state);

  overlay::frame_t frame{};

  if(auto neutron{build_neutron_checklist_lines(state, plotted)}; not neutron.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_left, .ttl_ms = block_ttl_ms(), .lines = std::move(neutron)}
    );

  // a ship actually docked, not a taxi ride nor an on-foot arrival, at a market never opened - the
  // side band already says so, easy to miss among everything else there; this repeats it where it
  // cannot be, until the commodities screen is opened or the ship leaves
  {
  constexpr uint64_t docked_flag{1u << 0u};
  constexpr uint64_t on_foot_flag{1u << 0u};
  constexpr uint64_t taxi_flag{1u << 1u};
  eht::market_reminder_t const & reminder{eht::settings()->overlay.market_reminder};
  if(
    reminder.enabled and market_unknown_ and (status_flags_ & docked_flag) != 0u
    and (status_flags2_ & (on_foot_flag | taxi_flag)) == 0u
  )
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_left,
        .ttl_ms = block_ttl_ms(),
        .lines = {overlay::line_t{.text = "Open the Commodities Market to record it", .color = colour_alert()}},
        .text = overlay::text_e::normal,
        .middle = true,
        .middle_y = reminder.y,
        .middle_width = reminder.width
      }
    );
  }

  if(not state.system.name.empty())
    {
    auto lines{describe_system(state.system, here_.lines.empty())};
    lines.insert(lines.end(), here_.lines.begin(), here_.lines.end());
    lines.insert(lines.end(), here_.conflicts.begin(), here_.conflicts.end());
    frame.blocks.push_back(
      overlay::block_t{
        // the chart goes under the text, because the names and the current values are what one
        // reads at a glance; the shape of the last ten days is what one studies when there is time
        .corner = overlay::corner_e::top_left,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(lines),
        .charts = here_.charts
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
  if(auto phenomena{overlay_exploration::describe_phenomena(state.system)}; not phenomena.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms(), .lines = std::move(phenomena)}
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
  if(auto map{build_system_diagram(state.system, stations_, status_body_, status_destination_, state.active_missions, faces_)}; map)
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

  // the way to a point on the ground is read while flying or walking towards it, so it flanks the centre
  if(auto way{interface_open ? std::vector<overlay::line_t>{} : build_surface_nav_lines()}; not way.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::centre_top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(way)}
    );

  // beside the enemies: read in supercruise, eyes on the port ahead
  if(auto hot{interface_open ? std::vector<overlay::line_t>{} : build_hot_drop_lines(state)}; not hot.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::centre_top_left, .ttl_ms = block_ttl_ms(), .lines = std::move(hot)}
    );
  if(auto crew{interface_open ? std::vector<overlay::line_t>{} : build_crew_lines(state)}; not crew.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::centre_top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(crew)}
    );

  // the word that the glare was seen and written down - the player's proof that the watch works, in red, for
  // two minutes; no flash, the room is bright enough already
  if(auto const & noticed{glare_.noticed()};
     noticed and std::chrono::steady_clock::now() - noticed->at < std::chrono::minutes{2})
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_right,
        .ttl_ms = block_ttl_ms(),
        .lines = {overlay::line_t{
          .text = std::format("lighting defect detected {}, evidence kept", noticed->local_time),
          .color = colour_expiring()
        }}
      }
    );

  if(auto settlement{build_settlement_lines(state)}; not settlement.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(settlement)}
    );

  if(auto supply{build_supply_lines(state.cargo, state.ship_locker, state.backpack)}; not supply.empty())
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

  if(auto logistics{build_logistics_lines(state.system.system_location)}; not logistics.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms(), .lines = std::move(logistics)}
    );

  // the territory while the galaxy map is open - where the next trip is chosen
  refresh_territory();
  if(auto territory{build_territory_lines(state)}; not territory.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::top_right,
        .ttl_ms = block_ttl_ms(),
        .lines = std::move(territory),
        .charts = {},
        // a table read when choosing, not glanced at in flight
        .text = overlay::text_e::small
      }
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

  // at the station services of a place with Universal Cartographics, with cartography unsold - the moment of
  // the sale. The game names only the sum of a sale, so one system a sale is the only way to learn what each
  // was worth
  constexpr uint32_t station_services_focus{5u};
  if(at_risk_.cartography != 0u and state.cartographics_here_ and gui_focus_ == station_services_focus)
    {
    std::vector<overlay::line_t> lines{overlay::line_t{
      .text = std::format(
        "cartography ~{} unsold - sell it system by system", overlay_exploration::short_credits(at_risk_.cartography)
      ),
      .color = colour_alert()
    }};
    if(station_type_ == "FleetCarrier")
      lines.push_back(overlay::line_t{.text = "a fleet carrier keeps a quarter of it", .color = colour_plain()});
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_left, .ttl_ms = block_ttl_ms(), .lines = std::move(lines)}
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
    // the game writes it only at login, and decays it quietly afterwards with no event of its own, so
    // this number is what it was at login, not necessarily what it is now
    if(legal_.notoriety != 0u)
      text += std::format("   notoriety {} at login {:%H:%M}, maybe lower by now", legal_.notoriety, legal_.notoriety_at);
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

  if(auto owners{build_settlement_owners(state.system)}; not owners.empty())
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
  // the whole-screen picture for eht_vision asks before all, so any other picture takes the turn from it
  if(auto const & vision{eht::settings()->vision}; vision.record)
    {
    uint64_t const now_ms{uint64_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    )};
    auto const reason{shot_clock_.tick(
      vision::shot_state_t{.flags = status_flags_, .flags2 = status_flags2_, .gui_focus = gui_focus_},
      now_ms,
      vision::shot_clock_t::timing_t{
        .every_ms = uint64_t{vision.shot_every_s} * 1000u,
        .after_change_ms = vision.shot_after_change_ms,
        .min_gap_ms = uint64_t{vision.shot_min_gap_s} * 1000u
      }
    )};
    if(reason)
      {
      // a picture of the whole middle screen is some 25 MB - one left lying means no recorder is running
      std::error_code ec;
      if(not shot_path_.empty() and std::filesystem::remove(shot_path_, ec) and not shot_untaken_told_)
        {
        shot_untaken_told_ = true;
        spdlog::warn("vision: the whole-screen picture was not taken by eht_vision - is the recorder running?");
        }
      shot_path_ = std::filesystem::path{overlay::default_spool_path()}
                   / std::format("{}{}_{}.ppm", overlay::vision_shot_prefix(), now_ms, vision::reason_name(*reason));
      capture_ = overlay::capture_t{
        .id = now_ms * 2u, .path = shot_path_.string(), .size = 1.f, .delay_ms = 0u, .quiet = true, .aspect = 16.f / 9.f
      };
      }
    }
  // the label of the port flown to, read off the screen while the approach lasts - it takes the turn from the
  // vision's picture, and the views of planets take it from the label
  track_hot_drop(state);
  // the views for the faces ask first, so any other picture asked for at the same moment takes the turn
  if(auto const & asked{approach_.capture_request()}; asked.id != approach_capture_id_)
    {
    approach_capture_id_ = asked.id;
    capture_ = asked;
    }
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
  if(auto const & asked{sky_.capture_request()}; asked.id != sky_capture_id_)
    {
    sky_capture_id_ = asked.id;
    capture_ = asked;
    }
  if(auto const & asked{glare_.capture_request()}; asked.id != glare_capture_id_)
    {
    glare_capture_id_ = asked.id;
    capture_ = asked;
    }
  frame.capture = capture_;
  frame.sample = approach_.sample_request();
  frame.screenshot.key = eht::settings()->screenshots.key;

  publish_edworld_target(state);

  // The panel of a jump being charged stands in the middle of the screen while the hyperdrive charges - a
  // state of Flags2 alone: StartJump is written only when the charge is done and the countdown begins,
  // and by the charging the target is the system chosen, as FSDTarget named it. In the tunnel the flag
  // may still stand while the game already names the next system of the route, and there is no panel
  constexpr uint64_t hyperdrive_charging_flag{1u << 19u};
  constexpr uint64_t fsd_jump_flag{1u << 30u};
  uint64_t const target{state.next_target.SystemAddress};
  if(
    (status_flags2_ & hyperdrive_charging_flag) != 0u and (status_flags_ & fsd_jump_flag) == 0u
    and not state.in_witchspace_ and target != 0u and target != state.current_system_address_
  )
    {
    if(target != jump_system_)
      {
      jump_system_ = target;
      jump_controlling_.clear();
      // a system we have never been to is not in the database, and then there are no factions to list
      if(auto known{db_.load_system(target)}; known and *known)
        jump_controlling_ = (*known)->controlling_faction;
      }

    eht::jump_emblem_t const & place{eht::settings()->overlay.jump_emblem};
    if(place.factions)
      {
      refresh_factions(state, target, jump_controlling_, jump_);
      // the tick's hour among the lines says nothing about the system - it stays in the band
      std::vector<overlay::line_t> lines;
      for(overlay::line_t const & line: jump_.lines)
        if(line.marker != overlay::marker_e::none)
          {
          // the markers tie the lines to the chart's series, and there is no chart under the panel
          lines.push_back(line);
          lines.back().marker = overlay::marker_e::none;
          lines.back().emblem_column = true;
          }
      if(not lines.empty())
        frame.blocks.push_back(
          overlay::block_t{
            .corner = overlay::corner_e::top_left,
            .ttl_ms = block_ttl_ms(),
            .lines = std::move(lines),
            // the middle of the screen is the game's, so the list takes as little of it as it can
            .text = overlay::text_e::small,
            .middle = true,
            .middle_y = place.factions_y,
            .middle_width = place.factions_width
          }
        );
      }
    }

  // In the scanner the band says what the last picture did: a new view is the sign that the filter is in
  // and the next one may be chosen
  if(std::string const target{gui_focus_ == 10u ? scanner_target(state) : std::string{}};
     not target.empty() and eht::settings()->exploration.scanner_pictures)
    {
    std::vector<overlay::line_t> lines{overlay::line_t{
      .text = std::format(
        "scanner views of {}: {} kept", short_body_name(state.system.name, target), scanner_.view_count(target)
      ),
      .color = colour_heading()
    }};
    if(auto const & news{scanner_.news()}; news and news->body == target)
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

  // The game's graphics set the place wants, while its file holds the other. It is in the frame before the
  // check for a change, so it comes at once, and it goes first in the right band below
  bool const graphics_shown{[&]
                            {
                              auto line{build_graphics_line(state)};
                              if(line)
                                frame.blocks.insert(
                                  frame.blocks.begin(),
                                  overlay::block_t{
                                    .corner = overlay::corner_e::top_right,
                                    .ttl_ms = block_ttl_ms(),
                                    .lines = {std::move(*line)}
                                  }
                                );
                              return line.has_value();
                            }()};

  // the game gets a frame when it has changed or when the keep-alive time has passed - not on every journal event
  auto const now{std::chrono::steady_clock::now()};
  if(same_content(frame, last_) and now - last_sent_ < heartbeat())
    return;

  frame.seq = ++sequence_;
  size_t const blocks_before{frame.blocks.size()};
  // The link to Frontier's servers goes under the temperatures, which are put before it next - the first
  // place looked at when the game seems to hang
  if(auto lines{build_server_link_lines(state)}; not lines.empty())
    frame.blocks.insert(
      frame.blocks.begin(),
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms(), .lines = std::move(lines)}
    );
  // The temperatures come first in the right band, straight under the layer's own frame rate - the
  // number goes warm, then red, near the driver's critical level
  if(auto const reading{sensors_.latest()}; reading and (reading->gpu or reading->cpu))
    {
    auto const cfg{eht::settings()};
    overlay::line_t line{.color = colour_plain()};
    auto const add = [&](std::string_view name,
                         std::optional<sensors::reading_t> const & value,
                         double fallback_critical,
                         sensors::level_e & level)
    {
      if(not line.text.empty())
        line.text += "   ";
      if(not value)
        {
        line.text += std::format("{} -", name);
        return;
        }
      level = sensors::level_of(
        value->celsius, value->critical.value_or(fallback_critical), cfg->sensors.warn_margin, cfg->sensors.hysteresis, level
      );
      std::string const part{std::format("{} {:.0f}\u00b0C", name, value->celsius)};
      if(level != sensors::level_e::normal)
        line.spans.push_back(
          overlay::span_t{
            .from = uint32_t(line.text.size()),
            .length = uint32_t(part.size()),
            .color = level == sensors::level_e::critical ? colour_expiring() : colour_alert()
          }
        );
      line.text += part;
    };
    add("GPU", reading->gpu, cfg->sensors.gpu_critical, gpu_level_);
    add("CPU", reading->cpu, cfg->sensors.cpu_critical, cpu_level_);
    frame.blocks.insert(
      frame.blocks.begin(),
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms(), .lines = {std::move(line)}}
    );
    }
  // the graphics set first of all, straight under the layer's own lines - the blocks put before it since
  // move down one
  if(graphics_shown)
    {
    auto const graphics{frame.blocks.begin() + std::ptrdiff_t(frame.blocks.size() - blocks_before)};
    std::rotate(frame.blocks.begin(), graphics, std::next(graphics));
    }

  server_->publish(frame);
  last_ = std::move(frame);
  last_sent_ = now;
  }

auto overlay_feed_t::log_traffic() -> void
  {
  auto const cfg{eht::settings()};
  auto const steady{std::chrono::steady_clock::now()};
  if(cfg->evidence.dir.empty() or cfg->evidence.traffic_interval_s == 0u
     or (traffic_counters_ and steady - traffic_at_ < std::chrono::seconds{cfg->evidence.traffic_interval_s}))
    return;

  auto const read = [](char const * path)
  {
    std::ifstream file{path};
    return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  };
  auto const counters{netstate::parse_snmp(read("/proc/net/snmp"))};
  auto const bytes{netstate::parse_net_dev(read("/proc/net/dev"))};
  if(not counters or not bytes)
    return;
  // the first reading only starts the count
  if(traffic_counters_ and traffic_bytes_)
    {
    instance_players::summary_t const counted{instance_players::summary(players_)};
    netstate::traffic_t const traffic{
      .seconds = std::chrono::duration<double>(steady - traffic_at_).count(),
      .delta = netstate::delta(*traffic_counters_, *counters),
      .bytes = {
        .received = bytes->received >= traffic_bytes_->received ? bytes->received - traffic_bytes_->received : 0u,
        .sent = bytes->sent >= traffic_bytes_->sent ? bytes->sent - traffic_bytes_->sent : 0u
      },
      .in_session = status_flags_ != 0u or status_flags2_ != 0u,
      .players = counted.players,
      .machines = players_.machines,
      .act1 = players_.act1,
      .act2 = players_.act2
    };
    auto const at{std::chrono::system_clock::now()};
    evidence::append(
      backup::expand_home(cfg->evidence.dir),
      "traffic",
      at,
      netstate::traffic_line(at, *server_game_, traffic),
      cfg->evidence.traffic_keep_days
    );
    }
  traffic_counters_ = counters;
  traffic_bytes_ = bytes;
  traffic_at_ = steady;
  }

auto overlay_feed_t::build_server_link_lines(current_state_t const & state) -> std::vector<overlay::line_t>
  {
  auto const now{std::chrono::system_clock::now()};
  server_tail_.read(
    netlog_dir_,
    [this](server_link::time_point_t at, std::string_view line)
    {
      server_link::feed(server_state_, at, line);
      // a new file is a new run of the game - nobody of the old one is linked any more
      if(server_tail_.file() != players_file_)
        {
        players_ = {};
        players_file_ = server_tail_.file();
        }
      instance_players::feed(players_, at, line);
    }
  );

  // a silence means something only while a session runs: the game's process there, and Status.json saying
  // more than the main menu's nothing
  if(server_game_ and not std::filesystem::exists(std::format("/proc/{}", *server_game_)))
    server_game_.reset();
  if(auto const steady{std::chrono::steady_clock::now()};
     not server_game_ and steady - server_game_looked_ > std::chrono::seconds{10})
    {
    server_game_looked_ = steady;
    server_game_ = netstate::find_game(state.journal_dir_path_);
    }
  std::optional<std::chrono::milliseconds> silence;
  if(server_game_ and (status_flags_ != 0u or status_flags2_ != 0u))
    {
    std::ifstream snmp{"/proc/net/snmp"};
    std::string const text{std::istreambuf_iterator<char>{snmp}, std::istreambuf_iterator<char>{}};
    if(auto const counters{netstate::parse_snmp(text)}; counters)
      {
      server_link::feed(udp_silence_, now, counters->udp_in);
      silence = server_link::silent_for(
        udp_silence_, now, std::chrono::milliseconds{eht::settings()->overlay.server_silence_ms}
      );
      }
    }
  else
    udp_silence_ = {};

  std::vector<server_link::warning_t> const shown{server_link::warnings(server_state_, silence, now)};
  // each new thing once into the log - the seconds counted on the screen left out
  std::vector<std::string> told;
  for(server_link::warning_t const & warning: shown)
    {
    std::string key{warning.text.substr(0, warning.text.find_first_of("0123456789"))};
    if(not std::ranges::contains(server_told_, key))
      spdlog::warn("server link: {}", warning.text);
    told.push_back(std::move(key));
    }
  server_told_ = std::move(told);

  std::vector<overlay::line_t> lines;
  for(server_link::warning_t const & warning: shown)
    lines.push_back(
      overlay::line_t{
        .text = warning.text,
        .color = warning.level == server_link::level_e::failing ? colour_expiring() : colour_alert()
      }
    );

  if(server_game_)
    log_traffic();
  else
    traffic_counters_.reset();

  // the other players come under the trouble - only while the game runs, its last file may end mid-session
  if(eht::settings()->overlay.players_in_instance and server_game_)
    {
    if(uint32_t const players{instance_players::summary(players_).players}; players != players_told_)
      {
      spdlog::info("instance: {} other player{}", players, players == 1u ? "" : "s");
      players_told_ = players;
      }
    if(auto const shown{instance_players::line(players_, now)}; shown)
      lines.push_back(overlay::line_t{.text = shown->text, .color = shown->fresh ? colour_alert() : colour_plain()});
    }
  return lines;
  }

auto overlay_feed_t::build_graphics_line(current_state_t const & state) -> std::optional<overlay::line_t>
  {
  auto const cfg{eht::settings()};
  if(not cfg->graphics.remind)
    return std::nullopt;

  // the file is looked at once a second and read again only when the game has written it
  auto const steady{std::chrono::steady_clock::now()};
  if(steady - graphics_looked_ >= std::chrono::seconds{1})
    {
    graphics_looked_ = steady;
    std::filesystem::path const dir{
      not cfg->graphics.dir.empty() ? backup::expand_home(cfg->graphics.dir)
                                    : graphics_profile::graphics_dir_of(state.journal_dir_path_)
    };
    std::error_code ec;
    std::filesystem::path const file{graphics_profile::newest_file(dir)};
    auto const written{std::filesystem::last_write_time(file, ec)};
    if(ec)
      graphics_setting_.reset();
    else if(file != graphics_file_ or written != graphics_written_)
      {
      graphics_file_ = file;
      graphics_written_ = written;
      std::ifstream in{file};
      std::string const text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
      graphics_setting_ = graphics_profile::parse(text);
      if(graphics_setting_)
        spdlog::info(
          "graphics: {} upscaling {} supersampling {:.2f} AA mode {}",
          file.filename().string(),
          graphics_setting_->upscaling,
          graphics_setting_->supersampling,
          graphics_setting_->anti_aliasing
        );
      else
        spdlog::warn("graphics: no upscaling, supersampling or AA mode in {}", file.string());
      }
    }
  if(not graphics_setting_)
    return std::nullopt;

  auto const place{graphics_profile::settle(
    graphics_watch_,
    graphics_profile::place_of(status_flags_, status_flags2_),
    steady,
    std::chrono::milliseconds{cfg->graphics.settle_ms}
  )};
  if(not place)
    return std::nullopt;
  auto text{graphics_profile::hint(*place, *graphics_setting_, cfg->graphics.planet, cfg->graphics.space)};
  if(text.value_or(std::string{}) != graphics_told_)
    {
    graphics_told_ = text.value_or(std::string{});
    spdlog::info("{}", graphics_told_.empty() ? "graphics: the set fits the place" : graphics_told_);
    }
  if(not text)
    return std::nullopt;
  return overlay::line_t{.text = std::move(*text), .color = colour_alert()};
  }
