#pragma once
#include <data/bgs.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

///\brief database rows: wars on foot - conflict zones, their intensity and the settlements fought for
namespace info
  {
///\brief the intensity of a conflict zone on foot, as the reward of a kill there tells it
enum struct cz_intensity_e : uint8_t
  {
  unknown,
  low,
  medium,
  high
  };

///\brief the game pays kills on foot from fixed tables, one per intensity, and the tables do not overlap:
/// low 1 896 - 4 561, medium 7 226 - 33 762, high 39 642 - 87 362. A high zone pays some kills a fraction
/// of its table, so a zone's intensity is the highest its kills told
[[nodiscard]]
constexpr auto cz_intensity_of(uint64_t reward) noexcept -> cz_intensity_e
  {
  if(reward < 6000u)
    return cz_intensity_e::low;
  if(reward < 36000u)
    return cz_intensity_e::medium;
  return cz_intensity_e::high;
  }

///\brief a kill on foot in a conflict zone, and the settlement it was at when that is known (market 0 if not)
struct ground_bond_t
  {
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t system_address;
  uint64_t market_id;
  std::string awarding_faction;
  std::string victim_faction;
  uint64_t reward;
  uint8_t intensity;
  };

///\brief a settlement of one of the sides of a war, as the war found it
struct war_settlement_t
  {
  uint64_t market_id;
  std::string name;
  std::string economy;
  ///\brief the owner when the war began - what the settlement is fought for
  std::string owner_before;
  ///\brief the highest intensity seen there in earlier wars - a lower bound now, it never falls
  cz_intensity_e before;
  ///\brief the highest intensity seen there in this war
  cz_intensity_e now;
  };

///\brief a war under way in a system, and the settlements of both its sides
struct war_view_t
  {
  conflict_t conflict;
  std::chrono::sys_seconds started;
  std::vector<war_settlement_t> settlements;
  };
  }  // namespace info
