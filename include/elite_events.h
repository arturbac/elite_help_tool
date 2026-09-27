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
  JetConeBoost,
  StartJump,
  ReceiveText,
  FSSDiscoveryScan,
  FSSBodySignals,
  FSSAllBodiesFound,
  FSSSignalDiscovered,
  DiscoveryScan,
  Scanned,
  ScanOrganic,
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
  GameModeChange,
  Friends,
  SellMicroResources,
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

  ApproachSettlement,
  DockingRequested,
  DockingGranted,
  Docked,
  Market,
  ColonisationConstructionDepot,
  ColonisationContribution,

  Promotion,
  SupercruiseEntry,
  SuitLoadout,
  Backpack,

  BuySuit,
  CreateSuitLoadout,
  SwitchSuitLoadout,
  Embark,
  Undocked,
  BookTaxi,
  CancelTaxi,
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

  MissionAbandoned,
  MissionAccepted,
  MissionCompleted,
  MissionFailed,
  MissionRedirected,

  MaterialDiscovered,
  MaterialCollected,
  CargoDepot,
  CrewAssign,
  Resurrect,

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
  FighterDestroyed,
  FighterRebuilt,
  NpcCrewRank,
  DockingCancelled,
  MiningRefined,
  MaterialTrade,

  CarrierStats,
  FCMaterials,
  CarrierLocation,
  CargoTransfer,
  CarrierJumpRequest,
  SquadronStartup,
  
  FactionKillBond,
  BookDropship,
  DropshipDeploy,
  ColonisationSystemClaim,
  ColonisationSystemClaimRelease,
  CarrierJumpCancelled,
  CarrierJump,
  
  NavRoute,
  NavRouteClear
  };

enum struct station_type : uint8_t
  {
  AsteroidBase,
  Bernal,
  Coriolis,
  CraterOutpost,
  CraterPort,
  DockablePlanetStation,
  Dodec,
  FleetCarrier,
  GameplayPOI,
  MegaShip,
  Ocellus,
  OnFootSettlement,
  Orbis,
  Outpost,
  PlanetaryConstructionDepot,
  SpaceConstructionDepot,
  SurfaceStation
  };

consteval auto adl_enum_bounds(station_type)
  {
  using enum station_type;
  return simple_enum::adl_info{AsteroidBase, SurfaceStation};
  }
enum struct landing_pad_size_t : uint8_t
  {
  none,
  small,
  medium,
  large
  };

consteval auto adl_enum_bounds(landing_pad_size_t)
  {
  using enum landing_pad_size_t;
  return simple_enum::adl_info{none, large};
  }

struct docking_requested_t
  {
  uint64_t MarketID;
  std::string StationName;
  station_type StationType;
  };

struct cargo_t
  {
  ///\brief Ship or SRV - only what stays on the ship counts
  std::string Vessel;
  uint32_t Count;
  };

struct carrier_space_sage_t
  {
  uint32_t TotalCapacity;
  uint32_t Crew;
  uint32_t Cargo;
  uint32_t CargoSpaceReserved;
  uint32_t ShipPacks;
  uint32_t ModulePacks;
  uint32_t FreeSpace;
  };

struct carrier_finance_t
  {
  uint64_t CarrierBalance;
  uint64_t ReserveBalance;
  uint64_t AvailableBalance;
  uint8_t ReservePercent;
  uint8_t TaxRate_refuel;
  };

struct carrier_stats_t
  {
  ///\brief a number, not the callsign - the same as this carrier's MarketID. FCMaterials.json names
  /// its own field the same way but keeps the callsign in it, so the two sources may be joined
  /// through Callsign alone
  uint64_t CarrierID;
  std::string Callsign;
  std::string Name;
  std::string CarrierType;
  std::string DockingAccess;
  uint16_t FuelLevel;
  double JumpRangeCurr;
  double JumpRangeMax;

  carrier_space_sage_t SpaceUsage;
  carrier_finance_t Finance;
  };
  
struct fcmaterial_t
{
  uint64_t id;
  ///\brief the internal name, the key shared with micro resource sales
  std::string Name;
  std::string Name_Localised;
  uint32_t Price;
  uint32_t Stock;
  uint32_t Demand;
};

struct fcmaterials_t
{
  std::chrono::sys_seconds timestamp;
  uint64_t MarketID;
  std::string CarrierName;
  std::string CarrierID;
  std::vector<fcmaterial_t> Items;
};

struct nav_route_t
  {
  struct item_t
    {
    std::string StarSystem;
    uint64_t SystemAddress;
    std::array<double, 3> StarPos;
    std::string StarClass;
    };

  std::vector<nav_route_t::item_t> Route;
  };

struct nav_route_clear_t
  {
  };

struct mission_abandoned_t
  {
  uint64_t MissionID;
  };

struct mission_accepted_t
  {
  uint64_t MissionID;
  std::chrono::sys_seconds Expiry;
  std::string Faction;
  std::string Name;  // Mission_Massacre*
  std::string LocalisedName;
  std::string Target;                 //: name of target; //
  std::string TargetType_Localised;   // "Pirates"
  std::string TargetFaction;          // "Anana Brotherhood"
  std::string DestinationSystem;      //": "Anana",
  std::string DestinationStation;     //": "Yamazaki Base",
  std::string DestinationSettlement;  //
  std::string Commodity;              //: commodity type
  std::string Commodity_Localised;    //: the readable name, the same one the commodity dictionary carries
  uint32_t Count;                     //: number required / to deliver
  std::string Donation;               //: contracted donation (as string) (for altruism missions)
  int32_t Donated;                    //: actual donation (as int)
  uint16_t PassengerCount;
  bool PassengerVIPs;         //
  bool PassengerWanted;       //
  std::string PassengerType;  //: eg Tourist, Soldier, Explorer,...
  /// expected cash reward
  uint64_t Reward;  //": 2454378,
  bool Wing;
  uint16_t KillCount;  //
  };

///\brief a material reward for a mission - it goes to the locker, it never shows in the backpack
struct material_reward_t
  {
  std::string Name;
  std::string Category_Localised;
  uint32_t Count;
  };

///\brief the influence one faction gained in one system, paid for by this mission
///\detail the game gives no number, only a run of "+", "++" or "+++" - its length is the whole measure.
/// Trend says which way the faction went: UpGood rises, DownBad falls
struct influence_effect_t
  {
  uint64_t SystemAddress;
  std::string Trend;
  std::string Influence;
  };

///\brief one mission's effect on one faction - a handed-in mission usually moves several factions at once,
/// and each of them may feel it in more than one system
struct faction_effect_t
  {
  std::string Faction;
  std::vector<influence_effect_t> Influence;
  };

struct mission_completed_t
  {
  uint64_t MissionID;
  ///\brief the actual payout, different from the one promised when the mission was taken
  uint64_t Reward;
  ///\brief material rewards - the single largest source of them, twice as large as data ports
  std::vector<material_reward_t> MaterialsReward;
  ///\brief who the mission moved and by how much - the only trace of a handed-in mission's influence in the whole journal.
  /// Without it there is no saying how much work a percentage point cost in the system
  std::vector<faction_effect_t> FactionEffects;
  };

struct mission_failed_t
  {
  uint64_t MissionID;
  };

struct mission_redirected_t
  {
  uint64_t MissionID;
  std::string NewDestinationStation;
  std::string NewDestinationSystem;
  std::string NewDestinationSettlement;
  };

///\brief who is playing this session - every journal opens with this event right after the header
///\detail the FID is the account's fixed identifier while the name can change - so the FID decides
struct commander_t
  {
  std::string FID;
  std::string Name;
  };

///\brief a row from the list of missions the game considers open
struct mission_active_t
  { uint64_t MissionID; };

struct missions_t
  {
  ///\brief the only trustworthy source on what is still open - the rest is our guesswork
  std::vector<mission_active_t> Active;
  std::vector<mission_failed_t> Failed;
  std::vector<mission_completed_t> Complete;
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
  Detailed,
  // ScanType also appears in Scanned and ScanOrganic; an unknown value used to break the parsing of
  // generic_event_t, that is of the whole journal line, not only of those events
  Cargo,
  Crime,
  Log,
  Sample,
  Analyse
  };

consteval auto adl_enum_bounds(scan_type_e)
  {
  using enum scan_type_e;
  return simple_enum::adl_info{AutoScan, Analyse};
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

// Allied	+75 do +100
// Friendly	+35 do +74
// Cordial	+4 do +34
// Neutral	-3 do +3
// Unfriendly	-34 do -4
// Hostile	-100 do -35
///\brief an entry in a faction's state list; Trend appears on only some of them
struct faction_state_entry_t
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

  std::vector<faction_state_entry_t> PendingStates;
  std::vector<faction_state_entry_t> ActiveStates;
  std::vector<faction_state_entry_t> RecoveringStates;
  };

///\brief one side of a conflict in the system
struct conflict_faction_t
  {
  std::string Name;
  std::string Stake;
  uint32_t WonDays;
  };

///\brief a war, a civil war or an election in the system
struct conflict_t
  {
  std::string WarType;
  std::string Status;
  conflict_faction_t Faction1;
  conflict_faction_t Faction2;
  };

struct system_faction_t
  {
  std::string Name;
  std::string FactionState;  // volatile
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
  int BoostUsed;
  bool Taxi;
  bool Multicrew;

  [[nodiscard]]
  constexpr auto player_position() const noexcept -> body_location_t
    { return body_location_t{{}, StarPos[0], StarPos[1], StarPos[2]}; }

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
  std::vector<conflict_t> Conflicts;
  };
/*
{ 
"timestamp":"2026-09-04T01:49:45Z",
"event":"FSDJump",
"Taxi":false,
"Multicrew":false,
"StarSystem":"M25 Sector PI-T d3-51",
"SystemAddress":1762740111931,
"StarPos":[-419.03125,-153.18750,2099.09375],
"SystemAllegiance":"",
"SystemEconomy":"$economy_None;",
"SystemEconomy_Localised":"None",
"SystemSecondEconomy":"$economy_None;",
"SystemSecondEconomy_Localised":"None",
"SystemGovernment":"$government_None;",
"SystemGovernment_Localised":"None",
"SystemSecurity":"$GAlAXY_MAP_INFO_state_anarchy;",
"SystemSecurity_Localised":"Anarchy",
"Population":0,
"Body":"M25 Sector PI-T d3-51",
"BodyID":0,
"BodyType":"Star",
"JumpDist":454.287,
"FuelUsed":6.287253,
"FuelLevel":38.012375,
"BoostUsed":4
  }

 */
struct location_t
  {
  system_faction_t SystemFaction;
  bool Docked;
  ///\brief started on foot - in a station's concourse the game gives no StationName, only Body
  bool OnFoot{};
  ///\brief the port, only when Docked
  std::string StationName;
  uint64_t MarketID{};
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

  std::vector<faction_info_t> Factions;
  std::vector<conflict_t> Conflicts;
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

struct signal_t
  {
  std::string Type_Localised;
  uint16_t Count;
  };

struct genus_t
  {
  std::string Genus_Localised;
  ///\brief filled in only after sampling; mapping alone gives just the genus
  std::string Species_Localised;
  ///\brief the sample is complete - the game ends the Log, Sample, Sample, Analyse sequence with that last one
  bool Sampled;
  };

///\brief pobranie probki organicznej, dopowiada gatunek do rodzaju znanego z mapowania
struct scan_organic_t
  {
  scan_type_e ScanType;
  std::string Genus_Localised;
  std::string Species_Localised;
  std::string Variant_Localised;
  ///\brief this commander has logged the species before - the codex entry is not new to them
  bool WasLogged;
  uint64_t SystemAddress;
  body_id_t Body;
  };

///\brief a signal found by an FSS scan - a station, an installation, a POI, a phenomenon
///\brief approaching a settlement - this is where the context for collecting comes from: the economy and government of the place
struct approach_settlement_t
  {
  uint64_t MarketID;
  uint64_t SystemAddress;
  std::string Name;
  ///\brief the body the settlement stands on
  std::optional<body_id_t> BodyID;
  std::string StationEconomy_Localised;
  std::string StationGovernment_Localised;
  ///\brief the faction holding the place - the journal gives it nested, not as a bare string
  system_faction_t StationFaction;
  };

///\brief leaving the vehicle - sometimes the only trace of the place when the arrival was by taxi
struct disembark_t
  {
  uint64_t MarketID;
  uint64_t SystemAddress;
  std::string StationName;
  std::string StationType;
  };

///\brief entering supercruise - it ends the stay at a settlement
struct supercruise_entry_t
  {
  uint64_t SystemAddress;
  };

///\brief a kill in a conflict zone - who pays for it and whom it was against. The reward also says the
/// intensity of a zone on foot: the game pays from fixed tables, one per intensity
struct faction_kill_bond_t
  {
  uint64_t Reward{};
  std::string AwardingFaction;
  std::string VictimFaction;
  };

///\brief a Frontline Solutions dropship booked - to the settlement of a conflict zone, or back from one
struct book_dropship_t
  {
  bool Retreat{};
  std::string DestinationSystem;
  std::string DestinationLocation;
  };

///\brief the dropship has set the commander down, on foot, on the body of the settlement booked
struct dropship_deploy_t
  {
  uint64_t SystemAddress{};
  std::string Body;
  std::optional<body_id_t> BodyID;
  };

///\brief boarding the ship, the SRV or a taxi - the commander is no longer on foot
struct embark_t
  {
  uint64_t SystemAddress{};
  bool SRV{};
  bool Taxi{};
  };

///\brief the commander died - on foot no longer, wherever they wake up
struct died_t
  {
  };

///\brief one commodity a construction site needs, and how much of it has come
struct construction_resource_t
  {
  std::string Name;
  std::string Name_Localised;
  uint32_t RequiredAmount{};
  uint32_t ProvidedAmount{};
  uint32_t Payment{};
  };

///\brief the whole state of a construction site - written on docking at it, and again every little while
struct colonisation_construction_depot_t
  {
  uint64_t MarketID{};
  double ConstructionProgress{};
  bool ConstructionComplete{};
  bool ConstructionFailed{};
  std::vector<construction_resource_t> ResourcesRequired;
  };

struct construction_contribution_t
  {
  std::string Name;
  std::string Name_Localised;
  uint32_t Amount{};
  };

///\brief cargo handed in at a construction site
struct colonisation_contribution_t
  {
  uint64_t MarketID{};
  std::vector<construction_contribution_t> Contributions;
  };

///\brief a carrier's jump ordered - where to and when it leaves; one's own carrier or the squadron's
struct carrier_jump_request_t
  {
  std::string CarrierType;
  uint64_t CarrierID{};
  std::string SystemName;
  std::string Body;
  uint64_t SystemAddress{};
  std::chrono::sys_seconds DepartureTime;
  };

///\brief where a carrier is - written at login, and a minute after a jump when in the game
struct carrier_location_t
  {
  std::string CarrierType;
  uint64_t CarrierID{};
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

struct carrier_jump_cancelled_t
  {
  std::string CarrierType;
  uint64_t CarrierID{};
  };

///\brief a carrier's jump seen from aboard it - the carrier is the station docked at
struct carrier_jump_t
  {
  std::string StarSystem;
  uint64_t SystemAddress{};
  std::string Body;
  std::string StationType;
  uint64_t MarketID{};
  };

///\brief changing ships at a shipyard - the ship left behind keeps its hold
struct shipyard_swap_t
  {
  std::string ShipType;
  uint64_t MarketID{};
  };

///\brief waking up again - after a death, or leaving by escape pod ("escape"), which leaves no death behind
struct resurrect_t
  {
  std::string Option;
  };

///\brief a system claimed for colonisation - by the commander of the session
struct colonisation_system_claim_t
  {
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

///\brief a claim given up
struct colonisation_system_claim_release_t
  {
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

///\brief a backpack row
struct backpack_item_t
  {
  std::string Name;
  std::string Name_Localised;
  ///\brief Data, Item, Component or Consumable
  std::string Type;
  uint32_t Count;
  };

///\brief a change to the backpack's contents - Added is a find from a settlement, a data port or a mission
struct backpack_change_t
  {
  std::vector<backpack_item_t> Added;
  };

///\brief a micro resource sale row
struct sold_micro_resource_t
  {
  std::string Name;
  std::string Name_Localised;
  std::string Category;
  uint32_t Count;
  };

///\brief selling micro resources to a bartender - at a station or on a player's carrier
struct sell_micro_resources_t
  {
  uint64_t MarketID;
  uint64_t Price;
  uint32_t TotalCount;
  std::vector<sold_micro_resource_t> MicroResources;
  };

///\brief docking - this is where a station's identity comes from, its type included
///\detail StationType tells a player's carrier from an ordinary station, which is what tells selling
/// micro resources to players from dropping them at a station
struct docked_t
  {
  uint64_t MarketID;
  uint64_t SystemAddress;
  std::string StationName;
  std::string StationType;
  std::string StarSystem;
  std::string StationEconomy_Localised;
  std::string StationGovernment_Localised;
  ///\brief the faction holding the place - the journal gives it nested, not as a bare string
  system_faction_t StationFaction;
  double DistFromStarLS;
  };

///\brief an order to move a ship between ports
///
/// The one moment the game says when the ship will arrive - afterwards it never mentions it again,
/// and the arrival itself has no event of its own
struct shipyard_transfer_t
  {
  std::string ShipType;
  std::string ShipType_Localised;
  uint64_t ShipID;
  ///\brief the system the ship flies from
  std::string System;
  ///\brief the market it flies from - for a carrier that is its MarketID
  uint64_t ShipMarketID;
  double Distance;
  uint64_t TransferPrice;
  ///\brief delivery time in seconds
  uint64_t TransferTime;
  ///\brief the destination market, that is the one we are standing in
  uint64_t MarketID;
  };

///\brief lifting off the pad - from this moment the market of that place stops concerning us
struct undocked_t
  {
  uint64_t MarketID;
  std::string StationName;
  };

///\brief the Market event from the journal - it carries only a header, the contents go to Market.json
struct market_t
  {
  uint64_t MarketID;
  std::string StationName;
  std::string StationType;
  std::string StarSystem;
  };

///\brief a row from Cargo.json
struct cargo_item_t
  {
  std::string Name;
  std::string Name_Localised;
  uint32_t Count;
  uint32_t Stolen;
  };

///\brief the contents of Cargo.json, a file overwritten on every change of cargo
struct cargo_file_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Vessel;
  uint32_t Count;
  std::vector<cargo_item_t> Inventory;
  };

///\brief a market row from Market.json
struct market_commodity_t
  {
  uint64_t id;
  std::string Name_Localised;
  std::string Category_Localised;
  uint32_t BuyPrice;
  uint32_t SellPrice;
  ///\brief the galactic average, a constant of the commodity - the reference point for a price
  uint32_t MeanPrice;
  uint32_t Stock;
  uint32_t Demand;
  ///\brief a price alone means nothing - a station quotes one for commodities it does not trade either
  bool Producer;
  bool Consumer;
  };

///\brief the contents of Market.json, a file overwritten at every docking
///\brief what the game is showing right now, from the Status.json it keeps beside the journals
///\detail the file is rewritten whenever anything in it changes, so it answers questions the journal
/// never does - among them which interface is open, which no amount of reading events can tell
struct status_file_t
  {
  std::chrono::sys_seconds timestamp;
  uint64_t Flags;
  ///\brief 0 none, 1-4 the cockpit panels, 5 station services, 6 galaxy map, 7 system map,
  /// 8 orrery, 9 FSS, 10 surface scanner, 11 codex
  uint32_t GuiFocus;
  ///\brief the body we are near or on, by its full name - absent in open space; on foot in an orbital
  /// station it is the station's name
  std::string BodyName;
  ///\brief where the ship is set to go - only the system when it lies elsewhere, the station and the
  /// body it is on once inside the same system
  struct destination_t
    {
    uint64_t System;
    uint32_t Body;
    std::string Name;
    };
  std::optional<destination_t> Destination;
  ///\brief on foot, in a taxi, in a hangar - the ship's Flags know nothing of the commander's own legs
  uint64_t Flags2;
  ///\brief the place on the body's surface, present only near one or on it - degrees, altitude in metres
  std::optional<double> Latitude;
  std::optional<double> Longitude;
  std::optional<double> Altitude;
  std::optional<double> Heading;
  ///\brief in metres; what turns two points in degrees into a walk in metres
  std::optional<double> PlanetRadius;
  ///\brief what the commander holds on foot - the genetic sampler says a sample is being taken
  std::string SelectedWeapon;
  ///\brief the commander's standing with the law in this system - Clean, Wanted, Hostile, Speeding,
  /// IllegalCargo, PassengerWanted, Warrant. The only word of bounties the game gives: their sums are
  /// in no file, and a squadron's Notoriety Decay changes them without a trace
  std::string LegalState;
  };

struct market_file_t
  {
  std::chrono::sys_seconds timestamp;
  uint64_t MarketID;
  std::string StationName;
  std::string StationType;
  std::string StarSystem;
  std::vector<market_commodity_t> Items;
  };

struct fss_signal_discovered_t
  {
  uint64_t SystemAddress;
  std::string SignalName;
  std::string SignalName_Localised;
  std::string SignalType;
  bool IsStation;
  ///\brief present only for USS signals, which expire after a few minutes
  std::optional<double> TimeRemaining;
  };

struct fss_body_signals_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  std::vector<signal_t> Signals;
  };

struct dss_body_signals_t
  {
  std::string BodyName;
  body_id_t BodyID;
  uint64_t SystemAddress;
  std::vector<signal_t> Signals;
  std::vector<genus_t> Genuses;
  };

// { "event":"SAASignalsFound", "BodyName":"Fedgau MY-G d11-4 1", "SystemAddress":149191313507, "BodyID":1,
// "Signals":[ { "Type":"$SAA_SignalType_Biological;", "Type_Localised":"Biological", "Count":1 } ],
// "Genuses":[ { "Genus":"$Codex_Ent_Bacterial_Genus_Name;", "Genus_Localised":"Bacterium" } ] }

struct fss_all_bodies_found_t
  {
  std::string SystemName;
  uint64_t SystemAddress;
  uint32_t Count;
  };

///\brief the ship under the crosshairs, as the scan uncovers it stage by stage
///\detail the stages are cumulative and each adds a field: 0 the hull type, 1 the pilot and their
/// rank, 2 the health, 3 the faction, the legal status and the price on their head. A target let go
/// arrives as the same event with TargetLocked false and nothing else in it
struct ship_targeted_t
  {
  std::chrono::sys_seconds timestamp;
  bool TargetLocked;
  std::string Ship;
  ///\brief only when it differs from the internal name
  std::string Ship_Localised;
  int ScanStage{-1};
  std::string PilotName;
  std::string PilotName_Localised;
  std::string PilotRank;
  ///\brief per cent, not a fraction
  double ShieldHealth;
  double HullHealth;
  std::string Faction;
  ///\brief Wanted, Clean, Lawless, Hunter, None
  std::string LegalStatus;
  ///\brief the price on their head, absent when there is none
  uint64_t Bounty;
  std::string Subsystem;
  std::string Subsystem_Localised;
  double SubsystemHealth;
  };

///\brief one faction's share of a kill
struct bounty_reward_t
  {
  std::string Faction;
  uint64_t Reward;
  };

///\brief a kill that pays, with the factions that will pay for it
///\detail who issued the warrant is known only here - the scan before the kill says there is a price
/// but never whose it is, so the systems the money can be claimed in follow from this event alone
struct bounty_t
  {
  std::chrono::sys_seconds timestamp;
  std::vector<bounty_reward_t> Rewards;
  std::string PilotName;
  std::string PilotName_Localised;
  std::string Target;
  std::string Target_Localised;
  uint64_t TotalReward;
  std::string VictimFaction;
  };

///\brief the fighter going out, and whether anyone of ours is flying it
struct launch_fighter_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Loadout;
  uint32_t ID;
  ///\brief false when the hired pilot has it, which is the case worth saying out loud
  bool PlayerControlled;
  };

///\brief the fighter called back in
struct dock_fighter_t
  {
  std::chrono::sys_seconds timestamp;
  uint32_t ID;
  };

///\brief the fighter shot out from under the pilot
struct fighter_destroyed_t
  {
  std::chrono::sys_seconds timestamp;
  uint32_t ID;
  };

///\brief the hangar has built another one
struct fighter_rebuilt_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Loadout;
  uint32_t ID;
  };

///\brief who of the hired crew is on duty
struct crew_assign_t
  {
  std::chrono::sys_seconds timestamp;
  std::string Name;
  uint64_t CrewID;
  ///\brief Active is the one who flies the fighter
  std::string Role;
  };

///\brief how good the hired pilot has become - they rank up by fighting
struct npc_crew_rank_t
  {
  std::chrono::sys_seconds timestamp;
  std::string NpcCrewName;
  uint64_t NpcCrewId;
  uint32_t RankCombat;
  };

using event_holder_t = std::variant<
  fsd_jump_t,
  fsd_target_t,
  start_jump_t,
  fss_discovery_scan_t,
  fss_body_signals_t,
  fss_signal_discovered_t,
  market_t,
  undocked_t,
  docked_t,
  shipyard_transfer_t,
  sell_micro_resources_t,
  approach_settlement_t,
  disembark_t,
  supercruise_entry_t,
  backpack_change_t,
  fss_all_bodies_found_t,
  scan_bary_centre_t,
  scan_detailed_scan_t,
  saa_scan_complete_t,
  dss_body_signals_t,
  scan_organic_t,
  fuel_scoop_t,
  loadout_t,
  location_t,
  mission_accepted_t,
  mission_abandoned_t,
  mission_completed_t,
  mission_failed_t,
  mission_redirected_t,
  missions_t,
  nav_route_t,
  nav_route_clear_t,
  cargo_t,
  carrier_stats_t,
  fcmaterials_t,
  ship_targeted_t,
  bounty_t,
  launch_fighter_t,
  dock_fighter_t,
  fighter_destroyed_t,
  fighter_rebuilt_t,
  crew_assign_t,
  npc_crew_rank_t,
  commander_t,
  faction_kill_bond_t,
  book_dropship_t,
  dropship_deploy_t,
  embark_t,
  died_t,
  colonisation_construction_depot_t,
  colonisation_contribution_t,
  colonisation_system_claim_t,
  colonisation_system_claim_release_t,
  carrier_jump_request_t,
  carrier_location_t,
  carrier_jump_cancelled_t,
  carrier_jump_t,
  shipyard_swap_t,
  resurrect_t>;

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
  ///\brief what the star orbits - with two stars or more it is a barycentre shared with its partner,
  /// which is what pairs them in a picture of the system; empty for rows written before it was kept
  std::optional<events::body_id_t> parent_star;
  std::optional<events::body_id_t> parent_barycenter;
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
auto filter_current_visit(std::vector<system_signal_t> signals) -> std::vector<system_signal_t>;

[[nodiscard]]
auto to_system_signal(events::fss_signal_discovered_t const & signal, std::chrono::sys_seconds seen)
  -> system_signal_t;

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

struct generic_state_t
  {
  std::string journal_dir_path_;

  generic_state_t(std::string_view journal_dir_path) : journal_dir_path_{journal_dir_path} {}

  virtual ~generic_state_t();

  auto discovery(std::string_view input) -> void;
  virtual auto handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) -> void = 0;
  ///\brief every line as the game wrote it, before any of it is read - for what needs the event whole
  virtual auto raw_line(std::string_view) -> void {}
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

struct planet_value_info_t
  {
  std::string_view planet_class;
  double base_value;
  double terraform_bonus{0.0};
  };

///\brief the value of an organic sample after analysis, in credits
struct organic_value_t
  {
  std::string_view species;
  uint32_t value;
  };

///\brief the species price list, names as in Species_Localised of the ScanOrganic event
static constexpr std::array<organic_value_t, 96> organic_values{
  {
   {"Aleoida Arcus", 7'252'500},
   {"Aleoida Coronamus", 6'284'600},
   {"Aleoida Gravis", 12'934'900},
   {"Aleoida Laminiae", 3'385'200},
   {"Aleoida Spica", 3'385'200},
   {"Amphora plant", 1'628'800},
   {"Anemone", 1'499'900},
   {"Bacterium Acies", 1'000'000},
   {"Bacterium Alcyoneum", 1'658'500},
   {"Bacterium Aurasus", 1'000'000},
   {"Bacterium Bullaris", 1'152'500},
   {"Bacterium Cerbrus", 1'689'800},
   {"Bacterium Informem", 8'418'000},
   {"Bacterium Nebulus", 5'289'900},
   {"Bacterium Omentum", 4'638'900},
   {"Bacterium Scopulum", 4'934'500},
   {"Bacterium Tela", 1'949'000},
   {"Bacterium Verrata", 3'897'000},
   {"Bacterium Vesicula", 1'000'000},
   {"Bacterium Volu", 7'774'700},
   {"Bark Mounds", 1'471'900},
   {"Brain Tree", 1'593'700},
   {"Cactoida Cortexum", 3'667'600},
   {"Cactoida Lapis", 2'483'600},
   {"Cactoida Peperatis", 2'483'600},
   {"Cactoida Pullulanta", 3'667'600},
   {"Cactoida Vermis", 16'202'800},
   {"Clypeus Lacrimam", 8'418'000},
   {"Clypeus Margaritus", 11'873'200},
   {"Clypeus Speculumi", 16'202'800},
   {"Concha Aureolas", 7'774'700},
   {"Concha Biconcavis", 19'010'800},
   {"Concha Labiata", 2'352'400},
   {"Concha Renibus", 4'572'400},
   {"Crystalline Shards", 1'628'800},
   {"Electricae Pluma", 6'284'600},
   {"Electricae Radialem", 6'284'600},
   {"Fonticulua Campestris", 1'000'000},
   {"Fonticulua Digitos", 1'804'100},
   {"Fonticulua Fluctus", 20'000'000},
   {"Fonticulua Lapida", 3'111'000},
   {"Fonticulua Segmentatus", 19'010'800},
   {"Fonticulua Upupam", 5'727'600},
   {"Frutexa Acus", 7'774'700},
   {"Frutexa Collum", 1'639'800},
   {"Frutexa Fera", 1'632'500},
   {"Frutexa Flabellum", 1'808'900},
   {"Frutexa Flammasis", 10'326'000},
   {"Frutexa Metallicum", 1'632'500},
   {"Frutexa Sponsae", 5'988'000},
   {"Fumerola Aquatis", 6'284'600},
   {"Fumerola Carbosis", 6'284'600},
   {"Fumerola Extremus", 16'202'800},
   {"Fumerola Nitris", 7'500'900},
   {"Fungoida Bullarum", 3'703'200},
   {"Fungoida Gelata", 3'330'300},
   {"Fungoida Setisis", 1'670'100},
   {"Fungoida Stabitis", 2'680'300},
   {"Osseus Cornibus", 1'483'000},
   {"Osseus Discus", 12'934'900},
   {"Osseus Fractus", 4'027'800},
   {"Osseus Pellebantus", 9'739'000},
   {"Osseus Pumice", 3'156'300},
   {"Osseus Spiralis", 2'404'700},
   {"Recepta Conditivus", 14'313'700},
   {"Recepta Deltahedronix", 16'202'800},
   {"Recepta Umbrux", 12'934'900},
   {"Sinuous Tubers", 1'514'500},
   {"Stratum Araneamus", 2'448'900},
   {"Stratum Cucumisis", 16'202'800},
   {"Stratum Excutitus", 2'448'900},
   {"Stratum Frigus", 2'637'500},
   {"Stratum Laminamus", 2'788'300},
   {"Stratum Limaxus", 1'362'000},
   {"Stratum Paleas", 1'362'000},
   {"Stratum Tectonicas", 19'010'800},
   {"Tubus Cavas", 11'873'200},
   {"Tubus Compagibus", 7'774'700},
   {"Tubus Conifer", 2'415'500},
   {"Tubus Rosarium", 2'637'500},
   {"Tubus Sororibus", 5'727'600},
   {"Tussock Albata", 3'252'500},
   {"Tussock Capillum", 7'025'800},
   {"Tussock Caputus", 3'472'400},
   {"Tussock Catena", 1'766'600},
   {"Tussock Cultro", 1'766'600},
   {"Tussock Divisa", 1'766'600},
   {"Tussock Ignis", 1'849'000},
   {"Tussock Pennata", 5'853'800},
   {"Tussock Pennatis", 1'000'000},
   {"Tussock Propagito", 1'000'000},
   {"Tussock Serrati", 4'447'100},
   {"Tussock Stigmasis", 19'010'800},
   {"Tussock Triticum", 7'774'700},
   {"Tussock Ventusa", 3'227'700},
   {"Tussock Virgam", 14'313'700},
  }
};

///\brief the range of values for a name out of the journal
///\detail for a species (after ScanOrganic) this is a single value, for a genus alone the span of the whole family.
/// Genus names from the journal do not always match the price list - "Brain Trees" against "Brain Tree",
/// "Luteolum Anemone" against "Anemone" - so the match falls through successive rules.
[[nodiscard]]
auto organic_value_range(std::string_view name) noexcept -> std::optional<std::pair<uint32_t, uint32_t>>;

///\brief a micro resource's internal name, the key shared by both sources
///\detail "$weaponschematic_name;" and "weaponschematic" are the same material
[[nodiscard]]
auto micro_resource_key(std::string_view name) -> std::string;

///\brief reads Status.json; it is absent until the game has run once
[[nodiscard]]
auto load_status(std::string journal_dir_path) -> cxx23::expected<events::status_file_t, std::error_code>;

///\brief loads the Market.json sitting next to the journals
[[nodiscard]]
auto load_market(std::string journal_dir_path) -> cxx23::expected<events::market_file_t, std::error_code>;

///\brief Cargo.json beside the journals, overwritten - it says what is aboard right now
[[nodiscard]]
auto load_cargo(std::string journal_dir_path) -> cxx23::expected<events::cargo_file_t, std::error_code>;

[[nodiscard]]
auto body_short_name(std::string_view system, std::string_view name) -> std::string_view;

[[nodiscard]]
auto planet_name_from_ring_name(std::string_view system, std::string_view name) -> std::string_view;

static constexpr std::array<planet_value_info_t, 19> exploration_values{
  {{"Metal rich body", 21'790.0},
   {"High metal content body", 9'693.0, 93'328.0},  // the bonus added when terraformable
   {"Rocky body", 300.0, 93'328.0},
   {"Icy body", 300.0},
   {"Rocky ice body", 300.0},
   {"Earthlike body", 64'831.0 + 116'295.0},  // an Earth-like is always "terraformed" by the definition of the base value
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
///\brief a star's value by its class and mass; the discovery bonus when nobody had it before
[[nodiscard]]
auto star_value(std::string_view star_type, double stellar_mass, bool is_first_discoverer = false) noexcept -> uint32_t;
///\brief a planet scanned and not mapped - what the FSS scan alone brings
[[nodiscard]]
auto scanned_value(planet_value_info_t const & info, double mass_em, bool is_terraformable, bool is_first_discoverer)
  -> uint32_t;
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
