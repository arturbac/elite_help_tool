#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief the sky album's index of the systems known: what stands beside a system's name, and how the names are grouped
///
/// A system stands under the galactic region its address names (galaxy_region.h); one outside the map under the
/// unknown region
namespace system_index
  {
inline constexpr std::string_view unknown_region{"Unknown region"};
inline constexpr std::string_view named_systems{"Named systems"};

///\brief a system's page, relative to the codex directory
[[nodiscard]]
auto page_name(std::string_view system) -> std::string;

///\brief the letter a kind of planet is counted under, empty for what is not counted - a belt cluster has no class
[[nodiscard]]
auto body_letter(std::string_view planet_class) -> std::string_view;

///\brief how many bodies of each kind, by the letters of body_letter and S for the stars
using body_counts_t = std::map<std::string, uint32_t>;

///\brief the counts as they stand beside a name: [S:1, G:3, R:14], stars first, the rarer worlds before the common
[[nodiscard]]
auto counts_text(body_counts_t const & counts) -> std::string;

///\brief the sector of a generated name - Bleia Eohn QT-O d7-43 is in Bleia Eohn - or named_systems for a name of its own
[[nodiscard]]
auto sector_of(std::string_view system) -> std::string;

///\brief a system as the index lists it
struct row_t
  {
  uint64_t address{};
  std::string name;
  std::string counts;
  std::string href;
  bool picture{};
  bool life{};
  ///\brief a body of it had been discovered by nobody before
  bool first{};
  };

///\brief what a codex directory left of its systems for the other accounts - none when it has not yet
[[nodiscard]]
auto read_rows(std::filesystem::path const & codex_dir) -> std::vector<row_t>;

auto write_rows(std::filesystem::path const & codex_dir, std::span<row_t const> rows) -> void;

///\brief the region a system stands under, the unknown one outside the map
[[nodiscard]]
auto region_of(uint64_t system_address) -> std::string;

///\brief the page: regions by how many systems they hold, the unknown one last, the sectors in them folded
[[nodiscard]]
auto index_page(std::span<row_t const> rows) -> std::string;

///\brief writes a file by way of a partial one, leaving it alone when it would not change
auto write_if_changed(std::filesystem::path const & path, std::string const & text) -> bool;

[[nodiscard]]
auto escaped(std::string_view text) -> std::string;

///\brief what both kinds of page look like
[[nodiscard]]
auto page_style() -> std::string_view;
  }  // namespace system_index
