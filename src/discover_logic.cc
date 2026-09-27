#include <file_io.h>
#include <elite_events.h>
#include <elite_data.h>
#include <sstream>
#include <glaze/glaze.hpp>
#include <simple_enum/glaze_json_enum_name.hpp>
#include <simple_enum/std_format.hpp>
#include <spdlog/spdlog.h>
#include <stralgo/stralgo.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <ranges>
#include <unordered_map>

using spdlog::debug;
using spdlog::error;
// using spdlog::info;
using spdlog::warn;
using namespace std::string_view_literals;

using utc_time_point_t = std::chrono::sys_time<std::chrono::milliseconds>;

using events::body_id_t;

namespace exploration
  {
auto is_high_value_star(std::string_view star_class) noexcept -> bool
  {
  using namespace std::literals;
  // stars of type A, F, G, K have the best "Goldilocks zone"
  return star_class == "A"sv || star_class == "F"sv || star_class == "G"sv || star_class == "K"sv;
  }

auto extract_mass_code(std::string_view name) noexcept -> char
  {
  // procedural names end with the pattern: [Letters]-[Letter] [MassCode][Number]-[Number]
  // look for the last space; the mass code is the first character after it, if it is a letter
  auto const last_space = name.find_last_of(' ');
  if(last_space == std::string_view::npos || last_space + 1 >= name.size())
    return 'a';  // Fallback

  char const code = static_cast<char>(std::tolower(name[last_space + 1]));
  return (code >= 'a' && code <= 'h') ? code : 'a';
  }

auto system_approx_value(std::string_view star_class, std::string_view system_name) noexcept -> planet_value_e
  {
  auto const mass_code = extract_mass_code(system_name);

  // how the approximation goes:
  // code 'd' with an F/G/A star is most often "high" (terraformables)
  // codes 'e' and above are usually very valuable systems (neutron stars, black holes)
  // codes 'a' and 'b' are usually cheap icy planets

  if(mass_code >= 'e')
    return planet_value_e::high;

  if(mass_code == 'd')
    {
    // stars of type A, F, G under code 'd' have the best chance of expensive planets
    if(is_high_value_star(star_class))
      return planet_value_e::high;
    return planet_value_e::medium;
    }

  if(mass_code == 'c')
    return planet_value_e::medium;

  return planet_value_e::low;
  }
  }  // namespace exploration

// Internal helper to unify scan data for position calculation
struct orbital_node_t
  {
  body_id_t body_id;
  double semi_major_axis;
  double eccentricity;
  double orbital_inclination;
  double periapsis;
  double orbital_period;
  double ascending_node;
  double mean_anomaly;
  std::vector<events::parent_t> parents;
  };

[[nodiscard]]
constexpr auto deg_to_rad(double deg) noexcept -> double
  {
  return deg * (std::numbers::pi / 180.0);
  }

[[nodiscard]]
auto solve_kepler(double mean_anomaly_rad, double eccentricity) noexcept -> double
  {
  constexpr int max_iterations = 10;
  constexpr double precision = 1e-9;

  double e_anomaly = mean_anomaly_rad;  // Initial guess
  for(int i = 0; i < max_iterations; ++i)
    {
    double delta = (e_anomaly - eccentricity * std::sin(e_anomaly) - mean_anomaly_rad)
                   / (1.0 - eccentricity * std::cos(e_anomaly));
    e_anomaly -= delta;
    if(std::abs(delta) < precision)
      break;
    }
  return e_anomaly;
  }

using events::body_location_t;

[[nodiscard]]
auto calculate_relative_pos(orbital_node_t const & node, double dt) -> body_location_t
  {
  if(node.orbital_period <= 0.0)
    return {node.body_id, 0.0, 0.0, 0.0};

  double const n = (2.0 * std::numbers::pi) / node.orbital_period;
  double const m = (node.mean_anomaly * std::numbers::pi / 180.0) + (n * dt);
  double const e_anon = solve_kepler(m, node.eccentricity);

  double const x_orb = node.semi_major_axis * (std::cos(e_anon) - node.eccentricity);
  double const y_orb = node.semi_major_axis * (std::sqrt(1.0 - std::pow(node.eccentricity, 2)) * std::sin(e_anon));

  double const i = node.orbital_inclination * std::numbers::pi / 180.0;
  double const w = node.periapsis * std::numbers::pi / 180.0;
  double const lan = node.ascending_node * std::numbers::pi / 180.0;

  double const x = x_orb * (std::cos(lan) * std::cos(w) - std::sin(lan) * std::sin(w) * std::cos(i))
                   - y_orb * (std::cos(lan) * std::sin(w) + std::sin(lan) * std::cos(w) * std::cos(i));
  double const y = x_orb * (std::sin(lan) * std::cos(w) + std::cos(lan) * std::sin(w) * std::cos(i))
                   + y_orb * (std::cos(lan) * std::cos(w) * std::cos(i) - std::sin(lan) * std::sin(w));
  double const z = x_orb * (std::sin(w) * std::sin(i)) + y_orb * (std::cos(w) * std::sin(i));

  return {node.body_id, x, y, z};
  }

[[nodiscard]]
auto order_calculation(std::span<bary_centre_t const> barycentres, std::vector<body_t const *> const & scans)
  -> std::vector<body_location_t>
  {
  std::unordered_map<body_id_t, orbital_node_t> registry;

  // Populate registry with both barycentres and detailed scans
  for(auto const & bc: barycentres)
    registry[bc.body_id] = {
      bc.body_id,
      bc.semi_major_axis,
      bc.eccentricity,
      bc.orbital_inclination,
      bc.periapsis,
      bc.orbital_period,
      bc.ascending_node,
      bc.mean_anomaly,
      {}
    };

  for(body_t const * s: scans)
    {
    auto & reg{registry[s->body_id]};
    reg = {
      s->body_id,
      s->semi_major_axis,
      s->eccentricity,
      s->orbital_inclination,
      s->periapsis,
      s->orbital_period,
      {},  // s->ascending_node,
      {},  // s->mean_anomaly,
      {}
    };
    std::visit(
      [&registry, &reg]<typename T>(T const & det)
      {
        if constexpr(std::same_as<T, planet_details_t>)
          {
          reg.ascending_node = det.ascending_node;
          reg.mean_anomaly = det.mean_anomaly;
          if(det.parent_barycenter)
            registry[*det.parent_barycenter].parents.emplace_back(events::parent_t{.Null = det.parent_barycenter});
          if(det.parent_star)
            registry[*det.parent_star].parents.emplace_back(events::parent_t{.Star = det.parent_star});
          if(det.parent_planet)
            registry[*det.parent_planet].parents.emplace_back(events::parent_t{.Planet = det.parent_planet});
          }
      },
      s->details
    );

    // filling in the hierarchy of barycentres from the scan's path of parents
    // for(size_t i = 0; i + 1 < s->parents.size(); ++i)
    //   {
    //   auto parent_id = s->parents[i].id();
    //   if(registry.contains(parent_id) and registry[parent_id].parents.empty())
    //     {
    //     // since s.Parents[i] is our barycentre, s.Parents[i+1] is its parent
    //     registry[parent_id].parents.push_back(s->parents[i + 1]);
    //     }
    //   }
    }

  // Explicit logic error check: If a barycentre has parents in the log, they should be mapped!
  // Note: Barycentre logs in ED sometimes don't list parents, but they are referenced by bodies.

  std::unordered_map<body_id_t, body_location_t> rel_coords;
  for(auto const & [id, node]: registry)
    rel_coords[id] = calculate_relative_pos(node, 0.0);

  std::vector<body_location_t> absolute_positions;
  absolute_positions.reserve(scans.size());

  for(auto const & s: scans)
    {
    double abs_x = 0.0, abs_y = 0.0, abs_z = 0.0;
    body_id_t current_id = s->body_id;

    // walk up the tree as far as the main star (no parents left)
    while(true)
      {
      if(rel_coords.contains(current_id))
        {
        auto const & rel = rel_coords.at(current_id);
        abs_x += rel.x;
        abs_y += rel.y;
        abs_z += rel.z;
        }

      if(!registry.contains(current_id) || registry.at(current_id).parents.empty())
        break;
      current_id = registry.at(current_id).parents[0].id();
      }
    absolute_positions.push_back({s->body_id, abs_x, abs_y, abs_z});
    }

  // --- TSP Nearest Neighbor (Start from index 0) ---
  if(absolute_positions.empty())
    return {};

  std::vector<body_location_t> path;
  std::vector<bool> visited(absolute_positions.size(), false);
  size_t current_idx = 0;

  path.push_back(absolute_positions[current_idx]);
  visited[current_idx] = true;

  for(size_t i = 1; i < absolute_positions.size(); ++i)
    {
    double min_d2 = std::numeric_limits<double>::max();
    size_t next_idx = current_idx;

    for(size_t j = 0; j < absolute_positions.size(); ++j)
      {
      if(!visited[j])
        {
        double dx = absolute_positions[current_idx].x - absolute_positions[j].x;
        double dy = absolute_positions[current_idx].y - absolute_positions[j].y;
        double dz = absolute_positions[current_idx].z - absolute_positions[j].z;
        double d2 = dx * dx + dy * dy + dz * dz;
        if(d2 < min_d2)
          {
          min_d2 = d2;
          next_idx = j;
          }
        }
      }
    visited[next_idx] = true;
    path.push_back(absolute_positions[next_idx]);
    current_idx = next_idx;
    }

  return path;
  }

// The 2-opt algorithm (swapping edges) improves the route the nearest neighbour algorithm laid out. It pays
// off above all in systems with many bodies, where the greedy approach often produces crossing paths and
// pointless returns.
//
// Here the starting point (index 0) is forced to stay where it is, because it stands for your actual
// position on entering the system.
//
// Route optimisation: the 2-opt algorithm in C++23

[[nodiscard]]
auto calculate_distance(body_location_t const & a, body_location_t const & b) noexcept -> double
  {
  double const dx = a.x - b.x;
  double const dy = a.y - b.y;
  double const dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

[[nodiscard]]
auto order_calculation_2_opt(body_location_t const player_pos, std::span<body_location_t const> targets)
  -> std::vector<body_location_t>
  {
  if(targets.empty())
    return {};

  // 1. start off: build the path beginning at the player (nearest neighbour)
  std::vector<body_location_t> path;
  path.reserve(targets.size() + 1);
  path.push_back(player_pos);

  std::vector<body_location_t> remaining(targets.begin(), targets.end());

  while(!remaining.empty())
    {
    auto const current = path.back();
    auto const closest_it = std::ranges::min_element(
      remaining,
      [&current](auto const & a, auto const & b)
      { return calculate_distance(current, a) < calculate_distance(current, b); }
    );

    path.push_back(*closest_it);
    remaining.erase(closest_it);
    }

  // 2. the 2-opt optimisation (open TSP)
  if(path.size() < 3)
    return path;

  bool improved = true;
  auto const n = path.size();

  while(improved)
    {
    improved = false;
    // i = 1: the player's position at index 0 is held in place
    for(size_t i = 1; i < n - 1; ++i)
      {
      for(size_t j = i + 1; j < n; ++j)
        {
        // the cost as it stands: (i-1 -> i) + (j -> j+1, if there is one)
        double const d_i_prev_i = calculate_distance(path[i - 1], path[i]);
        double const d_j_j_next = (j < n - 1) ? calculate_distance(path[j], path[j + 1]) : 0.0;

        // the cost after the reversal: (i-1 -> j) + (i -> j+1, if there is one)
        double const d_i_prev_j = calculate_distance(path[i - 1], path[j]);
        double const d_i_j_next = (j < n - 1) ? calculate_distance(path[i], path[j + 1]) : 0.0;

        if((d_i_prev_j + d_i_j_next) < (d_i_prev_i + d_j_j_next) - 1e-6)
          {
          std::reverse(
            path.begin() + static_cast<std::ptrdiff_t>(i), path.begin() + static_cast<std::ptrdiff_t>(j) + 1
          );
          improved = true;
          }
        }
      }
    }

  // optional: drop the player's position from the front, when the result is to hold only the targets
  path.erase(path.begin());
  return path;
  }

/**
 * @brief Calculates Euclidean distance between two points in Light Seconds.
 * * The input coordinates are assumed to be in meters (based on SemiMajorAxis).
 * Speed of light constant is taken as 299,792,458 m/s.
 */
[[nodiscard]]
auto distance_ls(body_location_t const a, body_location_t const b) noexcept -> double
  {
  // Constant for the speed of light in m/s

  double const dx = a.x - b.x;
  double const dy = a.y - b.y;
  double const dz = a.z - b.z;

  // Distance in meters
  double const distance_m = std::sqrt(dx * dx + dy * dy + dz * dz);

  // Convert to Light Seconds
  return distance_m / info::light_speed_mps;
  }

auto body_short_name(std::string_view system, std::string_view name) -> std::string_view
  {
  // a port or a named body carries no system prefix - on foot Status.json names the station
  if(not name.starts_with(system))
    return stralgo::trim(name);
  return stralgo::trim(name.substr(system.size()));
  }

auto planet_name_from_ring_name(std::string_view system, std::string_view name) -> std::string_view
  {
  //"18 Camelopardalis AB 3 A Ring" . sub "18 Camelopardalis AB 3""
  std::string_view plane_with_ring_name{body_short_name(system, name)};  // AB 3 A Ring
  return stralgo::trim(stralgo::substr(plane_with_ring_name, 0, plane_with_ring_name.size() - 7));
  }

namespace exploration
  {
[[nodiscard]]
auto calculate_value(
  planet_value_info_t const & info,
  double mass_em,
  bool is_terraformable,
  bool is_first_discoverer,
  bool is_first_mapper,
  bool efficiency_bonus
) -> uint32_t
  {
  // 1. the mass factor (0.3 at least)
  double const q = std::max(0.3, std::pow(mass_em, 0.2));

  // 2. the base value (K)
  double const base_value = info.base_value + (is_terraformable ? info.terraform_bonus : 0.0);
  double const fss_value = base_value * q;

  // 3. what mapping gives (DSS)
  // mapping is the base * 3.333333, and the efficiency bonus is a further +25%
  double const mapping_multiplier = efficiency_bonus ? 1.25 : 1.0;
  double const dss_value = (fss_value * 3.333333) * mapping_multiplier;

  double final_value = 0.0;

  // 4. how the "first" bonuses work
  if(is_first_discoverer && is_first_mapper)
    {
    // being first in both categories gives a multiplier of ~3.695x on the WHOLE sum
    final_value = (fss_value + dss_value) * 3.695244;
    }
  else if(is_first_discoverer)
    {
    // first discoverer only (FSS)
    final_value = (fss_value * 2.6) + dss_value;
    }
  else if(is_first_mapper)
    {
    // first mapper only (DSS)
    final_value = fss_value + (dss_value * 3.695244);
    }
  else
    {
    // no "first" bonuses at all
    final_value = fss_value + dss_value;
    }

  return static_cast<uint32_t>(std::max(500.0, std::round(final_value)));
  }

auto star_value(std::string_view star_type, double stellar_mass, bool is_first_discoverer) noexcept -> uint32_t
  {
  constexpr static auto get_base_value = [](std::string_view type) -> double
  {
    using namespace std::literals;

    // white dwarfs
    if(type.starts_with("D"sv))
      return 14057.0;

    // neutron stars and black holes - the journal writes them N and H
    if(type == "Neutron"sv or type == "N"sv)
      return 22628.0;
    if(type == "BlackHole"sv or type == "H"sv)
      return 22628.0;

    // supergiants
    if(type.find("SuperGiant"sv) != std::string_view::npos)
      return 33.0;

    // ordinary main sequence stars and the rest (K, G, B, F, O, A, M)
    // most share the same base and differ by mass
    return 1200.0;
  };

  auto const k{get_base_value(star_type)};
  // FDEV's standard formula for stars, and the first discoverer's multiplier on top
  return static_cast<uint32_t>((k + (stellar_mass * k / 66.25)) * (is_first_discoverer ? 2.6 : 1.0));
  }

auto scanned_value(planet_value_info_t const & info, double mass_em, bool is_terraformable, bool is_first_discoverer)
  -> uint32_t
  {
  double const q{std::max(0.3, std::pow(mass_em, 0.2))};
  double const fss_value{(info.base_value + (is_terraformable ? info.terraform_bonus : 0.0)) * q};
  return static_cast<uint32_t>(std::max(500.0, std::round(fss_value * (is_first_discoverer ? 2.6 : 1.0))));
  }

auto aprox_value(body_t const & body) noexcept -> uint32_t
  {
  uint32_t result{};
  if(body.body_type() == body_type_e::star)
    {
    star_details_t const & details{std::get<star_details_t>(body.details)};
    result = star_value(details.star_type, details.stellar_mass);
    }
  else
    {
    planet_details_t const & details{std::get<planet_details_t>(body.details)};

    auto it{std::ranges::find(
      exploration_values,
      details.planet_class,
      [](planet_value_info_t const & body) noexcept -> std::string_view { return body.planet_class; }
    )};
    if(it != exploration_values.end())
      {
      planet_value_info_t const & info{*it};
      result = calculate_value(
        info,
        details.mass_em,
        details.terraform_state != events::terraform_state_e::none,
        not body.was_discovered,
        not details.was_mapped,
        true
      );
      }
    }
  return result;
  }
  }  // namespace exploration

auto value_class(uint32_t const sv) noexcept -> planet_value_e
  {
  if(sv > 400000)
    return planet_value_e::high;
  if(sv > 200000)
    return planet_value_e::medium;
  return planet_value_e::low;
  }

auto micro_resource_key(std::string_view name) -> std::string
  {
  std::string_view key{name};
  if(key.starts_with('$'))
    key.remove_prefix(1);
  if(key.ends_with("_name;"sv))
    key.remove_suffix(6);

  std::string result;
  result.reserve(key.size());
  for(char const c: key)
    result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  return result;
  }

auto to_system_signal(events::fss_signal_discovered_t const & signal, std::chrono::sys_seconds seen)
  -> system_signal_t
  {
  return system_signal_t{
    .system_address = signal.SystemAddress,
    // station names come through as they are, the rest as an identifier with the translation beside it
    .name = signal.SignalName_Localised.empty() ? signal.SignalName : signal.SignalName_Localised,
    .signal_type = signal.SignalType,
    .is_station = signal.IsStation,
    .last_seen = seen
  };
  }

auto filter_current_visit(std::vector<system_signal_t> signals) -> std::vector<system_signal_t>
  {
  if(signals.empty())
    return signals;

  // a longer gap than this separates visits; a shorter one means further batches of the same stay
  constexpr auto visit_gap{std::chrono::hours{2}};

  std::ranges::sort(signals, std::ranges::greater{}, &system_signal_t::last_seen);

  auto cutoff{signals.front().last_seen};
  for(system_signal_t const & signal: signals)
    {
    if(cutoff - signal.last_seen > visit_gap)
      break;
    cutoff = signal.last_seen;
    }

  auto const stale{std::ranges::remove_if(signals, [cutoff](system_signal_t const & signal)
                                          { return signal.last_seen < cutoff; })};
  signals.erase(stale.begin(), stale.end());
  return signals;
  }

auto classify_signal(std::string_view signal_type) noexcept -> signal_class_e
  {
  using enum signal_class_e;

  // stations come through as StationCoriolis, StationONeilOrbis and the like
  if(signal_type.starts_with("Station"))
    return station;

  for(std::string_view type: {"Outpost"sv, "Installation"sv, "FleetCarrier"sv, "SquadronCarrier"sv, "Megaship"sv, "NavBeacon"sv})
    if(signal_type == type)
      return station;

  // Generic stays out of exploration - what sits there is mostly fleeting signals such as
  // Pirate Activity Detected or a debris field, not landmarks
  for(std::string_view type: {"Codex"sv, "TouristBeacon"sv, "Titan"sv})
    if(signal_type == type)
      return exploration;

  return other;
  }

auto organic_value_range(std::string_view name) noexcept -> std::optional<std::pair<uint32_t, uint32_t>>
  {
  auto range_of = [](auto && matches) -> std::optional<std::pair<uint32_t, uint32_t>>
  {
    std::optional<std::pair<uint32_t, uint32_t>> result;
    for(organic_value_t const & entry: matches)
      if(not result)
        result = std::pair{entry.value, entry.value};
      else
        {
        result->first = std::min(result->first, entry.value);
        result->second = std::max(result->second, entry.value);
        }
    return result;
  };

  // a species after ScanOrganic hits the price list directly
  if(auto it{std::ranges::find(organic_values, name, &organic_value_t::species)}; it != organic_values.end())
    return std::pair{it->value, it->value};

  // the genus alone - the whole family, that is everything starting with its name
  if(auto res{range_of(
       organic_values | std::views::filter([name](organic_value_t const & entry)
                                           { return entry.species.starts_with(name) and entry.species != name; })
     )};
     res)
    return res;

  // "Brain Trees" in the journal, "Brain Tree" in the price list
  if(name.ends_with('s'))
    {
    auto const singular{name.substr(0, name.size() - 1)};
    if(auto it{std::ranges::find(organic_values, singular, &organic_value_t::species)}; it != organic_values.end())
      return std::pair{it->value, it->value};
    }

  // "Luteolum Anemone" in the journal, "Anemone" in the price list
  if(auto const space{name.rfind(' ')}; space != std::string_view::npos)
    {
    auto const last_word{name.substr(space + 1)};
    if(auto it{std::ranges::find(organic_values, last_word, &organic_value_t::species)}; it != organic_values.end())
      return std::pair{it->value, it->value};
    }

  return {};
  }

[[nodiscard]]
auto format_credits_value(uint32_t value) -> std::string
  {
  auto s_t = std::to_string(value);
  auto res_t = std::string{};

  int count_t = 0;
  for(auto const & c_t: s_t | std::views::reverse)
    {
    if(count_t != 0 && count_t % 3 == 0)
      res_t += '\'';
    res_t += c_t;
    count_t++;
    }

  std::ranges::reverse(res_t);
  return res_t;
  }

static constexpr auto value_color(planet_value_e value)
  {
  std::string_view color{color_codes_t::reset};
  if(value == planet_value_e::high)
    color = color_codes_t::blue;
  else if(value == planet_value_e::medium)
    color = color_codes_t::yellow;
  return color;
  }

namespace
  {
[[nodiscard]]
auto load_nav_route(std::string journal_dir_path) -> cxx23::expected<events::nav_route_t, std::error_code>
  {
  events::nav_route_t result{};
  std::string buffer;
  std::filesystem::path navroute_json{journal_dir_path};
  navroute_json /= "NavRoute.json";

  if(auto res{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(result, navroute_json.string(), buffer)};
     res) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));

  return result;
  }
  
  [[nodiscard]]
  auto load_fcmaterials(std::string journal_dir_path)
  -> cxx23::expected<events::fcmaterials_t, std::error_code>
  {
  events::fcmaterials_t result;
  std::string buffer;
  std::filesystem::path navroute_json{journal_dir_path};
  navroute_json /= "FCMaterials.json";

  if(auto res{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(result, navroute_json.string(), buffer)};
     res) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));

  return result;
  }
  }  // namespace

auto load_status(std::string journal_dir_path) -> cxx23::expected<events::status_file_t, std::error_code>
  {
  // on foot the game leaves GuiFocus out - a field the file lacks must read as 0, not as whatever the
  // stack held, which once told the overlay a map was open and took the head-up readouts away
  events::status_file_t result{};
  std::string buffer;
  std::filesystem::path status_json{journal_dir_path};
  status_json /= "Status.json";

  if(
    auto res{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(result, status_json.string(), buffer)};
    res
  ) [[unlikely]]
    {
    // the game rewrites the file while we read it now and then, so this is no error worth shouting about
    debug("Status.json not read: {}", glz::format_error(res, buffer));
    return cxx23::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));
    }

  return result;
  }

auto load_market(std::string journal_dir_path) -> cxx23::expected<events::market_file_t, std::error_code>
  {
  events::market_file_t result{};
  std::string buffer;
  std::filesystem::path market_json{journal_dir_path};
  market_json /= "Market.json";

  if(
    auto res{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(result, market_json.string(), buffer)};
    res
  ) [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));

  return result;
  }

auto load_cargo(std::string journal_dir_path) -> cxx23::expected<events::cargo_file_t, std::error_code>
  {
  events::cargo_file_t result{};
  std::string buffer;
  std::filesystem::path cargo_json{journal_dir_path};
  cargo_json /= "Cargo.json";

  if(auto res{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(result, cargo_json.string(), buffer)}; res)
    [[unlikely]]
    return cxx23::unexpected(std::make_error_code(std::errc::resource_unavailable_try_again));

  return result;
  }

auto generic_state_t::discovery(std::string_view input) -> void
  {
  raw_line(input);
  std::string buffer{input};
  events::generic_event_t gevt;
  auto parse_res{glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}>(gevt, buffer)};
  if(parse_res) [[unlikely]]
    {
    warn("failed to parse {}", input);
    return;
    }
  using enum events::event_e;
  auto const parse_and_handle = [&]<typename event_t>() -> void
  {
    event_t obj{};
    auto const parse_res
      = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}>(obj, buffer);

    if(parse_res) [[unlikely]]
      {
      // auto pre{stralgo::substr(input, parse_res.count - 40, 40)};
      // auto post{stralgo::substr(input, parse_res.count, 40)};
      warn("failed to parse {}", input);  // Assumes 'input' is available in scope
      return;
      }

    handle(gevt.timestamp, std::move(obj));  // Assumes 'handle' is available in scope
  };
  auto castres{simple_enum::enum_cast<events::event_e>(gevt.event)};
  if(not castres) [[unlikely]]
    {
    warn("failed to cast event type {}", gevt.event);
    return;
    }
  events::event_e type{*castres};
  switch(type)
    {
    case FSDJump:   parse_and_handle.template operator()<events::fsd_jump_t>(); break;
    case FSDTarget: parse_and_handle.template operator()<events::fsd_target_t>(); break;
    case StartJump: parse_and_handle.template operator()<events::start_jump_t>(); break;

    case FSSDiscoveryScan:  parse_and_handle.template operator()<events::fss_discovery_scan_t>(); break;
    case FSSBodySignals:    parse_and_handle.template operator()<events::fss_body_signals_t>(); break;
    case Market:            parse_and_handle.template operator()<events::market_t>(); break;
    case Docked:            parse_and_handle.template operator()<events::docked_t>(); break;
    case ShipyardTransfer:  parse_and_handle.template operator()<events::shipyard_transfer_t>(); break;
    case Undocked:          parse_and_handle.template operator()<events::undocked_t>(); break;
    case ApproachSettlement:
      parse_and_handle.template operator()<events::approach_settlement_t>();
      break;
    case Disembark:        parse_and_handle.template operator()<events::disembark_t>(); break;
    case SupercruiseEntry: parse_and_handle.template operator()<events::supercruise_entry_t>(); break;
    case BackpackChange:   parse_and_handle.template operator()<events::backpack_change_t>(); break;
    case SellMicroResources:
      parse_and_handle.template operator()<events::sell_micro_resources_t>();
      break;
    case FSSSignalDiscovered:
      parse_and_handle.template operator()<events::fss_signal_discovered_t>();
      break;
    case FSSAllBodiesFound: parse_and_handle.template operator()<events::fss_all_bodies_found_t>(); break;
    case ScanBaryCentre:    parse_and_handle.template operator()<events::scan_bary_centre_t>(); break;
    case Scan:              parse_and_handle.template operator()<events::scan_detailed_scan_t>(); break;
    case SAAScanComplete:   parse_and_handle.template operator()<events::saa_scan_complete_t>(); break;
    case SAASignalsFound:   parse_and_handle.template operator()<events::dss_body_signals_t>(); break;
    case ScanOrganic:       parse_and_handle.template operator()<events::scan_organic_t>(); break;
    case Music:             break;
    case NavRoute:
        {
        auto nr{load_nav_route(journal_dir_path_)};
        if(not nr) [[unlikely]]
          {
          warn("failed to parse event type {}", gevt.event);
          return;
          }
        handle(gevt.timestamp, std::move(*nr));
        }
      break;
    case NavRouteClear:     handle(gevt.timestamp, events::nav_route_clear_t{}); break;
    case FuelScoop:         parse_and_handle.template operator()<events::fuel_scoop_t>(); break;
    case Loadout:           parse_and_handle.template operator()<events::loadout_t>(); break;
    case Location:          parse_and_handle.template operator()<events::location_t>(); break;
    case MissionAbandoned:  parse_and_handle.template operator()<events::mission_abandoned_t>(); break;
    case MissionAccepted:   parse_and_handle.template operator()<events::mission_accepted_t>(); break;
    case MissionCompleted:  parse_and_handle.template operator()<events::mission_completed_t>(); break;
    case MissionFailed:     parse_and_handle.template operator()<events::mission_failed_t>(); break;
    case MissionRedirected: parse_and_handle.template operator()<events::mission_redirected_t>(); break;
    case Missions:          parse_and_handle.template operator()<events::missions_t>(); break;
    // after this event it is known whose the following entries are, to the end of the file
    case Commander:         parse_and_handle.template operator()<events::commander_t>(); break;
    case Cargo:             parse_and_handle.template operator()<events::cargo_t>(); break;  //
    // the crosshairs and what they pay - live only, neither belongs in any database
    case ShipTargeted:      parse_and_handle.template operator()<events::ship_targeted_t>(); break;
    case Bounty:            parse_and_handle.template operator()<events::bounty_t>(); break;
    // the fighter and whoever flies it - also live only
    case LaunchFighter:     parse_and_handle.template operator()<events::launch_fighter_t>(); break;
    case DockFighter:       parse_and_handle.template operator()<events::dock_fighter_t>(); break;
    case FighterDestroyed:  parse_and_handle.template operator()<events::fighter_destroyed_t>(); break;
    case FighterRebuilt:    parse_and_handle.template operator()<events::fighter_rebuilt_t>(); break;
    case CrewAssign:        parse_and_handle.template operator()<events::crew_assign_t>(); break;
    case NpcCrewRank:       parse_and_handle.template operator()<events::npc_crew_rank_t>(); break;
    case Shutdown:          break;
    case CarrierStats:      parse_and_handle.template operator()<events::carrier_stats_t>(); break;
    case FCMaterials:
        {
        // the game writes FCMaterials.json exactly when the bartender is opened, which is this event
        // - before, the reading hung on CarrierStats and lost every third set of prices
        events::fcmaterials_t evt{};
        if(auto res{glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}>(evt, buffer)};
           res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }

        auto file{load_fcmaterials(journal_dir_path_)};
        if(not file) [[unlikely]]
          {
          warn("failed to parse fcmaterials");
          return;
          }

        // the file is overwritten, so it matches the last opening of the bartender and no other
        if(file->MarketID != evt.MarketID)
          return;

        handle(gevt.timestamp, std::move(*file));
        }
      break;
    default:                break;
    }
  }

auto to_body(events::scan_detailed_scan_t && event) -> body_t
  {
  body_t b{
    .value = {},
    .body_id = event.BodyID,
    .name = std::string{body_short_name(event.StarSystem, event.BodyName)},
    .details = {},
    .orbital_period = event.OrbitalPeriod,
    .orbital_inclination = event.OrbitalInclination,
    .distance_from_arrival_ls = event.DistanceFromArrivalLS,
    .semi_major_axis = event.SemiMajorAxis,
    .eccentricity = event.Eccentricity,
    .periapsis = event.Periapsis,
    .radius = event.Radius,
    .was_discovered = event.WasDiscovered,
  };
  if(not event.Luminosity.empty())
    {
    b.details = star_details_t{
      .system_address = event.SystemAddress,
      .star_type = event.StarType,
      .luminosity = event.Luminosity,
      .stellar_mass = event.StellarMass,
      .absolute_magnitude = event.AbsoluteMagnitude,
      .surface_temperature = event.SurfaceTemperature,
      .rotation_period = event.RotationPeriod,
      .age_my = event.Age_MY,
      .sub_class = event.Subclass,
      .parent_star = {},
      .parent_barycenter = {}
    };
    // a star orbits a star or a barycentre, and the nearest of each is the first in the list
    star_details_t & star{std::get<star_details_t>(b.details)};
    if(auto it{std::ranges::find_if(event.Parents, [](events::parent_t const & p) { return p.Star.has_value(); })};
       it != event.Parents.end())
      star.parent_star = *it->Star;
    if(auto it{std::ranges::find_if(event.Parents, [](events::parent_t const & p) { return p.Null.has_value(); })};
       it != event.Parents.end())
      star.parent_barycenter = *it->Null;
    }
  else
    {
    b.details = planet_details_t{
      .parent_planet = {},
      .parent_star = {},
      .parent_barycenter = {},
      .terraform_state = events::terraform_state_e::none,
      .planet_class = event.PlanetClass,
      .atmosphere = event.Atmosphere,
      .atmosphere_type = event.AtmosphereType,
      .atmosphere_composition = event.AtmosphereComposition,
      .composition = event.Composition,
      .signals_ = {},
      .volcanism = event.Volcanism,
      .mass_em = event.MassEM,
      .surface_gravity = event.SurfaceGravity,
      .surface_temperature = event.SurfaceTemperature,
      .surface_pressure = event.SurfacePressure,
      .ascending_node = event.AscendingNode,
      .mean_anomaly = event.MeanAnomaly,
      .rotation_period = event.RotationPeriod,
      .axial_tilt = event.AxialTilt,
      .landable = event.Landable,
      .tidal_lock = event.TidalLock,
      .was_mapped = event.WasMapped,
      .was_footfalled = event.WasFootfalled,
      .mapped = {},
      .footfalled = {}
    };

    planet_details_t & details{std::get<planet_details_t>(b.details)};

    if(not event.TerraformState.empty())
      {
      auto res{simple_enum::enum_cast<events::terraform_state_e>(event.TerraformState)};
      if(res)
        details.terraform_state = *res;
      }
    if(auto it{
         std::ranges::find_if(event.Parents, [](events::parent_t const & p) -> bool { return p.Planet.has_value(); })
       };
       it != event.Parents.end())
      details.parent_planet = *it->Planet;
    if(auto it{
         std::ranges::find_if(event.Parents, [](events::parent_t const & p) -> bool { return p.Star.has_value(); })
       };
       it != event.Parents.end())
      details.parent_star = *it->Star;
    if(auto it{
         std::ranges::find_if(event.Parents, [](events::parent_t const & p) -> bool { return p.Null.has_value(); })
       };
       it != event.Parents.end())
      details.parent_barycenter = *it->Null;

    b.value = exploration::aprox_value(b);
    }

  return b;
  }

generic_state_t::~generic_state_t() {}
