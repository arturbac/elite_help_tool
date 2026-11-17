#include <system_tree.h>

#include <boost/ut.hpp>

#include <string>

using namespace boost::ut;
using namespace std::string_view_literals;

namespace
  {
  using system_tree::barycentre_t;
  using system_tree::layout_t;
  using system_tree::star_t;

  ///\brief the rows as names: a star's own, a barycentre's prefix in brackets
  auto names(layout_t const & layout, std::span<star_t const> stars, std::span<barycentre_t const> barycentres) -> std::string
    {
    std::string out;
    for(system_tree::row_t const & row: layout.rows)
      {
      if(not out.empty())
        out += ' ';
      out += row.star ? std::string{stars[row.index].short_name} : "(" + std::string{barycentres[row.index].prefix} + ")";
      }
    return out;
    }
  }  // namespace

auto main() -> int
  {
  "A B, AB's planets under them, C and ABC's planets closing the system"_test = []
  {
    // ids as the game gives them, down the tree: ABC 0, AB 1, A 2, B 3, C 4
    std::vector<star_t> const stars{{2u, "A"sv, 1u}, {3u, "B"sv, 1u}, {4u, "C"sv, 0u}};
    std::vector<barycentre_t> const barycentres{{0u, "ABC"sv, true}, {1u, "AB"sv, true}};
    auto const layout{system_tree::order(stars, barycentres)};
    expect(names(layout, stars, barycentres) == "A B (AB) C (ABC)"sv);
    expect(fatal(layout.brackets.size() == 2u));
    expect(layout.brackets[0].barycentre == 1u and layout.brackets[0].first == 0u and layout.brackets[0].last == 2u
           and layout.brackets[0].depth == 0);
    expect(layout.brackets[1].barycentre == 0u and layout.brackets[1].first == 0u and layout.brackets[1].last == 4u
           and layout.brackets[1].depth == 1);
    // the outer one ticks AB's own row, C and its own
    expect(layout.brackets[1].ticks == std::vector<std::size_t>{2u, 3u, 4u});
  };

  "ABCD round two pairs with no planets of their own: the pairs bracketed, ABCD's row last round all"_test = []
  {
    std::vector<star_t> const stars{{2u, "A"sv, 1u}, {3u, "B"sv, 1u}, {5u, "C"sv, 4u}, {6u, "D"sv, 4u}};
    std::vector<barycentre_t> const barycentres{{0u, "ABCD"sv, true}, {1u, ""sv, false}, {4u, ""sv, false}};
    auto const layout{system_tree::order(stars, barycentres)};
    expect(names(layout, stars, barycentres) == "A B C D (ABCD)"sv);
    expect(fatal(layout.brackets.size() == 3u));
    expect(layout.brackets[2].barycentre == 0u and layout.brackets[2].first == 0u and layout.brackets[2].last == 4u
           and layout.brackets[2].depth == 1);
  };

  "a pair of A and C: they stand together with their row, B after them"_test = []
  {
    std::vector<star_t> const stars{{2u, "A"sv, 1u}, {4u, "B"sv, 0u}, {3u, "C"sv, 1u}};
    std::vector<barycentre_t> const barycentres{{1u, "AC"sv, true}};
    auto const layout{system_tree::order(stars, barycentres)};
    expect(names(layout, stars, barycentres) == "A C (AC) B"sv);
  };

  "names of their own and no star circling it: the row goes last, as before"_test = []
  {
    std::vector<star_t> const stars{{1u, ""sv, std::nullopt}};
    std::vector<barycentre_t> const barycentres{{5u, "Alpha"sv, true}};
    auto const layout{system_tree::order(stars, barycentres)};
    expect(names(layout, stars, barycentres) == "(Alpha)"sv);
    expect(layout.brackets.empty());
  };

  "a barycentre crossing another (by a wrong name) goes last instead of breaking the tree"_test = []
  {
    std::vector<star_t> const stars{{2u, "A"sv, 1u}, {3u, "B"sv, 1u}, {4u, "C"sv, std::nullopt}};
    std::vector<barycentre_t> const barycentres{{1u, "AB"sv, true}, {7u, "BC"sv, true}};
    auto const layout{system_tree::order(stars, barycentres)};
    expect(names(layout, stars, barycentres) == "A B (AB) C (BC)"sv);
    expect(layout.brackets.size() == 1u);
  };
  }
