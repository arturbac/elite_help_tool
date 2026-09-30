#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

///\brief journal events: missions - taken, handed in, failed, and what they did to the factions
namespace events
  {

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

///\brief what a mission did to a faction's state bars - economy (EP), security (SP), health (Outbreak)
///\detail the name alone says which bar and which way, e.g. "$MISSIONUTIL_Interaction_Summary_EP_up;". It
/// names no system and carries no measure - no pluses like the influence has
struct state_effect_t
  {
  std::string Effect;
  std::string Trend;
  };

///\brief one mission's effect on one faction - a handed-in mission usually moves several factions at once,
/// and each of them may feel it in more than one system
struct faction_effect_t
  {
  std::string Faction;
  ///\brief in the order of Influence when there are as many of them, otherwise all for the first system
  std::vector<state_effect_t> Effects;
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

  }  // namespace events
