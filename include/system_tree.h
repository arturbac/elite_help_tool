#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

///\brief the order of the system map's rows as the system's tree has it (PoC, `overlay.system_map.nest_barycentres`):
/// the row of a barycentre's own planets right after the stars it circles, bracketed with them, a barycentre round
/// others bracketing theirs too - so AB 1 stands under A and B, and ABCD 1 closes the whole system
namespace system_tree
  {
///\brief a star: its body id, its name without the system's (`A`, `B`; empty for a lone star named as the system)
/// and the barycentre it circles, the nearest one (the database keeps no other)
struct star_t
  {
  uint32_t body_id{};
  std::string_view short_name;
  std::optional<uint32_t> barycentre;
  };

///\brief a barycentre: its body id, the name its planets have without their number and the system's (`AB` of
/// `AB 1`; empty for a pair of stars with no planets of its own), whether it has a row of planets
struct barycentre_t
  {
  uint32_t body_id{};
  std::string_view prefix;
  bool has_row{};
  };

///\brief a row: a star, or the planets of a barycentre and of those among the same stars (CD 1 round C and D, CD 2
/// and CD 3 round a pair's barycentre of their own: one row), by their index in the spans given, and the barycentre
/// the row stands for - its planets circling another are a pair, bracketed over them
struct row_t
  {
  bool star{};
  std::size_t index{};
  std::vector<std::size_t> with;
  uint32_t centre{};
  };

///\brief a bracket over rows first..last of the order, round a barycentre; depth 0 the innermost, each one round
/// others one more; ticks at its rows that are its own (its stars, its own row, the own rows of the barycentres in it)
struct bracket_t
  {
  uint32_t barycentre{};
  std::size_t first{};
  std::size_t last{};
  int depth{};
  std::vector<std::size_t> ticks;
  };

struct layout_t
  {
  std::vector<row_t> rows;
  std::vector<bracket_t> brackets;
  };

///\brief which stars a barycentre stands among: those circling it, and those its name's letters name (`ABCD` - every
/// star whose own name is one of its letters); none when neither says (a system with names of its own)
[[nodiscard]]
auto members(barycentre_t const & barycentre, std::span<star_t const> stars) -> std::vector<std::size_t>;

///\brief the rows in the tree's order: the stars as given (already in the order of their letters) except that the
/// stars of one barycentre stand together, each barycentre's row after its stars and the barycentres in it; a
/// barycentre whose stars are not known, or that would cross another's, after everything as before
[[nodiscard]]
auto order(std::span<star_t const> stars, std::span<barycentre_t const> barycentres) -> layout_t;
  }  // namespace system_tree
