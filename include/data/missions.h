#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <simple_enum/simple_enum.hpp>

///\brief database rows: missions as the tool keeps them
namespace info
  {
enum struct mission_status_e : uint8_t
  {
  accepted,
  redirected,  // done but not delivered and completed
  completed,
  failed,
  abandoned,
  ///\brief the game stopped listing it as open and we never saw it close
  expired
  };

consteval auto adl_enum_bounds(mission_status_e)
  {
  using enum mission_status_e;
  return simple_enum::adl_info{accepted, expired};
  }

struct mission_t
  {
  uint64_t mission_id;
  mission_status_e status;
  std::chrono::sys_seconds expiry;
  std::string faction;
  std::string type;
  std::string description;
  uint64_t reward;
  ///\brief the station the mission was taken at, zero when unknown
  uint64_t market_id;
  ///\brief when the mission closed - without it weekly statistics cannot be counted
  std::chrono::sys_seconds closed;

  std::string target;
  std::string target_type;
  std::string target_faction;

  std::string destination_system;   //": "Anana",
  std::string destination_station;  //": "Yamazaki Base",
  std::string destination_settlement;

  std::string redirected_system;   //": "Anana",
  std::string redirected_station;  //": "Yamazaki Base",
  std::string redirected_settlement;

  uint32_t count;
  uint16_t kill_count;
  uint16_t passenger_count;

  [[nodiscard]]
  auto mission_count() const noexcept
    {
    return std::max<uint32_t>(std::max<uint32_t>(count, kill_count), passenger_count);
    }

  ///\brief the place the mission sends the player to, as far as the game really tells it
  ///\detail empty for a hunt in space - see the definition
  [[nodiscard]]
  auto destination_place() const noexcept -> std::string_view;
  };

///\brief a commodity a mission requires - a separate table, so the mission schema stays untouched
struct mission_cargo_t
  {
  uint64_t mission_id;
  ///\brief the readable name, the same as in the commodity dictionary
  std::string commodity;
  uint32_t count;
  };

///\brief how many missions, and of what kind, were done for a faction in a given period
struct mission_stat_t
  {
  std::string faction;
  uint32_t missions;
  uint64_t rewards;
  ///\brief the most frequent kind, Mission_Massacre for instance
  std::string top_type;
  };

[[nodiscard]]
auto transform_mission_name(std::string_view input) -> std::string;
  }  // namespace info
