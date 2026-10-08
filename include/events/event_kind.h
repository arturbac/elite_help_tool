#pragma once
#include <simple_enum/simple_enum.hpp>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

///\brief journal events: what tells one journal line from another - read first, before the event itself
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
  ShipyardSell,
  SellShipOnRebuy,
  SetUserShipName,
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
  CommunityGoal,
  
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

  }  // namespace events
