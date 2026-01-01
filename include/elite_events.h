#pragma once
#include <variant>
#include <string>
#include <simple_enum/glaze_json_enum_name.hpp>
#include <chrono>

namespace color_codes_t
  {
inline constexpr std::string_view reset = "\033[m";
inline constexpr std::string_view red = "\033[31m";
inline constexpr std::string_view green = "\033[32m";
inline constexpr std::string_view blue = "\033[34m";
inline constexpr std::string_view yellow = "\033[33m";
  };  // namespace color_codes_t

namespace events
  {

enum struct event_e
  {
  FSDTarget,
  FSDJump,
  StartJump,
  ReceiveText,
  FSSDiscoveryScan,
  FSSBodySignals,
  FSSAllBodiesFound,
  FSSSignalDiscovered,
  DiscoveryScan,
  Scan,
  ScanBaryCentre,
  SAAScanComplete,
  SAASignalsFound,
  SupercruiseDestinationDrop,
  SupercruiseExit,
  Cargo,
  Loadout,
  Missions,
  Location,
  LoadGame,
  Statistics,
  EngineerProgress,
  Reputation,
  Progress,
  Rank,
  Materials,
  Commander,
  Fileheader,
  Music,
  FuelScoop,
  Shutdown,
  ReservoirReplenished,
  ShipLocker,
  NpcCrewPaidWage,
  RedeemVoucher,
  RefuelAll,
  ModuleInfo,
  Outfitting,
  StoredModules,
  ModuleBuy,
  Repair,
  ModuleStore,
  MultiSellExplorationData,
  SellExplorationData,
  DockingRequested,
  DockingGranted,
  Docked,
  Promotion,
  NavRoute,
  NavRouteClear
  };

consteval auto adl_enum_bounds(event_e)
  {
  using enum event_e;
  return simple_enum::adl_info{FSDTarget, NavRouteClear};
  }
enum struct scan_type_e
  {
  AutoScan,
  Detailed
  };

consteval auto adl_enum_bounds(scan_type_e)
  {
  using enum scan_type_e;
  return simple_enum::adl_info{AutoScan, Detailed};
  }

struct generic_event_t
  {
  std::chrono::sys_seconds timestamp;
  event_e event;
  std::optional<scan_type_e> ScanType;
  };

using utc_time_point_t = std::chrono::sys_time<std::chrono::milliseconds>;

[[nodiscard]]
auto parse_timestamp_t(std::string_view input) -> std::optional<utc_time_point_t>;

/*
{
  "timestamp": "2025-12-24T10:13:34Z",
  "event": "FSDTarget",
  "Name": "Pru Theia PE-G d11-4",
  "SystemAddress": 149376059619,
  "StarClass": "F",
  "RemainingJumpsInRoute": 17
}
 */
struct fsd_target_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Name;
  std::string StarClass;
  uint64_t SystemAddress;
  uint32_t RemainingJumpsInRoute;
  };

enum struct jump_type_e
  {
  Hyperspace,
  Supercruise
  };

consteval auto adl_enum_bounds(jump_type_e)
  {
  using enum jump_type_e;
  return simple_enum::adl_info{Hyperspace, Hyperspace};
  }

///\brief When written: at the start of a Hyperspace or Supercruise jump (start of countdown)
struct start_jump_t
  {
  std::chrono::sys_seconds timestamp;
  jump_type_e JumpType;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::string StarClass;
  };

using body_id_t = uint32_t;

struct faction_state_trend_t
  {
  std::string State;
  int32_t Trend;
  };

struct faction_info_t
  {
  std::string Name;
  std::string FactionState;
  std::string Government;
  std::string Allegiance;
  std::string Happiness_Localised;
  double Influence;
  double MyReputation;
  };

struct system_faction_t
  {
  std::string Name;
  };

struct location_t
  {
  events::body_id_t body_id;
  double x, y, z;
  };

///\brief When written: when jumping from one star system to another
///\detail Note, when following a multi-jump route, this will typically appear for the next star, during a jump, ie
/// after "StartJump" but before the "FSDJump"
struct fsd_jump_t
  {
  std::chrono::sys_seconds timestamp;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::array<double, 3> StarPos;  // [x, y, z]
  std::string Body;
  double JumpDist;
  double FuelUsed;
  double FuelLevel;
  bool BoostUsed;
  bool Taxi;
  bool Multicrew;

  [[nodiscard]]
  constexpr auto player_position() const noexcept -> location_t
    {
    return location_t{{}, StarPos[0], StarPos[1], StarPos[2]};
    }

  system_faction_t SystemFaction;
  std::string SystemAllegiance;
  std::string SystemEconomy_Localised;
  std::string SystemSecondEconomy_Localised;
  std::string SystemGovernment_Localised;
  std::string SystemSecurity_Localised;
  std::string ControllingPower;
  std::string PowerplayState;
  double PowerplayStateControlProgress;
  uint32_t PowerplayStateReinforcement;
  uint32_t PowerplayStateUndermining;
  std::vector<std::string> Powers;
  int64_t Population;

  bool Wanted;
  std::vector<faction_info_t> Factions;
  };

///\brief When plotting a multi-star route, the file "NavRoute.json" is written in the same directory as the journal,
/// with a list of stars along that route
struct nav_route_t
  {
  };

struct nav_route_clear_t
  {
  };

struct fss_discovery_scan_t
  {
  double Progress;
  uint32_t BodyCount;
  uint32_t NonBodyCount;
  std::string SystemName;
  uint64_t SystemAddress;
  };

struct parent_t
  {
  std::optional<uint32_t> Planet;
  std::optional<uint32_t> Star;
  std::optional<uint32_t> Null;

  auto id() const noexcept -> uint32_t
    {
    if(Planet)
      return *Planet;
    if(Star)
      return *Star;
    if(Null)
      return *Null;
    return 0;
    }
  };
// Gases in AtmosphereComposition
enum struct atmosphere_gas_type_e : uint8_t
  {
  Water,
  Oxygen,
  CarbonDioxide,
  SulphurDioxide,
  Ammonia,
  Methane,
  Nitrogen,
  Hydrogen,
  Helium,
  Neon,
  Argon,
  Silicates,
  Iron
  };

consteval auto adl_enum_bounds(atmosphere_gas_type_e)
  {
  using enum atmosphere_gas_type_e;
  return simple_enum::adl_info{Water, Iron};
  }

struct atmosphere_element_t
  {
  atmosphere_gas_type_e Name;
  float Percent;
  };

struct ring_t
  {
  std::string Name;
  std::string RingClass;
  double MassMT;
  double InnerRad;
  double OuterRad;
  };

enum struct meterial_type_e : uint8_t
  {
  Bromellite,
  LithiumHydroxide,
  MethaneClathrate,
  MethanolMonohydrateCrystals,
  Samarium,
  antimony,
  arsenic,
  bauxite,
  cadmium,
  carbon,
  chromium,
  cobalt,
  coltan,
  gallite,
  germanium,
  haematite,
  indite,
  iron,
  lepidolite,
  liquidoxygen,
  manganese,
  mercury,
  molybdenum,
  nickel,
  niobium,
  phosphorus,
  polonium,
  ruthenium,
  rutile,
  selenium,
  sulphur,
  technetium,
  tellurium,
  tin,
  tritium,
  tungsten,
  uraninite,
  vanadium,
  water,
  yttrium,
  zinc,
  zirconium
  };

consteval auto adl_enum_bounds(meterial_type_e)
  {
  using enum meterial_type_e;
  return simple_enum::adl_info{Bromellite, zirconium};
  }
enum struct terraform_state_e : uint8_t
  {
  none,
  Terraformable,
  Terraforming,
  Terraformed
  };

consteval auto adl_enum_bounds(terraform_state_e)
  {
  using enum terraform_state_e;
  return simple_enum::adl_info{none, Terraformed};
  }

struct maetrial_t
  {
  meterial_type_e Name;
  float Percent;
  };

struct composition_t
  {
  float Ice;
  float Rock;
  float Metal;
  };

struct scan_detailed_scan_t
  {
  std::string BodyName;
  std::vector<ring_t> Rings;
  std::optional<double> RotationPeriod;
  std::optional<double> AxialTilt;
  double DistanceFromArrivalLS;
  double SemiMajorAxis;
  double Eccentricity;
  double OrbitalInclination;
  double Periapsis;
  double OrbitalPeriod;
  body_id_t BodyID;
  bool WasDiscovered;
  bool WasMapped;

  // star
  std::string StarSystem;
  std::string StarType;
  std::string Luminosity;
  uint64_t SystemAddress;
  double StellarMass;
  double Radius;
  double AbsoluteMagnitude;
  double SurfaceTemperature;
  uint32_t Age_MY;
  uint8_t Subclass;

  // planets
  std::vector<parent_t> Parents;
  std::string TerraformState;
  std::string PlanetClass;
  std::string Atmosphere;
  std::string AtmosphereType;
  std::vector<atmosphere_element_t> AtmosphereComposition;
  std::string Volcanism;
  composition_t Composition;
  double MassEM;
  double SurfaceGravity;
  double SurfacePressure;
  double AscendingNode;
  double MeanAnomaly;

  bool Landable;
  bool TidalLock;
  bool WasFootfalled;
  };

struct saa_scan_complete_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  uint16_t ProbesUsed;
  uint16_t EfficiencyTarget;
  };

struct scan_bary_centre_t
  {
  std::string StarSystem;
  uint64_t SystemAddress;
  body_id_t BodyID;
  double SemiMajorAxis;
  double Eccentricity;
  double OrbitalInclination;
  double Periapsis;
  double OrbitalPeriod;
  double AscendingNode;
  double MeanAnomaly;
  };

enum struct signal_type_e
  {
  Biological,
  Geological,
  Human
  };

consteval auto adl_enum_bounds(signal_type_e)
  {
  using enum signal_type_e;
  return simple_enum::adl_info{Biological, Human};
  }

struct signal_t
  {
  signal_type_e Type_Localised;
  uint16_t Count;
  };

struct fss_body_signals_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  std::vector<signal_t> Signals;
  };

// {
//   "timestamp": "2025-12-24T10:15:10Z",
//   "event": "FSSAllBodiesFound",
//   "SystemName": "Pru Theia LV-I c24-0",
//   "SystemAddress": 95395713490,
//   "Count": 12
// }

struct fss_all_bodies_found_t
  {
  std::string SystemName;
  uint64_t SystemAddress;
  uint32_t Count;
  };

using event_holder_t = std::variant<
  fsd_jump_t,
  fsd_target_t,
  start_jump_t,
  fss_discovery_scan_t,
  fss_body_signals_t,
  fss_all_bodies_found_t,
  scan_bary_centre_t,
  scan_detailed_scan_t,
  saa_scan_complete_t>;

  }  // namespace events

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

struct sell_value_t
  {
  uint32_t discovery;
  uint32_t mapping;
  };

[[nodiscard]]
auto value_class(sell_value_t const sv) noexcept -> planet_value_e;
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
  };

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
  std::string volcanism;
  
  double mass_em;
  double surface_gravity;
  double surface_pressure;
  double ascending_node;
  double mean_anomaly;
  std::optional<double> rotation_period;
  std::optional<double> axial_tilt;
  bool landable;
  bool tidal_lock;
  bool was_mapped;
  bool mapped;
  };

using body_variant_t = std::variant<star_details_t, planet_details_t>;

struct body_t
  {
  sell_value_t value;
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
  

  [[nodiscard]]
  auto body_type() const noexcept
    {
    return std::holds_alternative<planet_details_t>(details) ? body_type_e::planet : body_type_e::star;
    }

  [[nodiscard]]
  auto value_class() const noexcept -> planet_value_e
    {
    return ::value_class(value);
    }
  };

[[nodiscard]]
auto to_body(events::scan_detailed_scan_t && scan) -> body_t;

inline constexpr auto body_body_id_proj = [](body_t const & b) noexcept -> events::body_id_t { return b.body_id; };

struct star_system_t
  {
  uint64_t system_address;
  std::string name;
  std::string star_type;
  std::string luminosity;
  std::vector<events::scan_bary_centre_t> scan_bary_centre;
  std::vector<body_t> bodies;
  double stellar_mass;
  uint8_t sub_class;

  [[nodiscard]]
  auto body_by_id(this auto && self, events::body_id_t const body_id) noexcept
    {
    return std::ranges::find(self.bodies, body_id, body_body_id_proj);
    }
  };

struct state_t
  {
  star_system_t system;
  bool fss_complete;
  events::fsd_jump_t jump_info;
  };

struct generic_state_t
  {
  virtual ~generic_state_t();
  auto discovery(std::string_view input) -> void;
  virtual auto handle(events::event_holder_t && event) -> void = 0;
  };

struct discovery_state_t : public generic_state_t
  {
  state_t * state;
  void handle(events::event_holder_t && event) override;
  };

struct planet_value_info_t
  {
  std::string_view planet_class;
  uint32_t discovery_value;          // FSS (Base)
  uint32_t mapping_value;            // FSS + DSS + Efficiency (Base)
  double bonus_first_fss;            // Multiplier for being the first to scan
  double bonus_first_dss;            // Multiplier for being the first to map
  double terraform_multiplier{1.0};  // Nowe pole
  };

auto body_short_name(std::string_view system, std::string_view name) -> std::string_view;

static constexpr std::array<planet_value_info_t, 19> exploration_values{
  {{"Metal rich body", 21'790, 129'900, 1.5, 3.45},
   {"High metal content body", 9'693, 57'700, 1.5, 3.45, 5.4},
   {"Rocky body", 300, 1'500, 1.5, 3.45, 260.0},
   {"Icy body", 300, 1'500, 1.5, 3.45},
   {"Rocky ice body", 300, 1'800, 1.5, 3.45},
   {"Earthlike body", 64'831, 386'400, 1.5, 3.45},
   {"Water world", 24'831, 148'000, 1.5, 3.45, 2.1},  // Mnożnik terra ok. 2.1
   {"Ammonia world", 33'268, 198'300, 1.5, 3.45},
   {"Water giant", 1'000, 4'000, 1.5, 3.45},
   {"Water giant with life", 1'500, 6'000, 1.5, 3.45},
   {"Gas giant with water based life", 3'000, 12'000, 1.5, 3.45},
   {"Gas giant with ammonia based life", 1'500, 6'000, 1.5, 3.45},
   {"Sudarsky class I gas giant", 1'650, 9'800, 1.5, 3.45},
   {"Sudarsky class II gas giant", int(9'650* 0.53), int(57'500 * 0.53), 1.5, 3.45},
   {"Sudarsky class III gas giant", 500, 3'000, 1.5, 3.45},
   {"Sudarsky class IV gas giant", 2'800, 16'000, 1.5, 3.45},
   {"Sudarsky class V gas giant", 3'100, 18'000, 1.5, 3.45},
   {"Helium rich gas giant", 3'000, 12'000, 1.5, 3.45},
   {"Helium gas giant", 500, 2'000, 1.5, 3.45}}
};

namespace exploration
  {
[[nodiscard]]
auto is_high_value_star(std::string_view star_class) noexcept -> bool;

[[nodiscard]]
auto extract_mass_code(std::string_view name) noexcept -> char;

[[nodiscard]]
auto system_approx_value(std::string_view star_class, std::string_view system_name) noexcept -> planet_value_e;
[[nodiscard]]
auto aprox_value(body_t const & body) noexcept -> sell_value_t;

  }  // namespace exploration
