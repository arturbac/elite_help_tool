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

struct location_t
  {
  events::body_id_t body_id;
  double x, y, z;
  };

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
  std::span<events::scan_bary_centre_t const> barycentres, std::span<events::scan_detailed_scan_t const> scans
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
    registry[s.BodyID] = {
      s.BodyID,
      s.SemiMajorAxis,
      s.Eccentricity,
      s.OrbitalInclination,
      s.Periapsis,
      s.OrbitalPeriod,
      s.AscendingNode,
      s.MeanAnomaly,
      s.Parents
    };

  // Explicit logic error check: If a barycentre has parents in the log, they should be mapped!
  // Note: Barycentre logs in ED sometimes don't list parents, but they are referenced by bodies.

  std::map<body_id_t, location_t> rel_coords;
  for(auto const & [id, node]: registry)
    rel_coords[id] = calculate_relative_pos(node, 0.0);

  std::vector<location_t> absolute_positions;
  for(auto const & s: scans)
    {
    double abs_x = 0.0, abs_y = 0.0, abs_z = 0.0;
    body_id_t current_id = s.BodyID;

    // Resolve the hierarchy up to the system root
    while(registry.contains(current_id))
      {
      auto const & rel = rel_coords[current_id];
      abs_x += rel.x;
      abs_y += rel.y;
      abs_z += rel.z;

      if(registry[current_id].parents.empty())
        break;
      current_id = registry[current_id].parents[0].id();  // Primary orbital parent
      }
    absolute_positions.push_back({s.BodyID, abs_x, abs_y, abs_z});
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
auto order_calculation_2_opt(std::vector<location_t> path) -> std::vector<location_t>
  {
  if(path.size() < 4)
    return path;  // 2-opt nie ma sensu dla mniej niż 4 punktów

  bool improved = true;
  size_t const n = path.size();

  while(improved)
    {
    improved = false;
    // Zaczynamy od i = 1, aby NIE zmieniać punktu startowego (index 0)
    for(size_t i = 1; i < n - 2; ++i)
      {
      for(size_t j = i + 1; j < n - 1; ++j)
        {
        // Obecne krawędzie: (i-1 -> i) oraz (j -> j+1)
        double const dist_current = calculate_distance(path[i - 1], path[i]) + calculate_distance(path[j], path[j + 1]);

        // Potencjalne nowe krawędzie po odwróceniu segmentu: (i-1 -> j) oraz (i -> j+1)
        double const dist_new = calculate_distance(path[i - 1], path[j]) + calculate_distance(path[i], path[j + 1]);

        if(dist_new < dist_current)
          {
          // Odwracamy segment między i a j
          std::reverse(
            path.begin() + static_cast<std::ptrdiff_t>(i), path.begin() + static_cast<std::ptrdiff_t>(j) + 1
          );
          improved = true;
          }
        }

      // Specjalny przypadek dla ostatniego punktu (ścieżka otwarta - Open TSP)
      // Sprawdzamy, czy zamiana końcówki trasy (i-1 -> i) na (i-1 -> ostatni) jest lepsza
      double const dist_end_current = calculate_distance(path[i - 1], path[i]);
      double const dist_end_new = calculate_distance(path[i - 1], path.back());

      if(dist_end_new < dist_end_current)
        {
        // W specyficznych warunkach Open TSP można tu zastosować dodatkową logikę,
        // ale standardowy 2-opt na segmentach wewnętrznych zazwyczaj wystarcza.
        }
      }
    }

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

[[nodiscard]]
auto parse_timestamp_t(std::string_view input) -> std::optional<utc_time_point_t>
  {
  std::istringstream stream{std::string(input)};
  utc_time_point_t tp;

  // %FT%TZ to skrót formatu ISO 8601 (Date T Time Z)
  // %F = %Y-%m-%d
  // %T = %H:%M:%S
  if(stream >> std::chrono::parse("%FT%TZ", tp))
    return tp;

  return std::nullopt;
  }

static auto body_short_name(std::string_view system, std::string_view name) -> std::string_view
  {
  return name.substr(system.size());
  }

[[nodiscard]]
static auto aprox_value(body_t const & body) noexcept -> sell_value_t
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
    double const mass = body.scan.MassEM > 0.0 ? body.scan.MassEM : 1.0;
    double const mass_multiplier = std::pow(mass, 0.2);

    double disc_mult{1.0};
    double map_mult{1.0};
    if(not body.was_discovered)
      disc_mult = info.bonus_first_fss;
    if(not body.was_mapped)
      map_mult = info.bonus_first_dss;

    if(not body.scan.TerraformState.empty() and info.terraform_multiplier > 1.0)
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

[[nodiscard]]
auto value_class(events::scan_detailed_scan_t const & obj) noexcept -> planet_value_e
  {
  planet_value_e value{};
  if(not obj.TerraformState.empty() or stralgo::starts_with(obj.PlanetClass, "Water"sv)
     or stralgo::starts_with(obj.PlanetClass, "Earth"sv) or stralgo::starts_with(obj.PlanetClass, "Ammonia"sv)
     or stralgo::starts_with(obj.PlanetClass, "Metal rich"sv))
    value = planet_value_e::high;
  else if(stralgo::starts_with(obj.PlanetClass, "High metal"sv)
          or stralgo::starts_with(obj.PlanetClass, "Sudarsky class II "sv))
    value = planet_value_e::medium;
  return value;
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

auto discovery_state_t::simple_discovery(std::string_view input) const -> void
  {
  state_t & state{*this->state};
  std::string buffer{input};
  events::generic_event_t gevt;
  auto parse_res{glz::read<glz::opts{.error_on_unknown_keys = false}>(gevt, buffer)};
  if(parse_res) [[unlikely]]
    {
    warn("failed to parse {}", input);
    return;
    }
  using enum events::event_e;
  switch(gevt.event)
    {
    case FSDTarget:
        {
        events::fsd_target_t fsd_target;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(fsd_target, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        debug("next target [{}] {}", fsd_target.StarClass, fsd_target.Name);
        }
      break;
    case StartJump:
        {
        events::start_jump_t start_jump;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(start_jump, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        info("jump to [{}] {}\n", start_jump.StarClass, start_jump.StarSystem);
        state.bodies.clear();
        state.scan_bary_centre.clear();
        state.system_name = start_jump.StarSystem;
        state.star_class = start_jump.StarClass;
        state.fss_complete = false;
        }
      break;

    case FSSDiscoveryScan:
        {
        events::fss_discovery_scan_t obj;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        info("discovery system {} body:{} nonbody:{}", obj.SystemName, obj.BodyCount, obj.NonBodyCount);
        state.bodies.reserve(obj.BodyCount);
        }
      break;
    case FSSBodySignals:
        {
        events::fss_body_signals_t obj;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        info(" {}", obj.BodyName);
        for(events::signal_t sig: obj.Signals)
          info("   {}: {}", sig.Type_Localised, sig.Count);
        }
      break;
    case FSSAllBodiesFound:
        {
        info("fss scan complete");
        state.fss_complete = true;
        std::ranges::sort(
          state.bodies,
          [](body_t const & l, body_t const & r) -> bool
          {
            if(l.name.size() != r.name.size())  // 1 vs 11
              return l.name.size() < r.name.size();
            return l.name < r.name;
          }
        );
        static constexpr auto value_color = [](planet_value_e value)
        {
          std::string_view color{color_codes_t::reset};
          if(value == planet_value_e::high)
            color = color_codes_t::blue;
          else if(value == planet_value_e::medium)
            color = color_codes_t::yellow;
          return color;
        };
        for(body_t const & obj: state.bodies)
          {
          sell_value_t value{aprox_value(obj)};

          spdlog::info(
            " [{}]{}{:5}- {} [fss: {}cr dss: {}cr] {}",
            obj.scan.BodyID,
            value_color(obj.value),
            obj.name,
            obj.planet_class,
            format_credits_value(value.discovery),
            format_credits_value(value.mapping),
            color_codes_t::reset
          );
          }

        std::vector<events::scan_detailed_scan_t> visiting_medium, visiting_high;
        auto filter_medium = std::ranges::views::filter(
          state.bodies, [](body_t const & body) -> bool { return body.value > planet_value_e::low; }
        );
        auto filter_high = std::ranges::views::filter(
          state.bodies, [](body_t const & body) -> bool { return body.value > planet_value_e::medium; }
        );

        std::ranges::transform(
          filter_medium,
          std::back_inserter(visiting_medium),
          [](body_t const & body) -> events::scan_detailed_scan_t { return body.scan; }
        );
        std::ranges::transform(
          filter_high,
          std::back_inserter(visiting_high),
          [](body_t const & body) -> events::scan_detailed_scan_t { return body.scan; }
        );

        std::unordered_map<body_id_t, body_t const *> name_ref;
        std::ranges::transform(
          filter_medium,
          std::inserter(name_ref, name_ref.end()),
          [](body_t const & body) -> std::pair<body_id_t, body_t const *> { return {body.scan.BodyID, &body}; }
        );

        auto order_info = [&name_ref](std::span<location_t const> order, std::string_view label)
        {
          std::string order_str;
          std::optional<location_t> prev;
          double total_ls{};
          for(location_t const & loc: order)
            {
            auto const & ref{name_ref[loc.body_id]};
            std::string dist_ls;
            if(prev)
              {
              double dls{distance_ls(*prev, loc)};
              total_ls += dls;
              dist_ls = std::format(" [{:1.1f}Ls]", dls);
              }
            order_str.append(
              std::format("{}{}{}{},", value_color(ref->value), ref->name, color_codes_t::reset, dist_ls)
            );
            prev = loc;
            }
          info("visiting order {} [{:1.1f}Ls]: {}", label, total_ls, order_str);
        };
        auto calculate_order_for = [&state, &order_info](std::span<events::scan_detailed_scan_t const> visiting)
        {
          std::vector<location_t> order{order_calculation(state.scan_bary_centre, visiting)};
          order_info(order, "naive"sv);
          std::vector<location_t> order2d{order_calculation_2_opt(order)};
          order_info(order2d, "2nd opt");
        };
        calculate_order_for(visiting_medium);
        calculate_order_for(visiting_high);
        }
      break;
    case ScanBaryCentre:
        {
        events::scan_bary_centre_t obj;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        info(
          "bary_centre:{} {} [SemiMajorAxis:{:2.4f} Eccentricity:{:2.4f} OrbitalInclination:{:2.4f} Periapsis:{:2.4f}"
          " OrbitalPeriod:{:2.4f} AscendingNode:{:2.4f} MeanAnomaly:{:2.4f}]",
          obj.StarSystem,
          obj.BodyID,
          obj.SemiMajorAxis,
          obj.Eccentricity,
          obj.OrbitalInclination,
          obj.Periapsis,
          obj.OrbitalPeriod,
          obj.AscendingNode,
          obj.MeanAnomaly
        );
        state.scan_bary_centre.emplace_back(obj);
        }
      break;
    case Scan:
        {
        events::scan_detailed_scan_t obj;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        if(state.fss_complete)
          return;

        planet_value_e const value{value_class(obj)};

        state.bodies.emplace_back(
          body_t{
            .value = value,
            .name = std::string{body_short_name(obj.StarSystem, obj.BodyName)},
            .planet_class = obj.PlanetClass,
            .was_discovered = obj.WasDiscovered,
            .was_mapped = obj.WasMapped,
            .scan = std::move(obj)
          }
        );
        body_t const & body{state.bodies.back()};
        sell_value_t value_cr{aprox_value(body)};

        spdlog::info(
          "{} {} {} {} [fss: {}cr dss: {}cr]{}{} ",
          body.name,
          body.scan.TerraformState,
          body.scan.PlanetClass,
          body.scan.Atmosphere,
          format_credits_value(value_cr.discovery),
          format_credits_value(value_cr.mapping),
          body.was_discovered ? " \033[33mdiscovered\033[m" : "",
          body.was_mapped ? " \033[31mmapped\033[m" : ""
        );
        }
      break;
    case SAAScanComplete:
        {
        events::saa_scan_complete_t obj;
        parse_res = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
        if(parse_res) [[unlikely]]
          {
          warn("failed to parse {}", input);
          return;
          }
        info("saa scan complete for {}", obj.BodyName);
        }

      break;
    case SAASignalsFound: break;
    case Music:           break;
    case NavRoute:        break;
    case NavRouteClear:   break;
    case FuelScoop:       break;
    case Shutdown:        break;
    }
  }

/*
{ "timestamp":"2025-12-24T10:06:29Z", "event":"StartJump", "JumpType":"Hyperspace", "Taxi":false, "StarSystem":"Pru
Theia IV-I c24-0", "SystemAddress":95194386898, "StarClass":"K" }

{
  "timestamp": "2025-12-24T10:13:51Z",
  "event": "Scan",
  "ScanType": "AutoScan",
  "BodyName": "Pru Theia LV-I c24-0",
  "BodyID": 0,
  "StarSystem": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "DistanceFromArrivalLS": 0.000000,
  "StarType": "M",
  "Subclass": 0,
  "StellarMass": 0.460938,
  "Radius": 437807552.000000,
  "AbsoluteMagnitude": 7.889389,
  "Age_MY": 6622,
  "SurfaceTemperature": 3601.000000,
  "Luminosity": "Va",
  "RotationPeriod": 186821.514346,
  "AxialTilt": 0.000000,
  "WasDiscovered": false,
  "WasMapped": false,
  "WasFootfalled": false
}
{
  "timestamp": "2025-12-24T10:13:57Z",
  "event": "FSSDiscoveryScan",
  "Progress": 0.157770,
  "BodyCount": 12,
  "NonBodyCount": 0,
  "SystemName": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490
}
{
  "timestamp": "2025-12-24T10:14:03Z",
  "event": "Scan",
  "ScanType": "Detailed",
  "BodyName": "Pru Theia LV-I c24-0 1",
  "BodyID": 1,
  "Parents": [
    {
      "Star": 0
    }
  ],
  "StarSystem": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "DistanceFromArrivalLS": 328.278813,
  "TidalLock": false,
  "TerraformState": "",
  "PlanetClass": "High metal content body",
  "Atmosphere": "thick argon rich atmosphere",
  "AtmosphereType": "ArgonRich",
  "AtmosphereComposition": [
    {
      "Name": "Nitrogen",
      "Percent": 94.815651
    },
    {
      "Name": "Argon",
      "Percent": 4.808140
    },
    {
      "Name": "Oxygen",
      "Percent": 0.374139
    }
  ],
  "Volcanism": "",
  "MassEM": 1.685417,
  "Radius": 7219699.000000,
  "SurfaceGravity": 12.887799,
  "SurfaceTemperature": 192.469955,
  "SurfacePressure": 9029279.000000,
  "Landable": false,
  "Composition": {
    "Ice": 0.042851,
    "Rock": 0.640895,
    "Metal": 0.316254
  },
  "SemiMajorAxis": 98387160301.208496,
  "Eccentricity": 0.000465,
  "OrbitalInclination": -0.082995,
  "Periapsis": 178.557664,
  "OrbitalPeriod": 24787623.286247,
  "AscendingNode": 144.852914,
  "MeanAnomaly": 128.309162,
  "RotationPeriod": 315763.205856,
  "AxialTilt": -0.376656,
  "WasDiscovered": false,
  "WasMapped": false,
  "WasFootfalled": false
}
{
  "timestamp": "2025-12-24T10:14:57Z",
  "event": "ScanBaryCentre",
  "StarSystem": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "BodyID": 2,
  "SemiMajorAxis": 239472025632.858276,
  "Eccentricity": 0.000229,
  "OrbitalInclination": -0.050906,
  "Periapsis": 151.681238,
  "OrbitalPeriod": 94125960.469246,
  "AscendingNode": -125.950369,
  "MeanAnomaly": 322.039855
}
{
  "timestamp": "2025-12-24T10:15:10Z",
  "event": "FSSBodySignals",
  "BodyName": "Pru Theia LV-I c24-0 4 a",
  "BodyID": 6,
  "SystemAddress": 95395713490,
  "Signals": [
    {
      "Type": "$SAA_SignalType_Biological;",
      "Type_Localised": "Biological",
      "Count": 2
    }
  ]
}
{
  "timestamp": "2025-12-24T10:15:10Z",
  "event": "FSSAllBodiesFound",
  "SystemName": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "Count": 12
}
{
  "timestamp": "2025-12-24T10:17:07Z",
  "event": "SAAScanComplete",
  "BodyName": "Pru Theia LV-I c24-0 1",
  "SystemAddress": 95395713490,
  "BodyID": 1,
  "ProbesUsed": 5,
  "EfficiencyTarget": 7
}
{
  "timestamp": "2025-12-24T10:17:07Z",
  "event": "SAAScanComplete",
  "BodyName": "Pru Theia LV-I c24-0 1",
  "SystemAddress": 95395713490,
  "BodyID": 1,
  "ProbesUsed": 5,
  "EfficiencyTarget": 7
}
{
  "timestamp": "2025-12-24T10:17:07Z",
  "event": "Scan",
  "ScanType": "Detailed",
  "BodyName": "Pru Theia LV-I c24-0 1",
  "BodyID": 1,
  "Parents": [
    {
      "Star": 0
    }
  ],
  "StarSystem": "Pru Theia LV-I c24-0",
  "SystemAddress": 95395713490,
  "DistanceFromArrivalLS": 328.278818,
  "TidalLock": false,
  "TerraformState": "",
  "PlanetClass": "High metal content body",
  "Atmosphere": "thick argon rich atmosphere",
  "AtmosphereType": "ArgonRich",
  "AtmosphereComposition": [
    {
      "Name": "Nitrogen",
      "Percent": 94.815651
    },
    {
      "Name": "Argon",
      "Percent": 4.808140
    },
    {
      "Name": "Oxygen",
      "Percent": 0.374139
    }
  ],
  "Volcanism": "",
  "MassEM": 1.685417,
  "Radius": 7219699.000000,
  "SurfaceGravity": 12.887799,
  "SurfaceTemperature": 192.469955,
  "SurfacePressure": 9029279.000000,
  "Landable": false,
  "Composition": {
    "Ice": 0.042851,
    "Rock": 0.640895,
    "Metal": 0.316254
  },
  "SemiMajorAxis": 98387160301.208496,
  "Eccentricity": 0.000465,
  "OrbitalInclination": -0.082995,
  "Periapsis": 178.557664,
  "OrbitalPeriod": 24787623.286247,
  "AscendingNode": 144.852914,
  "MeanAnomaly": 128.311835,
  "RotationPeriod": 315763.205856,
  "AxialTilt": -0.376656,
  "WasDiscovered": false,
  "WasMapped": false,
  "WasFootfalled": false
}

 */
