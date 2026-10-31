#include <holdings.h>

#include <boost/ut.hpp>

#include <vector>

using namespace boost::ut;

auto main() -> int
  {
  using holdings::counts_t;
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

  "the backpack and the locker together, consumables left out"_test = []
  {
    std::vector<holdings::held_t> const backpack{
      {.name = "suitschematic", .shown = "Suit Schematic", .category = "Item", .count = 1u},
      {.name = "healthpack", .shown = "Medkit", .category = "Consumable", .count = 4u},
    };
    std::vector<holdings::held_t> const locker{
      {.name = "suitschematic", .shown = "Suit Schematic", .category = "Item", .count = 10u}
    };
    auto const all{holdings::totals(backpack, locker)};
    expect(all.size() == 1u);
    expect(all.at("suitschematic") == 11);
  };

  "moved between the backpack and the locker: nothing left on the carrier"_test = []
  {
    counts_t const docked{{"suitschematic", 11}};
    counts_t const leaving{{"suitschematic", 11}};
    expect(holdings::visit_balance(docked, leaving).empty());
  };

  "less on leaving was left in the store, more was taken from it"_test = []
  {
    counts_t const docked{{"suitschematic", 11}, {"insight", 2}};
    counts_t const leaving{{"suitschematic", 6}, {"insight", 5}, {"graphene", 1}};
    auto const balance{holdings::visit_balance(docked, leaving)};
    expect(balance.size() == 3u);
    expect(balance.at("suitschematic") == 5);
    expect(balance.at("insight") == -3);
    expect(balance.at("graphene") == -1);
  };

  "handed in at one's own bar is left in the store: the bar sells out of it"_test = []
  {
    // 4 handed in at the bar and 1 more left in the store - all 5 are in the store now
    counts_t const docked{{"weaponschematic", 10}};
    counts_t const leaving{{"weaponschematic", 5}};
    auto const balance{holdings::visit_balance(docked, leaving)};
    expect(balance.size() == 1u);
    expect(balance.at("weaponschematic") == 5);
  };

  "the rows: each kind in its places and its stores, by kind and name"_test = []
  {
    std::vector<holdings::held_t> const backpack{
      {.name = "insight", .shown = "Insight", .category = "Data", .count = 2u}
    };
    std::vector<holdings::held_t> const locker{
      {.name = "insight", .shown = "Insight", .category = "Data", .count = 5u},
      {.name = "healthpack", .shown = "Medkit", .category = "Consumable", .count = 9u},
      {.name = "graphene", .shown = "Graphene", .category = "Component", .count = 4u},
    };
    std::vector<counts_t> const stores{{{"compactlibrary", 3}, {"insight", 1}}, {{"graphene", 7}, {"nothing", 0}}};
    std::map<std::string, std::pair<std::string, std::string>, std::less<>> const names{
      {"compactlibrary", {"Compact Library", "Item"}}
    };
    auto const rows{holdings::rows(backpack, locker, stores, names)};
    expect(fatal(rows.size() == 3u));
    expect(rows[0].shown == "Insight" and rows[0].backpack == 2u and rows[0].locker == 5u);
    expect(rows[0].stores == std::vector<int64_t>{1, 0});
    expect(rows[1].kind == kind_e::goods and rows[1].shown == "Compact Library");
    expect(rows[1].stores == std::vector<int64_t>{3, 0});
    expect(rows[2].kind == kind_e::assets and rows[2].locker == 4u);
    expect(rows[2].stores == std::vector<int64_t>{0, 7});
  };

  "junk: marked, and hidden when asked"_test = []
  {
    std::vector<holdings::row_t> const rows{
      {.kind = kind_e::data, .name = "insight", .shown = "Insight"},
      {.kind = kind_e::data, .name = "spyware", .shown = "Spyware"},
    };
    std::set<std::string, std::less<>> const junk{"spyware"};
    auto const shown{holdings::with_junk(rows, junk, false)};
    expect(fatal(shown.size() == 2u));
    expect(not shown[0].junk and shown[1].junk);
    auto const hidden{holdings::with_junk(rows, junk, true)};
    expect(fatal(hidden.size() == 1u));
    expect(hidden[0].name == "insight");
  };

  "a bartender's reading sets the store when it is newer than the store's last change"_test = []
  {
    using namespace std::chrono_literals;
    constexpr std::chrono::sys_seconds read{std::chrono::sys_days{std::chrono::year{2026} / 10 / 30}};
    expect(holdings::reading_applies(std::nullopt, read));
    expect(holdings::reading_applies(read - 1h, read));
    expect(not holdings::reading_applies(read, read));
    expect(not holdings::reading_applies(read + 1min, read));
  };
  }
