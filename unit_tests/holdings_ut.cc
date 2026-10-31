#include <holdings.h>

#include <boost/ut.hpp>

#include <vector>

using namespace boost::ut;

namespace
  {
using namespace std::chrono_literals;
constexpr std::chrono::sys_seconds reading{std::chrono::sys_days{std::chrono::year{2026} / 10 / 30}};
  }  // namespace

auto main() -> int
  {
  using holdings::bar_t;
  using holdings::kind_e;

  "the kinds as the locker and the bartender file them"_test = []
  {
    expect(holdings::kind_of("Data") == kind_e::data);
    expect(holdings::kind_of("Item") == kind_e::goods);
    expect(holdings::kind_of("Goods") == kind_e::goods);
    expect(holdings::kind_of("Component") == kind_e::assets);
    expect(holdings::kind_of("Assets") == kind_e::assets);
    expect(not holdings::kind_of("Consumable"));
  };

  "the bar: the reading, then what was handed in and bought after it"_test = []
  {
    bar_t const bar{
      .read = reading,
      .shelf = {{.name = "weaponschematic", .shown = "Weapon Schematic", .category = "Data", .stock = 10u}},
      .sets = {},
      .moves = {
        {.when = reading - 1h, .name = "weaponschematic", .delta = 5},
        {.when = reading, .name = "weaponschematic", .delta = 7},
        {.when = reading + 1h, .name = "weaponschematic", .delta = 3},
        {.when = reading + 2h, .name = "weaponschematic", .delta = -4},
      }
    };
    // the move before the reading and the one in its second are in the reading already
    expect(holdings::on_bar(bar, "weaponschematic") == 9);
  };

  "a kind not on the shelf stood at nought at the reading"_test = []
  {
    bar_t const bar{
      .read = reading,
      .shelf = {},
      .sets = {},
      .moves = {{.when = reading - 1h, .name = "insight", .delta = 2}, {.when = reading + 1h, .name = "insight", .delta = 1}}
    };
    expect(holdings::on_bar(bar, "insight") == 1);
  };

  "a count set by hand after the reading starts the kind afresh"_test = []
  {
    bar_t const bar{
      .read = reading,
      .shelf = {{.name = "insight", .shown = "Insight", .category = "Data", .stock = 20u}},
      .sets = {{.when = reading + 2h, .name = "insight", .count = 6}, {.when = reading + 1h, .name = "insight", .count = 50}},
      .moves
      = {{.when = reading + 1h + 30min, .name = "insight", .delta = 4}, {.when = reading + 3h, .name = "insight", .delta = 1}}
    };
    expect(holdings::on_bar(bar, "insight") == 7);
  };

  "a reading after the count set by hand wins"_test = []
  {
    bar_t const bar{
      .read = reading,
      .shelf = {{.name = "insight", .shown = "Insight", .category = "Data", .stock = 3u}},
      .sets = {{.when = reading - 1h, .name = "insight", .count = 50}},
      .moves = {}
    };
    expect(holdings::on_bar(bar, "insight") == 3);
  };

  "no reading yet: every move counts, never below nought"_test = []
  {
    bar_t const bar{.read = {}, .shelf = {}, .sets = {}, .moves = {{.when = reading, .name = "insight", .delta = -3}}};
    expect(holdings::on_bar(bar, "insight") == 0);
  };

  "the rows: each kind in its places, by kind and name, consumables left out"_test = []
  {
    std::vector<holdings::held_t> const backpack{
      {.name = "insight", .shown = "Insight", .category = "Data", .count = 2u}
    };
    std::vector<holdings::held_t> const locker{
      {.name = "insight", .shown = "Insight", .category = "Data", .count = 5u},
      {.name = "medkit", .shown = "Medkit", .category = "Consumable", .count = 9u},
      {.name = "graphene", .shown = "Graphene", .category = "Component", .count = 4u},
    };
    bar_t const bar{
      .read = reading,
      .shelf = {{.name = "weaponschematic", .shown = "Weapon Schematic", .category = "Data", .stock = 10u}},
      .sets = {},
      .moves = {{.when = reading + 1h, .name = "compactlibrary", .delta = 1}}
    };
    std::map<std::string, std::pair<std::string, std::string>, std::less<>> const names{
      {"compactlibrary", {"Compact Library", "Item"}}
    };
    auto const rows{holdings::rows(backpack, locker, bar, names)};
    expect(fatal(rows.size() == 4u));
    expect(rows[0].shown == "Insight" and rows[0].backpack == 2u and rows[0].locker == 5u and rows[0].bar == 0);
    expect(rows[1].shown == "Weapon Schematic" and rows[1].bar == 10);
    expect(rows[2].kind == kind_e::goods and rows[2].shown == "Compact Library" and rows[2].bar == 1);
    expect(rows[3].kind == kind_e::assets and rows[3].shown == "Graphene" and rows[3].locker == 4u);
  };
  }
