#include <file_io.h>
#include <elite_events.h>
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
using spdlog::info;
using spdlog::warn;
using namespace std::string_view_literals;

using utc_time_point_t = std::chrono::sys_time<std::chrono::milliseconds>;

using events::body_id_t;

namespace exploration
  {
auto is_high_value_star(std::string_view star_class) noexcept -> bool
  {
  using namespace std::literals;
  // Gwiazdy typu A, F, G, K mają najlepszą "Goldilocks Zone"
  return star_class == "A"sv || star_class == "F"sv || star_class == "G"sv || star_class == "K"sv;
  }

auto extract_mass_code(std::string_view name) noexcept -> char
  {
  // Proceduralne nazwy kończą się schematem: [Litery]-[Litera] [MassCode][Liczba]-[Liczba]
  // Szukamy ostatniej spacji, Mass Code to pierwszy znak po niej (jeśli to litera)
  auto const last_space = name.find_last_of(' ');
  if(last_space == std::string_view::npos || last_space + 1 >= name.size())
    return 'a';  // Fallback

  char const code = static_cast<char>(std::tolower(name[last_space + 1]));
  return (code >= 'a' && code <= 'h') ? code : 'a';
  }

auto system_approx_value(std::string_view star_class, std::string_view system_name) noexcept -> planet_value_e
  {
  auto const mass_code = extract_mass_code(system_name);

  // Logika aproksymacji:
  // Kod 'd' przy gwiazdach F/G/A to najczęściej "high" (Terraformables)
  // Kody 'e' i wyżej to zazwyczaj bardzo cenne układy (Neutron/BlackHoles)
  // Kody 'a' i 'b' to zazwyczaj tanie lodowe planety

  if(mass_code >= 'e')
    return planet_value_e::high;

  if(mass_code == 'd')
    {
    // Gwiazdy typu A, F, G w kodzie 'd' mają najwyższą szansę na drogie planety
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

using events::location_t;

[[nodiscard]]
auto calculate_relative_pos(orbital_node_t const & node, double dt) -> location_t
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
auto order_calculation(
  std::span<events::scan_bary_centre_t const> barycentres, std::vector<body_t const *> const & scans
) -> std::vector<location_t>
  {
  std::unordered_map<body_id_t, orbital_node_t> registry;

  // Populate registry with both barycentres and detailed scans
  for(auto const & bc: barycentres)
    registry[bc.BodyID] = {
      bc.BodyID,
      bc.SemiMajorAxis,
      bc.Eccentricity,
      bc.OrbitalInclination,
      bc.Periapsis,
      bc.OrbitalPeriod,
      bc.AscendingNode,
      bc.MeanAnomaly,
      {}
    };

  for(auto const & s: scans)
    {
    registry[s->body_id] = {
      s->body_id,
      s->semi_major_axis,
      s->eccentricity,
      s->orbital_inclination,
      s->periapsis,
      s->orbital_period,
      s->ascending_node,
      s->mean_anomaly,
      {}
    };

    if(s->parent_barycenter)
      registry[*s->parent_barycenter ].parents.emplace_back(events::parent_t{.Null =s->parent_barycenter });
    if(s->parent_star)
      registry[*s->parent_star ].parents.emplace_back(events::parent_t{.Star =s->parent_star });
    if(s->parent_planet)
      registry[*s->parent_planet ].parents.emplace_back(events::parent_t{.Planet =s->parent_planet });
    // Uzupełnianie hierarchii dla barycentrów na podstawie ścieżki rodziców skanu
    // for(size_t i = 0; i + 1 < s->parents.size(); ++i)
    //   {
    //   auto parent_id = s->parents[i].id();
    //   if(registry.contains(parent_id) and registry[parent_id].parents.empty())
    //     {
    //     // Skoro s.Parents[i] to nasze barycentrum, to s.Parents[i+1] jest jego rodzicem
    //     registry[parent_id].parents.push_back(s->parents[i + 1]);
    //     }
    //   }
    }

  // Explicit logic error check: If a barycentre has parents in the log, they should be mapped!
  // Note: Barycentre logs in ED sometimes don't list parents, but they are referenced by bodies.

  std::unordered_map<body_id_t, location_t> rel_coords;
  for(auto const & [id, node]: registry)
    rel_coords[id] = calculate_relative_pos(node, 0.0);

  std::vector<location_t> absolute_positions;
  absolute_positions.reserve(scans.size());

  for(auto const & s: scans)
    {
    double abs_x = 0.0, abs_y = 0.0, abs_z = 0.0;
    body_id_t current_id = s->body_id;

    // Iterujemy w górę drzewa, aż do gwiazdy głównej (brak rodziców)
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

  std::vector<location_t> path;
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

// Implementacja algorytmu 2-opt (zamiana krawędzi) pozwoli na optymalizację trasy wyznaczonej przez algorytm
// najbliższego sąsiada. Jest to szczególnie przydatne w systemach z wieloma ciałami niebieskimi, gdzie zachłanne
// podejście często generuje krzyżujące się ścieżki i niepotrzebne powroty.
//
// W tej wersji wymuszamy, aby punkt startowy (indeks 0) pozostał niezmieniony, ponieważ reprezentuje on Twoją aktualną
// pozycję po wejściu do systemu.
//
// Optymalizacja Trasy: Algorytm 2-opt w C++23
// C++

[[nodiscard]]
auto calculate_distance(location_t const & a, location_t const & b) noexcept -> double
  {
  double const dx = a.x - b.x;
  double const dy = a.y - b.y;
  double const dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

[[nodiscard]]
auto order_calculation_2_opt(location_t const player_pos, std::span<location_t const> targets)
  -> std::vector<location_t>
  {
  if(targets.empty())
    return {};

  // 1. Inicjalizacja: Budujemy ścieżkę zaczynając od gracza (Nearest Neighbor)
  std::vector<location_t> path;
  path.reserve(targets.size() + 1);
  path.push_back(player_pos);

  std::vector<location_t> remaining(targets.begin(), targets.end());

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

  // 2. Optymalizacja 2-opt (Open TSP)
  if(path.size() < 3)
    return path;

  bool improved = true;
  auto const n = path.size();

  while(improved)
    {
    improved = false;
    // i = 1: Blokujemy pozycję gracza na indeksie 0
    for(size_t i = 1; i < n - 1; ++i)
      {
      for(size_t j = i + 1; j < n; ++j)
        {
        // Koszt obecny: (i-1 -> i) + (j -> j+1 jeśli istnieje)
        double const d_i_prev_i = calculate_distance(path[i - 1], path[i]);
        double const d_j_j_next = (j < n - 1) ? calculate_distance(path[j], path[j + 1]) : 0.0;

        // Koszt nowy po odwróceniu: (i-1 -> j) + (i -> j+1 jeśli istnieje)
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

  // Opcjonalnie: usuwamy pozycję gracza z przodu, jeśli wynik ma zawierać tylko cele
  path.erase(path.begin());
  return path;
  }

/**
 * @brief Calculates Euclidean distance between two points in Light Seconds.
 * * The input coordinates are assumed to be in meters (based on SemiMajorAxis).
 * Speed of light constant is taken as 299,792,458 m/s.
 */
[[nodiscard]]
constexpr auto distance_ls(location_t const a, location_t const b) noexcept -> double
  {
  // Constant for the speed of light in m/s
  constexpr double light_speed_mps = 299'792'458.0;

  double const dx = a.x - b.x;
  double const dy = a.y - b.y;
  double const dz = a.z - b.z;

  // Distance in meters
  double const distance_m = std::sqrt(dx * dx + dy * dy + dz * dz);

  // Convert to Light Seconds
  return distance_m / light_speed_mps;
  }

auto body_short_name(std::string_view system, std::string_view name) -> std::string_view
  {
  return name.substr(system.size());
  }

auto aprox_value(body_t const & body) noexcept -> sell_value_t
  {
  sell_value_t result{};
  auto it{std::ranges::find(
    exploration_values,
    body.planet_class,
    [](planet_value_info_t const & body) noexcept -> std::string_view { return body.planet_class; }
  )};
  if(it != exploration_values.end())
    {
    planet_value_info_t const & info{*it};
    // 1. Obliczenie mnożnika masy: (MassEarth ^ 0.2)
    // W Journalu masa jest podawana w wielokrotnościach masy Ziemi (MassEM)
    // Jeśli masa wynosi 0 (np. słońce/czarna dziura - rzadkie w tym kontekście), używamy 1.0
    double const mass = body.mass_em > 0.0 ? body.mass_em : 1.0;
    double const mass_multiplier = std::pow(mass, 0.2);

    double disc_mult{1.0};
    double map_mult{1.0};
    if(not body.was_discovered)
      disc_mult = info.bonus_first_fss;
    if(not body.was_mapped)
      map_mult = info.bonus_first_dss;

    if(not body.terraform_state.empty() and info.terraform_multiplier > 1.0)
      {
      disc_mult *= info.terraform_multiplier;
      map_mult *= info.terraform_multiplier;
      }

    // result.discovery = uint32_t(std::round(info.discovery_value * disc_mult));
    // result.mapping = uint32_t(std::round(info.mapping_value * map_mult));
    result.discovery = static_cast<uint32_t>(std::round(info.discovery_value * mass_multiplier * disc_mult));
    result.mapping = static_cast<uint32_t>(std::round(info.mapping_value * mass_multiplier * map_mult));
    }
  return result;
  }

auto value_class(sell_value_t const sv) noexcept -> planet_value_e
  {
  if(sv.mapping > 400000)
    return planet_value_e::high;
  if(sv.mapping > 200000)
    return planet_value_e::medium;
  return planet_value_e::low;
  }

[[nodiscard]]
constexpr static auto star_value_calculate(events::scan_detailed_scan_t const & scan) -> double
  {
  constexpr static auto get_base_value = [](std::string_view type) -> double
  {
    using namespace std::literals;

    // Białe karły
    if(type.starts_with("D"sv))
      return 14057.0;

    // Gwiazdy neutronowe i Czarne dziury
    if(type == "Neutron"sv)
      return 22628.0;
    if(type == "BlackHole"sv)
      return 22628.0;

    // Supergiganty
    if(type.find("SuperGiant"sv) != std::string_view::npos)
      return 33.0;

    // Standardowe gwiazdy ciągu głównego i inne (K, G, B, F, O, A, M)
    // Większość ma tę samą bazę, różnią się masą
    return 1200.0;
  };
  if(!scan.StarType || !scan.StellarMass) [[unlikely]]
    return 0.0;

  auto const k = get_base_value(*scan.StarType);
  auto const mass = *scan.StellarMass;

  // Standardowy wzór FDEV dla gwiazd
  return k + (mass * k / 66.25);
  }

[[nodiscard]]
static auto format_credits_value(uint32_t value) -> std::string
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

auto generic_state_t::discovery(std::string_view input) -> void
  {
  std::string buffer{input};
  events::generic_event_t gevt;
  auto parse_res{glz::read<glz::opts{.error_on_unknown_keys = false}>(gevt, buffer)};
  if(parse_res) [[unlikely]]
    {
    warn("failed to parse {}", input);
    return;
    }
  using enum events::event_e;
  auto const parse_and_handle = [&]<typename event_t>() -> void
  {
    event_t obj{};
    auto const parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);

    if(parse_res) [[unlikely]]
      {
      warn("failed to parse {}", input);  // Assumes 'input' is available in scope
      return;
      }

    handle(std::move(obj));  // Assumes 'handle' is available in scope
  };

  switch(gevt.event)
    {
    case FSDJump:   parse_and_handle.template operator()<events::fsd_jump_t>(); break;
    case FSDTarget: parse_and_handle.template operator()<events::fsd_target_t>(); break;
    case StartJump: parse_and_handle.template operator()<events::start_jump_t>(); break;

    case FSSDiscoveryScan:  parse_and_handle.template operator()<events::fss_discovery_scan_t>(); break;
    case FSSBodySignals:    parse_and_handle.template operator()<events::fss_body_signals_t>(); break;
    case FSSAllBodiesFound: parse_and_handle.template operator()<events::fss_all_bodies_found_t>(); break;
    case ScanBaryCentre:    parse_and_handle.template operator()<events::scan_bary_centre_t>(); break;
    case Scan:              parse_and_handle.template operator()<events::scan_detailed_scan_t>(); break;
    case SAAScanComplete:   parse_and_handle.template operator()<events::saa_scan_complete_t>(); break;
    case SAASignalsFound:   break;
    case Music:             break;
    case NavRoute:          break;
    case NavRouteClear:     break;
    case FuelScoop:         break;
    case Shutdown:          break;
    default:                break;
    }
  }

auto to_body(events::scan_detailed_scan_t && event) -> body_t
  {
  body_t b{
    .value = {},
    .system_address = event.SystemAddress,
    .body_id = event.BodyID,
    .name = std::string{body_short_name(event.StarSystem, event.BodyName)},
    .planet_class = event.PlanetClass,
    .terraform_state = event.TerraformState,
    .atmosphere = event.Atmosphere,
    .atmosphere_type = event.AtmosphereType,
    .atmosphere_composition = event.AtmosphereComposition,
    .parent_planet = {},
    .parent_star = {},
    .parent_barycenter = {},
    .volcanism = event.Volcanism,
    .mass_em = event.MassEM,
    .radius = event.Radius,
    .absolute_magnitude = event.AbsoluteMagnitude,
    .surface_gravity = event.SurfaceGravity,
    .surface_temperature = event.SurfaceTemperature,
    .surface_pressure = event.SurfacePressure,
    .semi_major_axis = event.SemiMajorAxis,
    .eccentricity = event.Eccentricity,
    .orbital_inclination = event.OrbitalInclination,
    .periapsis = event.Periapsis,
    .orbital_period = event.OrbitalPeriod,
    .ascending_node = event.AscendingNode,
    .mean_anomaly = event.MeanAnomaly,
    .rotation_period = event.RotationPeriod,
    .axial_tilt = event.AxialTilt,
    .landable = event.Landable,
    .tidal_lock = event.TidalLock,
    .was_discovered = event.WasDiscovered,
    .was_mapped = event.WasMapped,
    .mapped = {}
  };
  b.value = aprox_value(b);
  if(auto it{
       std::ranges::find_if(event.Parents, [](events::parent_t const & p) -> bool { return p.Planet.has_value(); })
     };
     it != event.Parents.end())
    b.parent_planet = *it->Planet;
  if(auto it{
       std::ranges::find_if(event.Parents, [](events::parent_t const & p) -> bool { return p.Star.has_value(); })
     };
     it != event.Parents.end())
    b.parent_star = *it->Star;
  if(auto it{
       std::ranges::find_if(event.Parents, [](events::parent_t const & p) -> bool { return p.Null.has_value(); })
     };
     it != event.Parents.end())
    b.parent_barycenter = *it->Null;
  return b;
  }

auto discovery_state_t::handle(events::event_holder_t && e) -> void
  {
  state_t & state{*this->state};
  std::visit(
    [&state]<typename T>(T & event)
    {
      if constexpr(std::same_as<T, events::fsd_jump_t>)
        {
        state.jump_info = std::move(event);
        }

      else if constexpr(std::same_as<T, events::fsd_target_t>)
        {
        planet_value_e const vl{exploration::system_approx_value(event.StarClass, event.Name)};
        debug("next target {}[{}] {}\033[m", value_color(vl), event.StarClass, event.Name);
        }

      else if constexpr(std::same_as<T, events::start_jump_t>)
        {
        planet_value_e const vl{exploration::system_approx_value(event.StarClass, event.StarSystem)};
        info("[{}] jump to {}[{}] {}\033[m\n", event.timestamp, value_color(vl), event.StarClass, event.StarSystem);
        state.system = star_system_t{
          .system_address = event.SystemAddress,
          .name = event.StarSystem,
          .star_type = event.StarClass,
          .luminosity = {},
          .scan_bary_centre = {},
          .bodies = {},
          .stellar_mass = {},
          .sub_class = {}
        };
        state.fss_complete = false;
        }

      else if constexpr(std::same_as<T, events::fss_discovery_scan_t>)
        {
        info("discovery system {} body:{} nonbody:{}", event.SystemName, event.BodyCount, event.NonBodyCount);
        state.system.bodies.reserve(event.BodyCount);
        }

      else if constexpr(std::same_as<T, events::fss_body_signals_t>)
        {
        info(" {}", event.BodyName);
        for(events::signal_t const & signal: event.Signals)
          info("   {}: {}", signal.Type_Localised, signal.Count);

        auto it{state.system.body_by_id(event.BodyID)};
        if(it != state.system.bodies.end())
          it->signals_ = event.Signals;
        }

      else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
        {
        debug("fss scan complete");
        state.fss_complete = true;
        std::ranges::sort(
          state.system.bodies,
          [](body_t const & l, body_t const & r) -> bool
          {
            if(l.name.size() != r.name.size())  // 1 vs 11
              return l.name.size() < r.name.size();
            return l.name < r.name;
          }
        );

        for(body_t const & obj: state.system.bodies)
          {
          sell_value_t value{aprox_value(obj)};

          spdlog::info(
            " [{}]{}{:5}- {} [fss: {}cr dss: {}cr] {}",
            obj.body_id,
            value_color(obj.value_class()),
            obj.name,
            obj.planet_class,
            format_credits_value(value.discovery),
            format_credits_value(value.mapping),
            color_codes_t::reset
          );
          }

        // std::vector<events::scan_detailed_scan_t> visiting_medium;
        auto filter_medium = std::ranges::views::filter(
          state.system.bodies, [](body_t const & body) -> bool { return body.value_class() > planet_value_e::low; }
        );

        // std::ranges::transform(
        //   filter_medium,
        //   std::back_inserter(visiting_medium),
        //   [](body_t const & body) -> events::scan_detailed_scan_t { return body.scan; }
        // );

        std::unordered_map<body_id_t, body_t const *> name_ref;
        std::ranges::transform(
          filter_medium,
          std::inserter(name_ref, name_ref.end()),
          [](body_t const & body) -> std::pair<body_id_t, body_t const *> { return {body.body_id, &body}; }
        );

        auto order_info = [&name_ref](std::span<location_t const> order, std::string_view label)
        {
          std::string order_str;
          std::optional<location_t> prev;
          double total_ls{};
          for(location_t const & loc: order)
            {
            std::string dist_ls;
            planet_value_e vc{};
            std::string_view name{"unknown"};
            if(auto it{name_ref.find(loc.body_id)}; it != name_ref.end() and it->second != nullptr)
              {
              vc = it->second->value_class();
              name = it->second->name;
              }
            if(prev)
              {
              double dls{distance_ls(*prev, loc)};
              total_ls += dls;
              dist_ls = std::format(" [{:1.1f}Ls]", dls);
              }
            order_str.append(std::format("{}{}{}{},", value_color(vc), name, color_codes_t::reset, dist_ls));
            prev = loc;
            }
          info("visiting order {} [{:1.1f}Ls]: {}", label, total_ls, order_str);
        };
        auto calculate_order_for = [&state, &order_info](std::vector<body_t const *> const & visiting)
        {
          std::vector<location_t> order{order_calculation(state.system.scan_bary_centre, visiting)};
          // order_info(order, "naive"sv);
          std::vector<location_t> order2d{order_calculation_2_opt(state.jump_info.player_position(), order)};
          order_info(order2d, "2nd opt");
        };
        std::unordered_map<int, std::vector<body_t const *>> sub_systems;
        for(body_t const & body: filter_medium)
          {
          int parent{-1};
          if(not body.parent_planet)
            parent = *body.parent_planet;
          sub_systems[parent].emplace_back(&body);
          }
        for(auto const & subsystem: sub_systems)
          calculate_order_for(subsystem.second);
        }

      else if constexpr(std::same_as<T, events::scan_bary_centre_t>)
        {
        state.system.scan_bary_centre.emplace_back(std::move(event));
        }

      else if constexpr(std::same_as<T, events::scan_detailed_scan_t>)
        {
        if(state.fss_complete)
          return;

        state.system.bodies.emplace_back(to_body(std::move(event)));
        body_t & body{state.system.bodies.back()};
          // star_value_calculate
          // if(event.StarType)
          //   {
          //   body.value.discovery = star_value_calculate(event);
          //   }
          // else
          {
          sell_value_t value_cr{aprox_value(body)};
          body.value = value_cr;
          }
        spdlog::info(
          "{}{} {} {} {}\033[m [fss: {}cr dss: {}cr]{}{} ",
          value_color(body.value_class()),
          body.name,
          body.terraform_state,
          body.planet_class,
          body.atmosphere,
          format_credits_value(body.value.discovery),
          format_credits_value(body.value.mapping),
          body.was_discovered ? " \033[33mdiscovered\033[m" : "",
          body.was_mapped ? " \033[31mmapped\033[m" : ""
        );
        }
      else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
        {
        info("saa scan complete for {}", event.BodyName);
        auto it{state.system.body_by_id(event.BodyID)};
        if(it != state.system.bodies.end())
          it->mapped = true;
        }
    },
    e
  );
  }

generic_state_t::~generic_state_t() {}
