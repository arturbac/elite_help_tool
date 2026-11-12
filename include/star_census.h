#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief how many systems of each kind of star were visited - to spread the visits evenly over the kinds, and to set
/// the galaxy map's star filter for the next jumps by what is still rare
namespace star_census
  {
struct star_t
  {
  ///\brief the journal's StarType, its Subclass and Luminosity
  std::string type;
  uint32_t sub_class{};
  std::string luminosity;
  double distance_ls{};
  };

///\brief a system visited - one with a star in the database, which only a visit scans
struct system_t
  {
  uint64_t address{};
  std::vector<star_t> stars;
  };

enum struct grouping_e : uint8_t
  {
  type,
  sub_class,
  luminosity
  };

struct row_t
  {
  ///\brief G, G9 or G Vab, by the grouping
  std::string key;
  std::string type;
  ///\brief systems whose arrival star it is, and stars of it in all, the arrival ones among them
  uint32_t arrival{};
  uint32_t all{};
  };

struct census_t
  {
  uint32_t systems{};
  std::vector<row_t> rows;
  };

///\brief the stars of the systems for which in_scope says yes, counted by the grouping; the arrival star is the
/// one nearest where the jump drops the ship. The rows go O, B, A, F, G, K, M, the brown dwarfs, then the rest
[[nodiscard]]
auto count(std::span<system_t const> systems, grouping_e grouping, auto const & in_scope) -> census_t;

///\brief what a StarType is, in a few words; empty for one not known
[[nodiscard]]
auto description(std::string_view type) noexcept -> std::string_view;

///\brief where a StarType stands in the rows
[[nodiscard]]
auto rank(std::string_view type) noexcept -> int;

[[nodiscard]]
auto key_of(star_t const & star, grouping_e grouping) -> std::string;

[[nodiscard]]
auto sorted(std::vector<row_t> rows) -> std::vector<row_t>;

auto count(std::span<system_t const> systems, grouping_e grouping, auto const & in_scope) -> census_t
  {
  census_t census;
  std::vector<row_t> rows;
  auto const row_of = [&rows](std::string key, std::string const & type) -> row_t &
  {
    for(row_t & row: rows)
      if(row.key == key)
        return row;
    return rows.emplace_back(row_t{.key = std::move(key), .type = type});
  };
  for(system_t const & system: systems)
    {
    if(system.stars.empty() or not in_scope(system))
      continue;
    ++census.systems;
    star_t const * arrival{&system.stars.front()};
    for(star_t const & star: system.stars)
      if(star.distance_ls < arrival->distance_ls)
        arrival = &star;
    for(star_t const & star: system.stars)
      {
      row_t & row{row_of(key_of(star, grouping), star.type)};
      ++row.all;
      if(&star == arrival)
        ++row.arrival;
      }
    }
  census.rows = sorted(std::move(rows));
  return census;
  }
  }  // namespace star_census
