#include <boost/ut.hpp>
#include <credits.h>

#include <chrono>
#include <string>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;
  using credits::category_e;

  auto const at{[](int day, std::chrono::seconds time)
                { return credits::time_point_t{std::chrono::sys_days{std::chrono::year{2026} / 9 / day}} + time; }};

  "the movements between two readings add up, nothing is told from the balance"_test = [&at]
  {
    // lines as the game wrote them, cut short where nothing more is read
    std::string const journal{
      R"({ "timestamp":"2026-09-01T10:00:00Z", "event":"Fileheader", "part":1 }
{ "timestamp":"2026-09-01T10:00:01Z", "event":"Commander", "FID":"F11572995", "Name":"SJONA ATREIDES" }
{ "timestamp":"2026-09-01T10:00:02Z", "event":"LoadGame", "FID":"F11572995", "Commander":"SJONA ATREIDES", "Credits":1000000, "Loan":0 }
{ "timestamp":"2026-09-01T10:05:00Z", "event":"MarketBuy", "MarketID":1, "Type":"insulatingmembrane", "Count":10, "BuyPrice":100, "TotalCost":1000 }
{ "timestamp":"2026-09-01T10:06:00Z", "event":"RedeemVoucher", "Type":"bounty", "Amount":443400, "Factions":[ { "Faction":"The Mercs of Mikunn", "Amount":443400 } ] }
{ "timestamp":"2026-09-01T10:07:00Z", "event":"SellOrganicData", "MarketID":1, "BioData":[ { "Genus":"$Codex_Ent_Shrubs_Genus_Name;", "Value":7774700, "Bonus":31098800 } ] }
{ "timestamp":"2026-09-01T10:08:00Z", "event":"MissionCompleted", "Faction":"X", "Reward":190669, "Donated":669 }
{ "timestamp":"2026-09-01T10:09:00Z", "event":"ModuleBuy", "Slot":"MainEngines", "BuyPrice":56398, "SellPrice":1000, "Ship":"mandalay", "ShipID":15 }
{ "timestamp":"2026-09-01T10:10:00Z", "event":"CarrierBankTransfer", "CarrierID":3, "Withdraw":500, "PlayerBalance":40451002, "CarrierBalance":5000000000 }
{ "timestamp":"2026-09-01T11:00:00Z", "event":"Music", "MusicTrack":"MainMenu" }
)"
    };
    auto const logs{credits::scan_journal(journal)};
    expect(fatal(logs.size() == 1u));
    expect(logs[0].fid == "F11572995");
    expect(logs[0].name == "SJONA ATREIDES");
    expect(fatal(logs[0].sessions.size() == 1u));
    expect(logs[0].sessions[0] == credits::session_t{at(1, 10h + 2s), at(1, 11h)});

    auto const ledger{credits::build_ledger(logs)};
    expect(ledger.balance == std::optional<int64_t>{40451002}) << *ledger.balance;
    expect(ledger.balance_read == ledger.balance);
    // 1'000'000 - 1000 + 443400 + 38873500 + 190000 - 55398 + 500 = 40451002: nothing is left over
    for(credits::record_t const & entry: ledger.entries)
      expect(not credits::is_inferred(entry.category)) << credits::category_label(entry.category);

    auto const session{credits::summarise_since(ledger, at(1, 10h))};
    expect(session.by_category[size_t(category_e::combat)] == 443400);
    expect(session.by_category[size_t(category_e::exobiology)] == 38873500);
    expect(session.by_category[size_t(category_e::missions)] == 190000);
    expect(session.by_category[size_t(category_e::ships_and_modules)] == -55398);
    expect(session.by_category[size_t(category_e::carrier_transfer)] == 500);
    expect(session.transfers() == 500);
    expect(session.income() == 443400 + 38873500 + 190000);
    expect(session.expenses() == -1000 - 55398);
    expect(session.played == 59min + 58s);
    expect(session.per_hour().has_value());
  };

  "goods handed in for colonisation move over from trade, the payout is told from the balance"_test = [&at]
  {
    std::string const journal{
      R"({ "timestamp":"2026-09-05T01:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":10000000 }
{ "timestamp":"2026-09-05T01:10:00Z", "event":"MarketBuy", "MarketID":4, "Type":"steel", "Count":100, "BuyPrice":1000, "TotalCost":100000 }
{ "timestamp":"2026-09-05T01:39:51Z", "event":"ColonisationContribution", "MarketID":3, "Contributions":[ { "Name":"$Steel_name;", "Name_Localised":"Steel", "Amount":40 } ] }
{ "timestamp":"2026-09-05T02:00:00Z", "event":"Shutdown" }
)"
    };
    // next session: the balance is up by what the site paid, nothing in the journal says so
    std::string const next{
      R"({ "timestamp":"2026-09-06T01:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":9952000 }
)"
    };
    auto logs{credits::scan_journal(journal)};
    auto const more{credits::scan_journal(next)};
    logs.insert(logs.end(), more.begin(), more.end());
    auto const ledger{credits::build_ledger(logs)};
    expect(ledger.balance == std::optional<int64_t>{9952000});
    expect(ledger.sessions.size() == 2u);

    auto const day{credits::summarise(ledger, credits::period_e::day, nullptr)};
    expect(fatal(day.size() == 1u));
    expect(day[0].label == "2026-09-05") << day[0].label;
    expect(day[0].by_category[size_t(category_e::trade)] == -60000);
    expect(day[0].by_category[size_t(category_e::colonisation)] == -40000);
    expect(day[0].by_category[size_t(category_e::colonisation_payout)] == 52000);
    expect(day[0].closing_balance == std::optional<int64_t>{9952000});
    expect(day[0].played == 1h);
  };

  "a round difference is the squadron bank, an odd one is unexplained"_test = []
  {
    std::string const journal{
      R"({ "timestamp":"2026-08-01T10:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":10000000000 }
{ "timestamp":"2026-08-01T10:10:00Z", "event":"RefuelAll", "Cost":56, "Amount":1.0 }
{ "timestamp":"2026-08-01T12:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":2509999944 }
{ "timestamp":"2026-08-01T12:10:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":2510013695 }
)"
    };
    auto const ledger{credits::build_ledger(credits::scan_journal(journal))};
    auto const month{credits::summarise(ledger, credits::period_e::month, nullptr)};
    expect(fatal(month.size() == 1u));
    expect(month[0].label == "2026-08");
    expect(month[0].by_category[size_t(category_e::squadron_bank)] == -7'490'000'000);
    expect(month[0].by_category[size_t(category_e::unexplained)] == 13751);
    expect(month[0].by_category[size_t(category_e::upkeep)] == -56);
    expect(month[0].net() == 13751 - 56);
    expect(month[0].transfers() == -7'490'000'000);
  };

  "a payment read on both sides of a reading cancels out, a large transfer is rounded"_test = []
  {
    // 27 Apr 2026: the Squadron Carrier's price left the balance before CarrierBuy was written
    std::string const journal{
      R"({ "timestamp":"2026-04-27T21:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":30000000000 }
{ "timestamp":"2026-04-27T21:04:11Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":4212601025 }
{ "timestamp":"2026-04-27T21:20:00Z", "event":"CarrierBuy", "Price":25000000000 }
{ "timestamp":"2026-04-27T21:21:53Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":4212601025 }
{ "timestamp":"2026-04-27T21:28:33Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":5000000000 }
{ "timestamp":"2026-08-01T09:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":10000000000 }
{ "timestamp":"2026-08-01T09:29:26Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":2512233332 }
)"
    };
    auto const ledger{credits::build_ledger(credits::scan_journal(journal))};
    auto const year{credits::summarise(ledger, credits::period_e::year, nullptr)};
    expect(fatal(year.size() == 1u));
    expect(year[0].by_category[size_t(category_e::carrier)] == -25'000'000'000);
    // 5'000'000'000 to 10'000'000'000 between the sessions is a gap of its own, round
    expect(year[0].by_category[size_t(category_e::squadron_bank)] == 5'000'000'000 - 7'488'000'000)
      << year[0].by_category[size_t(category_e::squadron_bank)];
    expect(year[0].by_category[size_t(category_e::unexplained)] == 233332)
      << year[0].by_category[size_t(category_e::unexplained)];
    expect(year[0].closing_balance == std::optional<int64_t>{2512233332});
  };

  "a crew wage right after an exploration sale is already in the sale's sum"_test = []
  {
    std::string const journal{
      R"({ "timestamp":"2026-08-01T10:00:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":0 }
{ "timestamp":"2026-08-01T10:10:00Z", "event":"MultiSellExplorationData", "BaseValue":1000, "Bonus":0, "TotalEarnings":900 }
{ "timestamp":"2026-08-01T10:10:01Z", "event":"NpcCrewPaidWage", "NpcCrewName":"Adele Saunders", "NpcCrewId":1, "Amount":100 }
{ "timestamp":"2026-08-01T10:20:00Z", "event":"MissionCompleted", "Reward":1000 }
{ "timestamp":"2026-08-01T10:20:01Z", "event":"NpcCrewPaidWage", "NpcCrewName":"Adele Saunders", "NpcCrewId":1, "Amount":50 }
)"
    };
    auto const ledger{credits::build_ledger(credits::scan_journal(journal))};
    expect(ledger.balance == std::optional<int64_t>{1850});
  };

  "periods: an ISO week and a quarter, local midnight in the zone given"_test = []
  {
    std::string const journal{
      R"({ "timestamp":"2025-12-31T22:30:00Z", "event":"LoadGame", "FID":"F1", "Commander":"A", "Credits":0 }
{ "timestamp":"2025-12-31T23:30:00Z", "event":"MissionCompleted", "Reward":1000 }
)"
    };
    auto const ledger{credits::build_ledger(credits::scan_journal(journal))};
    // 23:30 UTC is already the new year in Warsaw, and 1 Jan 2026 (a Thursday) is in 2026's first week
    auto const * const warsaw{std::chrono::locate_zone("Europe/Warsaw")};
    auto const week{credits::summarise(ledger, credits::period_e::week, warsaw)};
    expect(fatal(week.size() == 1u));
    expect(week[0].label == "2026 W01") << week[0].label;
    auto const quarter{credits::summarise(ledger, credits::period_e::quarter, warsaw)};
    expect(fatal(quarter.size() == 2u));
    expect(quarter[0].label == "2026 Q1") << quarter[0].label;
    expect(quarter[0].by_category[size_t(category_e::missions)] == 1000);
    expect(quarter[0].played == 30min);
    expect(quarter[1].label == "2025 Q4") << quarter[1].label;
    expect(quarter[1].played == 30min);
    expect(quarter[1].closing_balance == std::optional<int64_t>{0});
  };
  }
