#include <boost/ut.hpp>
#include <buy_search.h>

#include <fstream>
#include <sstream>

namespace
  {
using namespace std::chrono_literals;

constexpr std::chrono::sys_seconds now{std::chrono::sys_days{std::chrono::November / 2 / 2026} + 20h};

[[nodiscard]]
auto row(uint64_t market_id, std::string commodity, int64_t price, int64_t stock, double x) -> info::buy_offer_t
  {
  return info::buy_offer_t{
    .market_id = market_id,
    .station = std::format("Port {}", market_id),
    .station_type = "Coriolis",
    .system = std::format("System {}", market_id),
    .loc_x = x,
    .loc_y = 0.0,
    .loc_z = 0.0,
    .dist_from_star_ls = 100.0,
    .pads_small = 4u,
    .pads_medium = 4u,
    .pads_large = 2u,
    .updated = now - 24h,
    .commodity = std::move(commodity),
    .buy_price = price,
    .stock = stock,
    .mean_price = 1000
  };
  }

[[nodiscard]]
auto every_place() -> buy_search::filter_t
  {
  using enum buy_search::place_e;
  return buy_search::filter_t{.places = {space_station, outpost, surface_port, settlement}};
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;
  using buy_search::pad_e;
  using buy_search::place_e;

  "a station type is a kind of place, and an unknown one is never a source"_test = []
  {
    expect(buy_search::place_of("Orbis") == place_e::space_station);
    expect(buy_search::place_of("Outpost") == place_e::outpost);
    expect(buy_search::place_of("CraterOutpost") == place_e::surface_port);
    expect(buy_search::place_of("OnFootSettlement") == place_e::settlement);
    expect(buy_search::place_of("FleetCarrier") == place_e::carrier);
    expect(buy_search::place_of("MegaShip") == place_e::other);
    expect(buy_search::place_of("") == place_e::other);
  };

  "the largest pad, unknown when the pads were never read"_test = []
  {
    expect(buy_search::largest_pad(4u, 4u, 2u) == pad_e::large);
    expect(buy_search::largest_pad(2u, 1u, 0u) == pad_e::medium);
    expect(buy_search::largest_pad(0u, 0u, 0u) == pad_e::none);
    expect(not buy_search::largest_pad(std::nullopt, std::nullopt, std::nullopt).has_value());
  };

  "rows gather by market, and the most hits come first, then the nearest"_test = []
  {
    std::vector<info::buy_offer_t> const rows{
      row(1u, "Steel", 900, 5000, 10.0),
      row(2u, "Steel", 950, 5000, 5.0),
      row(2u, "Gold", 9000, 100, 5.0),
      row(3u, "Steel", 800, 5000, 2.0),
      row(3u, "gold", 9100, 30, 2.0)
    };
    auto const places{buy_search::places_of(rows, std::array{0.0, 0.0, 0.0})};
    expect(places.size() == 3u);
    expect(places[1].offers.size() == 2u);
    expect(places[0].ly.has_value() and *places[0].ly == 10.0);

    std::vector<buy_search::wanted_t> const wanted{{"Steel", 1000u}, {"Gold", 50u}};
    auto const ranked{buy_search::rank(places, wanted, every_place(), now)};
    expect(ranked.size() == 3u);
    // 3 has too little gold, so only 2 has both
    expect(ranked[0].market_id == 2u);
    expect(ranked[1].market_id == 3u);
    expect(ranked[2].market_id == 1u);
    expect(buy_search::hits(ranked[0], wanted) == 2u);
    expect(buy_search::cost(ranked[0], wanted) == std::optional<int64_t>{950 * 1000 + 9000 * 50});
    expect(not buy_search::cost(ranked[1], wanted).has_value());
    expect(not buy_search::cost(ranked[0], {{"Steel", 0u}}).has_value());
  };

  "the filter leaves out the kind, the small pad, the unknown pad and the old reading"_test = []
  {
    auto offer{row(1u, "Steel", 900, 5000, 1.0)};
    auto const kept = [&](buy_search::filter_t const & filter, info::buy_offer_t const & r)
    { return buy_search::rank(buy_search::places_of({r}, std::nullopt), {{"Steel", 0u}}, filter, now).size() == 1u; };

    expect(kept(every_place(), offer));

    buy_search::filter_t carriers_only{.places = {place_e::carrier}};
    expect(not kept(carriers_only, offer));

    auto outpost{offer};
    outpost.pads_large = 0u;
    buy_search::filter_t large{every_place()};
    large.min_pad = pad_e::large;
    expect(not kept(large, outpost));
    large.min_pad = pad_e::medium;
    expect(kept(large, outpost));

    auto unknown{offer};
    unknown.pads_small = unknown.pads_medium = unknown.pads_large = std::nullopt;
    buy_search::filter_t known_only{every_place()};
    known_only.unknown_pads = false;
    expect(not kept(known_only, unknown));
    expect(kept(every_place(), unknown));

    auto old{offer};
    old.updated = now - std::chrono::days{31};
    expect(not kept(every_place(), old));
  };

  "a pair is offered only when no single place has everything"_test = []
  {
    std::vector<info::buy_offer_t> const rows{
      row(1u, "Steel", 900, 5000, 3.0), row(2u, "Gold", 9000, 100, 4.0), row(3u, "Gold", 9000, 100, 50.0)
    };
    std::vector<buy_search::wanted_t> const wanted{{"Steel", 10u}, {"Gold", 10u}};
    auto const ranked{buy_search::rank(buy_search::places_of(rows, std::array{0.0, 0.0, 0.0}), wanted, every_place(), now)};
    auto const pairs{buy_search::covering_pairs(ranked, wanted, 3u)};
    expect(pairs.size() == 2u);
    expect(ranked[pairs[0].first].market_id == 1u and ranked[pairs[0].second].market_id == 2u);
    expect(pairs[0].ly == 4.0) << pairs[0].ly;

    std::vector<buy_search::wanted_t> const steel{{"Steel", 10u}};
    expect(buy_search::covering_pairs(ranked, steel, 3u).empty());
  };

  "jumps are the distance over the range, rounded up"_test = []
  {
    expect(buy_search::jumps(0.0, 30.f) == std::optional<unsigned>{0u});
    expect(buy_search::jumps(31.0, 30.f) == std::optional<unsigned>{2u});
    expect(not buy_search::jumps(10.0, 0.f).has_value());
  };

  "the pads of the last Docked at each place in a journal"_test = []
  {
    std::istringstream journal{
      R"({ "timestamp":"2026-11-01T10:00:00Z", "event":"Docked", "StationName":"A", "MarketID":7, "LandingPads":{ "Small":1, "Medium":0, "Large":1 } })"
      "\n"
      R"({ "timestamp":"2026-11-01T11:00:00Z", "event":"Docked", "StationName":"B", "MarketID":8 })"
      "\n"
      R"({ "timestamp":"2026-11-01T12:00:00Z", "event":"Docked", "StationName":"A", "MarketID":7, "LandingPads":{ "Small":2, "Medium":2, "Large":3 } })"
      "\n"
    };
    auto const pads{buy_search::pads_in_journal(journal)};
    expect(pads.size() == 1u);
    expect(pads.contains(7u) and pads.at(7u).Large == 3u and pads.at(7u).Medium == 2u);
  };

  "only the journals newer than the last one read are read again"_test = []
  {
    // the tests run from an empty directory of their own
    std::filesystem::path const dir{"pads_journals"};
    std::filesystem::create_directory(dir);
    std::ofstream{dir / "Journal.2026-10-01T100000.01.log"}
      << R"({ "event":"Docked", "MarketID":7, "LandingPads":{ "Small":1, "Medium":1, "Large":1 } })" << '\n';
    std::ofstream{dir / "Journal.2026-11-01T100000.01.log"}
      << R"({ "event":"Docked", "MarketID":8, "LandingPads":{ "Small":1, "Medium":0, "Large":0 } })" << '\n';

    auto const all{buy_search::pads_from_journals(dir, {7u, 8u, 9u}, "")};
    expect(all.found.size() == 2u);
    expect(all.newest_journal == std::string{"Journal.2026-11-01T100000.01.log"});

    auto const newer{buy_search::pads_from_journals(dir, {7u, 9u}, "Journal.2026-10-01T100000.01.log")};
    expect(newer.found.empty());
    expect(newer.newest_journal == std::string{"Journal.2026-11-01T100000.01.log"});

    auto const none{buy_search::pads_from_journals(dir, {7u}, all.newest_journal)};
    expect(none.found.empty() and none.newest_journal.empty());
  };
  }
