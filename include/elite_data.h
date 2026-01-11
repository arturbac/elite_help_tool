#pragma once
#include <elite_events.h>

namespace info
  {
enum struct government_e : uint8_t
  {
  unknown,
  anarchy,
  communism,
  confederacy,
  cooperative,
  corporate,
  democracy,
  dictatorship,
  feudal,
  patronage,
  prison_colony,
  theocracy,
  engineer,
  private_ownership
  };

consteval auto adl_enum_bounds(government_e)
  {
  using enum government_e;
  return simple_enum::adl_info{unknown, private_ownership};
  }
enum struct allegiance_e : uint8_t
  {
  unknown,
  independent,
  alliance,
  empire,
  federation,
  thargoid,
  guardian
  };

consteval auto adl_enum_bounds(allegiance_e)
  {
  using enum allegiance_e;
  return simple_enum::adl_info{unknown, guardian};
  }
enum struct happiness_e
  {
  unknown,
  elated,
  happy,
  discontented,
  unhappy,
  despondent
  };

consteval auto adl_enum_bounds(happiness_e)
  {
  using enum happiness_e;
  return simple_enum::adl_info{unknown, despondent};
  }

struct faction_info_t
  {
  std::string name;
  int64_t oid{-1};
  double influence;
  double reputation;
  government_e government;
  allegiance_e allegiance;
  happiness_e happiness;

  // assuming same name and skips oid verification
  [[nodiscard]]
  auto operator==(faction_info_t const &) const noexcept -> bool;
  };

[[nodiscard]]
auto to_native(events::faction_info_t && faction) -> faction_info_t;

enum struct mission_status_e : uint8_t
  {
  accepted,
  redirected,  // done but not delivered and completed
  completed,
  failed,
  abandoned
  };

consteval auto adl_enum_bounds(mission_status_e)
  {
  using enum mission_status_e;
  return simple_enum::adl_info{accepted, abandoned};
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
  };

using space_location_t = std::array<double, 3>;

struct route_item_t
  {
  std::string system;
  uint64_t system_address;
  /// star position in light years
  space_location_t star_location;
  std::string star_class;
  double distance;
  bool visited;
  };

constexpr double light_speed_mps = 299'792'458.0;

///\returns distance in Ly
[[nodiscard]]
auto distance(space_location_t const & loc1, space_location_t const & loc2) -> double;

[[nodiscard]]
auto transform_mission_name(std::string_view input) -> std::string;
  }  // namespace info
