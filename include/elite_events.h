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

enum struct event_e : uint16_t
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
  Scanned,
  Scan,
  NavBeaconScan,
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
  SupercruiseEntry,
  SuitLoadout,
  Backpack,
  ApproachSettlement,
  BuySuit,
  CreateSuitLoadout,
  SwitchSuitLoadout,
  Embark,
  Undocked,
  BookTaxi,
  ApproachBody,
  LeaveBody,
  DockingDenied,
  Touchdown,
  ShipTargeted,
  RepairAll,
  EscapeInterdiction,
  Disembark,
  PayFines,
  CodexEntry,
  CollectItems,
  BackpackChange,
  ShipyardSwap,
  Shipyard,
  UseConsumable,
  StoredShips,
  ShipyardTransfer,
  ShipyardBuy,
  CommitCrime,
  CrimeVictim,
  UnderAttack,
  TradeMicroResources,
  CollectCargo,
  RestockVehicle,
  LaunchSRV,
  SRVDestroyed,
  Bounty,
  DockSRV,
  ModuleRetrieve,
  BuyMicroResources,
  ShieldState,
  HullDamage,
  BuyAmmo,
  Liftoff,
  Died,
  MissionAccepted,
  MissionFailed,
  MissionAbandoned,
  MissionRedirected,
  MissionCompleted,
  MaterialDiscovered,
  MaterialCollected,
  CargoDepot,
  CrewAssign,
  Resurrect,
  Market,
  MarketSell,
  MarketBuy,
  HeatWarning,
  HeatDamage,
  EjectCargo,
  BuyWeapon,
  PayBounties,
  LoadoutEquipModule,
  ShipyardNew,
  USSDrop,
  Interdicted,
  FetchRemoteModule,
  ModuleSellRemote,
  EngineerCraft,
  SearchAndRescue,
  BuyDrones,
  SellDrones,
  LaunchDrone,
  LaunchFighter,
  DockFighter,
  MiningRefined,
  MaterialTrade,
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
  NavBeaconDetail,
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
  std::string event;
  std::optional<scan_type_e> ScanType;
  };

using utc_time_point_t = std::chrono::sys_time<std::chrono::milliseconds>;

[[nodiscard]]
auto parse_timestamp_t(std::string_view input) -> std::optional<utc_time_point_t>;

struct fuel_scoop_t
  {
  float Scooped;
  float Total;
  };

struct fuel_capacity_t
  {
  float Main;
  float Reserve;
  };

struct module_t
  {
  std::string Slot;
  std::string Item;  //": "mandalay_armour_grade1",
  bool On;
  uint8_t Priority;
  float Health;
  };

struct loadout_t
  {
  std::string Ship;
  uint32_t ShipID;
  std::string ShipName;
  std::string ShipIdent;
  uint32_t HullValue;
  uint32_t ModulesValue;
  float HullHealth;
  float UnladenMass;
  uint16_t CargoCapacity;
  float MaxJumpRange;
  uint32_t Rebuy;
  fuel_capacity_t FuelCapacity;
  std::vector<module_t> Modules;
  };

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
  return simple_enum::adl_info{Hyperspace, Supercruise};
  }

///\brief When written: at the start of a Hyperspace or Supercruise jump (start of countdown)
struct start_jump_t
  {
  std::chrono::sys_seconds timestamp;
  jump_type_e JumpType;
  std::optional<std::string> StarSystem;
  std::optional<uint64_t> SystemAddress;
  std::optional<std::string> StarClass;
  std::optional<bool> Taxi;
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

struct body_location_t
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
  constexpr auto player_position() const noexcept -> body_location_t
    {
    return body_location_t{{}, StarPos[0], StarPos[1], StarPos[2]};
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
  uint64_t Population;

  bool Wanted;
  std::vector<faction_info_t> Factions;
  };

struct location_t
  {
  bool Docked;
  bool Taxi;
  bool Multicrew;
  std::string StarSystem;
  uint64_t SystemAddress;
  std::array<double, 3> StarPos;
  std::string SystemAllegiance;
  std::string SystemEconomy_Localised;
  std::string SystemSecondEconomy_Localised;
  std::string SystemGovernment_Localised;
  std::string SystemSecurity_Localised;
  uint64_t Population;
  std::string Body;
  body_id_t BodyID;
  std::string BodyType;
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
  saa_scan_complete_t,
  fuel_scoop_t,
  loadout_t,
  location_t>;

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
  };

struct star_system_t
  {
  uint64_t system_address;
  std::string name;
  std::string star_type;
  // absolute location in galaxy in LY
  // X	East / West	Wartości dodatnie rosną w prawo od Sol (patrząc na mapę z góry).
  // Y	Up / Down	Wysokość nad lub pod płaszczyzną galaktyki. Sol leży prawie na 0.
  // Z	North / South
  std::array<double, 3> system_location;
  std::vector<bary_centre_t> bary_centre;
  std::vector<body_t> bodies;
  uint8_t sub_class;
  bool fss_complete;

  [[nodiscard]]
  auto body_by_id(this auto && self, events::body_id_t const body_id) noexcept
    {
    return std::ranges::find(self.bodies, body_id, body_body_id_proj);
    }
  };

struct state_t
  {
  star_system_t system;
  events::fsd_jump_t jump_info;
  };

struct generic_state_t
  {
  virtual ~generic_state_t();
  auto discovery(std::string_view input) -> void;
  virtual auto handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) -> void = 0;
  };

struct discovery_state_t : public generic_state_t
  {
  state_t * state;
  void handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) override;
  };

struct planet_value_info_t
  {
  std::string_view planet_class;
  double base_value;
  double terraform_bonus{0.0};
  };

auto body_short_name(std::string_view system, std::string_view name) -> std::string_view;

static constexpr std::array<planet_value_info_t, 19> exploration_values{
  {{"Metal rich body", 21'790.0},
   {"High metal content body", 9'693.0, 93'328.0},  // Bonus dodawany jeśli terraformowalna
   {"Rocky body", 300.0, 93'328.0},
   {"Icy body", 300.0},
   {"Rocky ice body", 300.0},
   {"Earthlike body", 64'831.0 + 116'295.0},  // Earth-like jest zawsze "terraformowana" z definicji bazy
   {"Water world", 24'831.0, 116'295.0},
   {"Ammonia world", 33'268.0},
   {"Water giant", 1'000.0},
   {"Water giant with life", 1'500.0},
   {"Gas giant with water based life", 3'000.0},
   {"Gas giant with ammonia based life", 1'500.0},
   {"Sudarsky class I gas giant", 1'650.0},
   {"Sudarsky class II gas giant", 9'650.0},
   {"Sudarsky class III gas giant", 500.0},
   {"Sudarsky class IV gas giant", 2'800.0},
   {"Sudarsky class V gas giant", 3'100.0},
   {"Helium rich gas giant", 3'000.0},
   {"Helium gas giant", 500.0}}
};

struct ship_loadout_t
  {
  std::string Ship;
  uint32_t ShipID;
  std::string ShipName;
  std::string ShipIdent;
  float HullHealth;
  uint16_t CargoCapacity;
  uint16_t CargoUsed;
  events::fuel_capacity_t FuelCapacity;
  float FuelLevel;
  std::vector<events::module_t> Modules;
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
auto aprox_value(body_t const & body) noexcept -> uint32_t;
[[nodiscard]]
auto calculate_value(
  planet_value_info_t const & info,
  double mass_em,
  bool is_terraformable,
  bool is_first_discoverer,
  bool is_first_mapper,
  bool efficiency_bonus
) -> uint32_t;

[[nodiscard]]
constexpr auto get_star_icon(std::string_view star_type) -> std::string_view
  {
  using namespace std::literals;

  if(star_type.starts_with("D"sv))
    return "⚪"sv;  // White Dwarfs
  if(star_type == "Neutron"sv)
    return "⚡"sv;  // Neutron Stars
  if(star_type == "BlackHole"sv)
    return "🕳"sv;  // Black Holes

  if(star_type.find("Giant"sv) != std::string_view::npos)
    return "✺"sv;

  if(star_type.starts_with("L"sv) or star_type.starts_with("T"sv) or star_type.starts_with("Y"sv))
    return "🌑"sv;

  // (KGBFOAM)
  return "☀"sv;
  }

[[nodiscard]]
constexpr auto get_planet_icon(std::string_view planet_class) -> std::string_view
  {
  using namespace std::literals;

  if(planet_class == "Earthlike body"sv)
    return "🌎"sv;
  if(planet_class.contains("Water world"sv))
    return "💧"sv;
  if(planet_class == "Ammonia world"sv)
    return "☣"sv;

  if(planet_class == "Metal rich body"sv)
    return "◈"sv;
  if(planet_class == "High metal content body"sv)
    return "🔘"sv;

  if(planet_class.contains("gas giant"sv))
    return "◎"sv;

  if(planet_class.contains("Icy"sv))
    return "❄"sv;

  if(planet_class == "Rocky body"sv)
    return "●"sv;

  return "○"sv;
  }
  }  // namespace exploration

[[nodiscard]]
auto format_credits_value(uint32_t value) -> std::string;
