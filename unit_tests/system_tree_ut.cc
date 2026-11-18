#include <system_tree.h>

#include <boost/ut.hpp>

#include <algorithm>
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

  "Spoihee OR-L c24-107 as Dunkan's database has it: AB, CD with a pair of its planets, ABCD round both, EF"_test = []
  {
    // CD 2 and CD 3 circle barycentre 22 of their own, named CD; nothing circles the root one
    std::vector<star_t> const stars{
      {3u, "A"sv, 2u}, {4u, "B"sv, 2u}, {6u, "C"sv, 5u}, {7u, "D"sv, 5u}, {9u, "E"sv, 8u}, {10u, "F"sv, 8u}};
    std::vector<barycentre_t> const barycentres{
      {1u, "ABCD"sv, true}, {2u, "AB"sv, true}, {5u, "CD"sv, true}, {8u, "EF"sv, true}, {22u, "CD"sv, true}};
    auto const layout{system_tree::order(stars, barycentres)};
    expect(names(layout, stars, barycentres) == "A B (AB) C D (CD) (ABCD) E F (EF)"sv);
    expect(fatal(layout.brackets.size() == 4u));
    // CD 1, 2 and 3 in one row standing for CD's barycentre, the pair's barycentre 22 with it
    expect(fatal(layout.rows.size() == 10u));
    expect(layout.rows[5].index == 2u and layout.rows[5].with == std::vector<std::size_t>{4u} and layout.rows[5].centre == 5u);
    auto const cd{std::ranges::find(layout.brackets, 5u, &system_tree::bracket_t::barycentre)};
    expect(fatal(cd != layout.brackets.end()));
    expect(cd->first == 3u and cd->last == 5u and cd->ticks == std::vector<std::size_t>{3u, 4u, 5u});
    auto const abcd{std::ranges::find(layout.brackets, 1u, &system_tree::bracket_t::barycentre)};
    expect(fatal(abcd != layout.brackets.end()));
    expect(abcd->first == 0u and abcd->last == 6u and abcd->depth == 1 and abcd->ticks == std::vector<std::size_t>{2u, 5u, 6u});
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

  "a star numbered as a planet and circling a star stands in its row; a lettered companion does not"_test = []
  {
    // Spoihee PT-Q e5-285 (2026-11-18): 5 round the AeBe star 0, 6 and 7 round a barycentre on its orbit
    expect(system_tree::in_planet_place("5"sv, 0u));
    expect(system_tree::in_planet_place("AB 6"sv, 1u));
    expect(not system_tree::in_planet_place("B"sv, 0u));
    expect(not system_tree::in_planet_place("5"sv, std::nullopt));
    expect(not system_tree::in_planet_place(""sv, 0u));
  };
  }
