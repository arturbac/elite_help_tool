#pragma once

#include <atmosphere_odds.h>

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief what the galaxy map shows now, read off its labels by edworld_eht (`galmap`), against what the databases know:
/// for each mass code of the procedural names in view, how often a system of that code scanned in full has a landable
/// planet with an atmosphere, and a rare one, and which of those in view were never visited - where to go next
namespace galaxy_map_view
  {
///\brief a procedural name: `Sector AB-C d12-34` (the map writes it in capitals) - the sector, the block (`AB-C`), the
/// mass code (a..h, how heavy its heaviest star may be) and the rest
struct procedural_t
  {
  std::string sector;
  std::string block;
  char mass_code{};
  std::string number;
  };

///\brief the parts of a procedural name, upper or lower case; none for a name of its own (Colonia, Sol)
[[nodiscard]]
auto parse(std::string_view name) -> std::optional<procedural_t>;

///\brief a mass code's systems in the databases: how many, how many scanned in full, of those how many with a landable
/// planet with an atmosphere and with a rare one, and the classes of their arrival stars (the galaxy map's filter),
/// the most common first
struct code_odds_t
  {
  char code{};
  uint32_t systems{};
  uint32_t full{};
  uint32_t with_atmosphere{};
  uint32_t with_rare{};
  std::vector<std::pair<std::string, uint32_t>> stars;
  };

using odds_t = std::map<char, code_odds_t>;

///\brief every mass code's odds; an atmosphere is rare when the databases know it on at most rare_worlds landable planets
[[nodiscard]]
auto mass_code_odds(atmosphere_odds::census_t const & census, atmosphere_odds::prepared_t const & prepared, uint32_t rare_worlds)
  -> odds_t;

///\brief what the databases know of names: each system's name by its lower case (to tell a system visited, and to
/// copy it as written), and each sector's as written by its lower case (to write a name never visited as the journal
/// would)
struct known_t
  {
  std::map<std::string, std::string, std::less<>> names;
  std::map<std::string, std::string, std::less<>> sectors;
  };

[[nodiscard]]
auto known(atmosphere_odds::census_t const & census) -> known_t;

///\brief a label's full name as the journal writes it: a visited system's from the databases; a procedural one never
/// visited with its sector as the databases write it (as the map does when none of the sector is known), its block
/// in capitals and its mass code in lower case (BLEIA EOHN PW-D B32-6: Bleia Eohn PW-D b32-6); anything else as it is
[[nodiscard]]
auto full_name(std::string_view label, known_t const & known) -> std::string;

///\brief the procedural names in view of one mass code: how many, which never visited and which visited (as the map
/// writes them, each in order)
struct group_t
  {
  char code{};
  uint32_t in_view{};
  std::vector<std::string> unvisited;
  std::vector<std::string> visited;
  code_odds_t odds;
  };

struct view_t
  {
  ///\brief the sector most names in view lie in, as the map writes it; empty when none is procedural
  std::string sector;
  ///\brief the distinct names in view, systems only (a label with an unknown glyph or the grid's coordinates is none)
  uint32_t names{};
  ///\brief by the share of rare ones among the code's systems scanned in full, the highest first
  std::vector<group_t> groups;
  };

///\brief the labels edworld read (each name may come twice) grouped by mass code
[[nodiscard]]
auto view(std::span<std::string const> labels, odds_t const & odds, known_t const & known) -> view_t;
  }  // namespace galaxy_map_view
