#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief the chance of a jump to find a landable planet of an atmosphere, by the class of the star the jump drops the
/// ship at - the one the galaxy map shows and filters by, so the one to choose the next jumps by; the parent star and
/// the parent body show where such planets sit, which a jump cannot be chosen by
namespace atmosphere_odds
  {
///\brief a class of the galaxy map's filter with fewer systems scanned in full than this: its chance is weak
inline constexpr uint32_t weak_below{20u};

struct system_t
  {
  uint64_t address{};
  ///\brief the arrival star's StarType, as FSDJump gives it; empty when not known
  std::string arrival_type;
  ///\brief the bodies the system has, by the discovery scan; 0 when not known
  uint32_t body_count{};
  };

///\brief a body scanned - a star or a planet
struct body_t
  {
  uint64_t address{};
  uint32_t body_id{};
  bool star{};
  std::string star_type;
  double distance_ls{};
  std::string atmosphere_type;
  bool landable{};
  ///\brief the body_id of the star it, or the planet it orbits, goes around; -1 when not known
  int64_t parent_star{-1};
  ///\brief the body_id of the planet it orbits, a moon's; -1 for a planet going around a star
  int64_t parent_planet{-1};
  std::string planet_class;
  };

///\brief what a database knows: its systems and their bodies scanned
struct census_t
  {
  std::vector<system_t> systems;
  std::vector<body_t> bodies;
  };

///\brief what a landable planet with an atmosphere orbits
enum struct parent_e : uint8_t
  {
  star,
  gas_giant,
  icy_body,
  rocky_body,
  unknown
  };

[[nodiscard]]
auto parent_name(parent_e parent) noexcept -> std::string_view;

///\brief a landable planet with an atmosphere, with what it orbits
struct world_t
  {
  uint64_t address{};
  std::string atmosphere;
  ///\brief the class of the galaxy map's filter its parent star is shown under, empty when not scanned
  std::string parent_star;
  parent_e parent{parent_e::unknown};
  };

struct prepared_t
  {
  uint32_t systems_known{};
  ///\brief the systems whose every body is known, by the class of the galaxy map's filter of their arrival star
  std::map<uint64_t, std::string> full;
  std::vector<world_t> worlds;
  };

///\brief another database's systems and bodies added to the first - a system and a body only once, the arrival star
/// and the body count kept where only one of them knows it
[[nodiscard]]
auto merge(census_t into, census_t const & other) -> census_t;

///\brief the systems scanned in full and the landable planets with an atmosphere; a system is in full when it knows
/// as many stars and planets as its discovery scan counted. The arrival star is the one FSDJump named, or the nearest
/// star scanned
[[nodiscard]]
auto prepare(census_t const & census) -> prepared_t;

struct atmosphere_row_t
  {
  std::string atmosphere;
  ///\brief its landable planets, in every system
  uint32_t worlds{};
  ///\brief the systems scanned in full with one of them at least, and their share of those systems
  uint32_t systems_with{};
  double chance{};
  };

///\brief every atmosphere of a landable planet, the rarest first
[[nodiscard]]
auto atmospheres(prepared_t const & prepared) -> std::vector<atmosphere_row_t>;

struct class_row_t
  {
  ///\brief the class of the galaxy map's star filter
  std::string key;
  uint32_t systems{};
  uint32_t systems_with{};
  double chance{};
  ///\brief the chance over the chance of any system scanned in full; 0 when the atmosphere is in none
  double lift{};
  bool weak{};
  };

///\brief the chance of each class of arrival star for one atmosphere, in the order of the galaxy map's filter
[[nodiscard]]
auto by_arrival_star(prepared_t const & prepared, std::string_view atmosphere) -> std::vector<class_row_t>;

struct share_row_t
  {
  std::string key;
  uint32_t worlds{};
  ///\brief of the atmosphere's landable planets, and of all the landable planets with an atmosphere
  double share{};
  double background{};
  };

///\brief the atmosphere's landable planets by the class of their parent star, in the order of the galaxy map's filter;
/// "not scanned" last
[[nodiscard]]
auto by_parent_star(prepared_t const & prepared, std::string_view atmosphere) -> std::vector<share_row_t>;

///\brief the atmosphere's landable planets by what they orbit
[[nodiscard]]
auto by_parent_body(prepared_t const & prepared, std::string_view atmosphere) -> std::vector<share_row_t>;

///\brief an atmosphere with this many landable planets known, or fewer, is rare: worth the landing to see
inline constexpr uint32_t rare_up_to{10u};

///\brief a landable planet with an atmosphere, by its system and body
struct landable_t
  {
  uint64_t address{};
  uint32_t body_id{};
  std::string atmosphere;
  };

using counts_t = std::map<std::string, uint32_t, std::less<>>;

///\brief how many landable planets of each atmosphere the databases know; a planet in more than one counts once, as
/// atmospheres() counts them in the merged census
[[nodiscard]]
auto landable_counts(std::vector<landable_t> planets) -> counts_t;

///\brief an atmosphere of a system's landable planets
struct here_row_t
  {
  std::string atmosphere;
  ///\brief its landable planets in this system
  uint32_t here{};
  ///\brief its landable planets known everywhere, these among them
  uint32_t known{};
  };

///\brief the atmospheres of one system's landable planets (one entry a planet; empty or "None" is none), the rarest
/// first; a planet here the databases do not hold yet still counts as known
[[nodiscard]]
auto atmospheres_here(std::span<std::string const> planets, counts_t const & known) -> std::vector<here_row_t>;
  }  // namespace atmosphere_odds
