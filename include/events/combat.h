#pragma once
#include <events/common.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

///\brief journal events: fighting - targets, kills, bounties, fighters and their crew, conflict zones on foot
namespace events
  {

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

///\brief the commander died - on foot no longer, wherever they wake up
struct died_t
  {
  };

///\brief waking up again - after a death, or leaving by escape pod ("escape"), which leaves no death behind
struct resurrect_t
  {
  std::string Option;
  };

///\brief a crime - on foot a murder of a clean victim is the only trace of that kill
struct commit_crime_t
  {
  std::string CrimeType;
  std::string Faction;
  std::string Victim;
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

  }  // namespace events
