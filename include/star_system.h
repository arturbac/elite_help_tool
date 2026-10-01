#pragma once
#include <events/common.h>
#include <events/exploration.h>
#include <simple_enum/simple_enum.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// a star system as the tool understands it, built out of the journal's events - bodies, rings, barycentres, signals

enum struct planet_value_e
  {
  low,
  medium,
  high
  };

consteval auto adl_enum_bounds(planet_value_e)
  {
  using enum planet_value_e;
  return simple_enum::adl_info{low, high};
  }

[[nodiscard]]
auto value_class(uint32_t const sv) noexcept -> planet_value_e;

enum struct body_type_e : uint8_t
  {
  star,
  planet
  };

consteval auto adl_enum_bounds(body_type_e)
  {
  using enum body_type_e;
  return simple_enum::adl_info{star, planet};
  }

// - Ammonia world
// - Earthlike body
// - Gas giant with ammonia based life
// - Gas giant with water based life
// - Helium rich gas giant
// - High metal content body
// - Icy body
// - Metal rich body
// - Rocky body
// - Rocky ice body
// - Sudarsky class I gas giant
// - Sudarsky class II gas giant
// - Sudarsky class III gas giant
// - Sudarsky class IV gas giant
// - Sudarsky class V gas giant
// - Water giant
// - Water world
inline constexpr uint8_t parent_kind_unknown{0u};

inline constexpr uint8_t parent_kind_planet{1u};

inline constexpr uint8_t parent_kind_star{2u};

inline constexpr uint8_t parent_kind_barycentre{3u};

struct star_details_t
  {
  uint64_t system_address;
  std::string star_type;
  std::string luminosity;
  double stellar_mass;
  double absolute_magnitude;
  double surface_temperature;
  std::optional<double> rotation_period;
  uint32_t age_my;
  uint8_t sub_class;
  ///\brief where the star stands on its own orbit around what it orbits, at the scan's own moment - 0 for
  /// a star alone at the centre, which the game's journal leaves out rather than writes as zero
  double ascending_node{};
  double mean_anomaly{};
  ///\brief what the star orbits - with two stars or more it is a barycentre shared with its partner,
  /// which is what pairs them in a picture of the system; empty for rows written before it was kept
  std::optional<events::body_id_t> parent_star;
  std::optional<events::body_id_t> parent_barycenter;
  ///\brief what kind of body the nearest parent is - the first of the journal's Parents, which one parent
  /// of each kind cannot tell; parent_kind_unknown for rows written before it was kept
  uint8_t nearest_parent{};
  };

struct ring_t
  {
  std::string name;
  std::string ring_class;
  double mass_mt;
  double inner_rad;
  double outer_rad;
  uint32_t parent_body_id;
  int32_t body_id{-1};  // known after DSS
  std::vector<events::signal_t> signals_;
  };

inline constexpr auto ring_name_proj = [](ring_t const & b) noexcept -> std::string_view { return b.name; };

inline constexpr auto ring_body_id_proj = [](ring_t const & b) noexcept -> int32_t { return b.body_id; };

struct planet_details_t
  {
  std::optional<events::body_id_t> parent_planet;
  std::optional<events::body_id_t> parent_star;
  std::optional<events::body_id_t> parent_barycenter;
  events::terraform_state_e terraform_state;
  std::string planet_class;
  std::string atmosphere;       // "thick argon rich atmosphere"
  std::string atmosphere_type;  // "ArgonRich"
  std::vector<events::atmosphere_element_t> atmosphere_composition;
  events::composition_t composition;

  std::vector<events::signal_t> signals_;
  std::vector<events::genus_t> genuses_;

  std::string volcanism;

  double mass_em;
  double surface_gravity;
  double surface_temperature;
  double surface_pressure;
  double ascending_node;
  double mean_anomaly;
  std::optional<double> rotation_period;
  std::optional<double> axial_tilt;
  bool landable;
  bool tidal_lock;
  bool was_mapped;
  bool was_footfalled;
  bool mapped;
  bool footfalled;
  ///\brief what kind of body the nearest parent is - the first of the journal's Parents, which one parent
  /// of each kind cannot tell; parent_kind_unknown for rows written before it was kept
  uint8_t nearest_parent{};
  };

using body_variant_t = std::variant<star_details_t, planet_details_t>;

struct body_t
  {
  uint32_t value;
  events::body_id_t body_id;
  std::string name;
  body_variant_t details;
  double orbital_period;
  double orbital_inclination;
  double distance_from_arrival_ls;
  double semi_major_axis;
  double eccentricity;
  double periapsis;
  double radius;
  bool was_discovered;
  ///\brief when the scan that gave the orbital elements above was taken - what a position "now" is
  /// propagated onward from; default-constructed (the epoch) for rows written before it was kept
  std::chrono::sys_seconds scanned_at{};

  [[nodiscard]]
  auto body_type() const noexcept
    { return std::holds_alternative<planet_details_t>(details) ? body_type_e::planet : body_type_e::star; }

  [[nodiscard]]
  auto value_class() const noexcept -> planet_value_e
    { return ::value_class(value); }
  };

[[nodiscard]]
auto to_body(events::scan_detailed_scan_t && scan) -> body_t;

inline constexpr auto body_body_id_proj = [](body_t const & b) noexcept -> events::body_id_t { return b.body_id; };

inline constexpr auto body_body_name_proj = [](body_t const & b) noexcept -> std::string_view { return b.name; };

struct bary_centre_t
  {
  events::body_id_t body_id;
  double semi_major_axis;
  double eccentricity;
  double orbital_inclination;
  double periapsis;
  double orbital_period;
  double ascending_node;
  double mean_anomaly;
  ///\brief when the ScanBaryCentre that gave the figures above was read
  std::chrono::sys_seconds scanned_at{};
  };

///\brief a lasting signal in the system, one row per name
struct system_signal_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  std::string name;
  std::string signal_type;
  bool is_station;
  ///\brief the last time a scan reported it - construction sites and temporary signals stop coming back
  std::chrono::sys_seconds last_seen;
  };

///\brief keeps the signals seen during the last visit to the system
///\detail the game reports signals in batches that are sometimes incomplete - the same station can drop out
/// in one batch and come back in the next - so a visit is the sum of the batches, not a single batch
[[nodiscard]]
auto filter_current_visit(std::vector<system_signal_t> seen_signals) -> std::vector<system_signal_t>;

[[nodiscard]]
auto to_system_signal(events::fss_signal_discovered_t const & signal, std::chrono::sys_seconds seen)
  -> system_signal_t;

///\brief the type a notable stellar phenomenon - a Lagrange cloud and whatever lives in it - comes through as
inline constexpr std::string_view phenomenon_signal_type{"Codex"};
///\brief the type of a row that is not a signal but a visit - a codex find made in space in the system
///\detail kept beside the signals so that it lasts as long as they do; filter_current_visit leaves it alone
inline constexpr std::string_view phenomenon_visit_type{"CodexEntry"};

///\brief a codex find made in space, as a row of the system's signals - none for a find on the ground or of a body
[[nodiscard]]
auto to_system_signal(events::codex_entry_t const & entry, std::chrono::sys_seconds seen)
  -> std::optional<system_signal_t>;

///\brief a system with a notable stellar phenomenon where no find in space was ever logged
struct unvisited_phenomenon_t
  {
  std::string system;
  ///\brief when the phenomenon was last reported
  std::chrono::sys_seconds last_seen;
  };

///\brief which window a signal belongs to
enum struct signal_class_e : uint8_t
  {
  ///\brief phenomena and landmarks - the exploration window
  exploration,
  ///\brief stations, outposts, installations, carriers - the inhabited system window
  station,
  ///\brief conflict zones and mining sites, stored but not yet shown
  other
  };

consteval auto adl_enum_bounds(signal_class_e)
  {
  using enum signal_class_e;
  return simple_enum::adl_info{exploration, other};
  }

[[nodiscard]]
auto classify_signal(std::string_view signal_type) noexcept -> signal_class_e;

struct star_system_t
  {
  uint64_t system_address;
  std::string name;
  std::string star_type;
  // absolute location in galaxy in LY
  // X	East / West	Positive values run to the right of Sol (looking at the map from above).
  // Y	Up / Down	Height above or below the galactic plane. Sol sits almost at 0.
  // Z	North / South
  std::array<double, 3> system_location;
  std::vector<bary_centre_t> bary_centre;
  std::vector<body_t> bodies;
  std::vector<ring_t> rings;
  bool fss_complete;
  std::vector<system_signal_t> system_signals;

  // the system described, from the Location/FSDJump event
  std::string economy;
  std::string second_economy;
  std::string government;
  std::string allegiance;
  std::string security;
  std::string controlling_faction;
  uint64_t population;
  ///\brief how many stars and planets the system holds, as the discovery scan counted them - 0 until
  /// someone honked; the scans in bodies measured against it say how much of the system is still dark
  uint32_t body_count{};

  [[nodiscard]]
  auto body_by_id(this auto && self, events::body_id_t const body_id) noexcept
    { return std::ranges::find(self.bodies, body_id, body_body_id_proj); }

  [[nodiscard]]
  auto ring_by_id(this auto && self, events::body_id_t const body_id) noexcept
    { return std::ranges::find(self.rings, body_id, ring_body_id_proj); }

  [[nodiscard]]
  auto body_by_name(this auto && self, std::string_view name) noexcept
    { return std::ranges::find(self.bodies, name, body_body_name_proj); }

  ///\brief the game repeats ScanBaryCentre on every rescan with the mean anomaly of that moment, so
  /// a barycentre seen again takes the place of the old reading rather than standing beside it
  auto put_bary_centre(bary_centre_t const & bc) -> bary_centre_t const &
    {
    if(auto it{std::ranges::find(bary_centre, bc.body_id, &bary_centre_t::body_id)}; it != bary_centre.end())
      return *it = bc;
    return bary_centre.emplace_back(bc);
    }
  };

///\brief copies the system description over from a Location/FSDJump event
///\returns true when any of the fields changed
template<typename event_t>
[[nodiscard]]
auto apply_system_info(star_system_t & system, event_t const & event) -> bool
  {
  auto const assign = [](auto & target, auto && value) -> bool
  {
    if(target == value)
      return false;
    target = value;
    return true;
    };

  bool changed{assign(system.economy, event.SystemEconomy_Localised)};
  changed = assign(system.second_economy, event.SystemSecondEconomy_Localised) or changed;
  changed = assign(system.government, event.SystemGovernment_Localised) or changed;
  changed = assign(system.allegiance, event.SystemAllegiance) or changed;
  changed = assign(system.security, event.SystemSecurity_Localised) or changed;
  changed = assign(system.controlling_faction, event.SystemFaction.Name) or changed;
  changed = assign(system.population, event.Population) or changed;
  return changed;
  }

[[nodiscard]]
auto body_short_name(std::string_view system, std::string_view name) -> std::string_view;

[[nodiscard]]
auto planet_name_from_ring_name(std::string_view system, std::string_view name) -> std::string_view;
