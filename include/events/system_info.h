#pragma once
#include <cstdint>
#include <string>
#include <vector>

///\brief journal events: the factions and conflicts of a star system, as Location and FSDJump describe it
namespace events
  {

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

  }  // namespace events
