#include <credits.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <map>
#include <ranges>
#include <unordered_map>

namespace credits
  {
namespace
  {
using namespace std::string_view_literals;
using namespace std::chrono_literals;

///\brief a crew wage written this soon after a sale whose sum is already net of the crew's cut is not
/// taken off the balance a second time
constexpr auto net_sale_window{10s};
///\brief a balance difference in whole steps of this is a squadron bank transfer - typed in by hand, in
/// round numbers, and the journal says nothing of it
constexpr int64_t squadron_step{1'000'000};
///\brief a difference this large, not a colonisation payout, is a squadron bank transfer
/// with a little of something else in the same stretch - rounded to the step, the rest left unexplained
constexpr int64_t squadron_large{100'000'000};
///\brief differences found within this of each other that add up to nothing are one movement read on
/// both sides of a reading - the Squadron Carrier's price, for one, left the balance before CarrierBuy was written
constexpr auto cancel_window{3h};

///\brief what one balance reading said that the movements before it did not
struct gap_t
  {
  int64_t amount;
  ///\brief the reading that found it
  time_point_t read_at;
  ///\brief the last movement before the reading - where the gap is booked
  time_point_t before;
  ///\brief goods were handed in at a construction site since the reading before, the last ones then
  std::optional<time_point_t> contributed;
  };

///\brief 2026-09-29T05:47:21Z, as every journal line starts
[[nodiscard]]
auto parse_stamp(std::string_view text) -> std::optional<time_point_t>
  {
  auto const number{[text](size_t at, size_t length) -> std::optional<int>
                    {
                      int value{};
                      if(at + length > text.size())
                        return std::nullopt;
                      auto const [end, error]{std::from_chars(text.data() + at, text.data() + at + length, value)};
                      if(error != std::errc{} or end != text.data() + at + length)
                        return std::nullopt;
                      return value;
                    }};
  auto const year{number(0u, 4u)};
  auto const month{number(5u, 2u)};
  auto const day{number(8u, 2u)};
  auto const hour{number(11u, 2u)};
  auto const minute{number(14u, 2u)};
  auto const second{number(17u, 2u)};
  if(not year or not month or not day or not hour or not minute or not second)
    return std::nullopt;
  std::chrono::year_month_day const date{
    std::chrono::year{*year}, std::chrono::month{unsigned(*month)}, std::chrono::day{unsigned(*day)}
  };
  if(not date.ok())
    return std::nullopt;
  return time_point_t{std::chrono::sys_days{date}} + std::chrono::hours{*hour} + std::chrono::minutes{*minute}
         + std::chrono::seconds{*second};
  }

///\brief the value of a quoted field on a journal line - `"key":"value"`, the way the game writes it
[[nodiscard]]
auto quoted_field(std::string_view line, std::string_view key) -> std::string_view
  {
  std::string const needle{std::format("\"{}\":\"", key)};
  auto const at{line.find(needle)};
  if(at == std::string_view::npos)
    return {};
  auto const from{at + needle.size()};
  auto const to{line.find('"', from)};
  return to == std::string_view::npos ? std::string_view{} : line.substr(from, to - from);
  }

using json_t = glz::generic;
using object_t = json_t::object_t;

[[nodiscard]]
auto number(object_t const & object, std::string_view key) -> int64_t
  {
  auto const it{object.find(key)};
  if(it == object.end())
    return 0;
  if(auto const * const value{it->second.get_if<double>()}; value != nullptr)
    return int64_t(*value);
  return 0;
  }

[[nodiscard]]
auto string_field(object_t const & object, std::string_view key) -> std::string
  {
  auto const it{object.find(key)};
  if(it == object.end())
    return {};
  if(auto const * const value{it->second.get_if<std::string>()}; value != nullptr)
    return *value;
  return {};
  }

[[nodiscard]]
auto array(object_t const & object, std::string_view key) -> json_t::array_t const *
  {
  auto const it{object.find(key)};
  return it == object.end() ? nullptr : it->second.get_if<json_t::array_t>();
  }

///\brief "$InsulatingMembrane_name;" and "insulatingmembrane" both to "insulatingmembrane"
[[nodiscard]]
auto commodity_id(std::string_view name) -> std::string
  {
  if(name.starts_with('$'))
    name.remove_prefix(1u);
  if(name.ends_with("_name;"sv))
    name.remove_suffix(6u);
  std::string id{name};
  std::ranges::transform(id, id.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return id;
  }

///\brief a voucher's kind decides whose column its money is in
[[nodiscard]]
auto voucher_category(std::string_view type) -> category_e
  {
  if(type == "bounty"sv or type == "CombatBond"sv)
    return category_e::combat;
  if(type == "codex"sv)
    return category_e::exploration;
  return category_e::other_income;
  }

///\brief the events that move the player's money, and with what - a single movement each, the market
/// purchase and the contribution aside since they carry their goods along
struct simple_event_t
  {
  std::string_view event;
  category_e category;
  std::string_view gain;
  std::string_view cost;
  };

constexpr std::array simple_events{
  simple_event_t{"MarketSell"sv, category_e::trade, "TotalSale"sv, {}},
  simple_event_t{"SellExplorationData"sv, category_e::exploration, "TotalEarnings"sv, {}},
  simple_event_t{"MultiSellExplorationData"sv, category_e::exploration, "TotalEarnings"sv, {}},
  simple_event_t{"BuyExplorationData"sv, category_e::exploration, {}, "Cost"sv},
  simple_event_t{"BuyTradeData"sv, category_e::trade, {}, "Cost"sv},
  simple_event_t{"SellMicroResources"sv, category_e::on_foot_goods, "Price"sv, {}},
  simple_event_t{"BuyMicroResources"sv, category_e::on_foot_goods, {}, "Price"sv},
  simple_event_t{"SellDrones"sv, category_e::upkeep, "TotalSale"sv, {}},
  simple_event_t{"BuyDrones"sv, category_e::upkeep, {}, "TotalCost"sv},
  simple_event_t{"RefuelAll"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"RefuelPartial"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"RepairAll"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"Repair"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"BuyAmmo"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"RestockVehicle"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"BookTaxi"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"BookDropship"sv, category_e::upkeep, {}, "Cost"sv},
  simple_event_t{"ShipyardTransfer"sv, category_e::upkeep, {}, "TransferPrice"sv},
  simple_event_t{"FetchRemoteModule"sv, category_e::upkeep, {}, "TransferCost"sv},
  simple_event_t{"Resurrect"sv, category_e::rebuy, {}, "Cost"sv},
  simple_event_t{"ShipyardBuy"sv, category_e::ships_and_modules, "SellPrice"sv, "ShipPrice"sv},
  simple_event_t{"ShipyardSell"sv, category_e::ships_and_modules, "ShipPrice"sv, {}},
  simple_event_t{"SellShipOnRebuy"sv, category_e::ships_and_modules, "ShipPrice"sv, {}},
  simple_event_t{"ModuleBuy"sv, category_e::ships_and_modules, "SellPrice"sv, "BuyPrice"sv},
  simple_event_t{"ModuleSell"sv, category_e::ships_and_modules, "SellPrice"sv, {}},
  simple_event_t{"ModuleSellRemote"sv, category_e::ships_and_modules, "SellPrice"sv, {}},
  simple_event_t{"BuySuit"sv, category_e::suits_and_weapons, {}, "Price"sv},
  simple_event_t{"SellSuit"sv, category_e::suits_and_weapons, "Price"sv, {}},
  simple_event_t{"BuyWeapon"sv, category_e::suits_and_weapons, {}, "Price"sv},
  simple_event_t{"SellWeapon"sv, category_e::suits_and_weapons, "Price"sv, {}},
  simple_event_t{"UpgradeSuit"sv, category_e::suits_and_weapons, {}, "Cost"sv},
  simple_event_t{"UpgradeWeapon"sv, category_e::suits_and_weapons, {}, "Cost"sv},
  simple_event_t{"PayBounties"sv, category_e::fines, {}, "Amount"sv},
  simple_event_t{"PayFines"sv, category_e::fines, {}, "Amount"sv},
  simple_event_t{"PayLegacyFines"sv, category_e::fines, {}, "Amount"sv},
  simple_event_t{"CrewHire"sv, category_e::crew, {}, "Cost"sv},
  simple_event_t{"CommunityGoalReward"sv, category_e::other_income, "Reward"sv, {}},
  simple_event_t{"SearchAndRescue"sv, category_e::other_income, "Reward"sv, {}},
  simple_event_t{"PowerplaySalary"sv, category_e::powerplay, "Amount"sv, {}},
  simple_event_t{"PowerplayFastTrack"sv, category_e::powerplay, {}, "Cost"sv},
  simple_event_t{"CarrierBuy"sv, category_e::carrier, {}, "Price"sv},
  simple_event_t{"CarrierDecommission"sv, category_e::carrier, "Refund"sv, {}},
};

///\brief the events that need more than a field or two, besides the simple ones
constexpr std::array special_events{
  "MissionCompleted"sv,
  "RedeemVoucher"sv,
  "SellOrganicData"sv,
  "NpcCrewPaidWage"sv,
  "EngineerContribution"sv,
  "CarrierBankTransfer"sv,
  "MarketBuy"sv,
  "ColonisationContribution"sv,
  "LoadGame"sv,
  "Commander"sv
};

///\brief the sales whose sums the game writes already net of the crew's cut
[[nodiscard]]
auto is_net_of_crew(std::string_view event) -> bool
  {
  return event == "SellExplorationData"sv or event == "MultiSellExplorationData"sv or event == "SellMicroResources"sv;
  }

struct reader_t
  {
  std::vector<commander_log_t> logs;
  commander_log_t * current{};
  ///\brief the newest stamp seen of any line, the end of the session running
  time_point_t last_line{};
  ///\brief the last sale whose sum is already net of the crew's cut, and when
  time_point_t net_sale_at{};

  auto commander(std::string_view fid, std::string_view name) -> void
    {
    auto it{std::ranges::find(logs, fid, &commander_log_t::fid)};
    if(it == logs.end())
      {
      logs.push_back(commander_log_t{.fid = std::string{fid}, .name = std::string{name}});
      it = std::prev(logs.end());
      }
    else if(not name.empty())
      it->name = std::string{name};
    current = &*it;
    }

  auto close_session() -> void
    {
    if(current != nullptr and not current->sessions.empty())
      current->sessions.back().to = std::max(current->sessions.back().from, last_line);
    }

  auto move(time_point_t at, category_e category, int64_t amount, std::string_view what, int64_t quantity = 0) -> void
    {
    if(current == nullptr or (amount == 0 and quantity == 0))
      return;
    current->records.push_back(
      record_t{.at = at, .kind = kind_e::movement, .category = category, .amount = amount, .quantity = quantity, .what = std::string{what}}
    );
    }

  auto balance(time_point_t at, int64_t value, std::string_view what) -> void
    {
    if(current != nullptr)
      current->records.push_back(record_t{.at = at, .kind = kind_e::balance, .amount = value, .what = std::string{what}});
    }

  auto line(time_point_t at, std::string_view event, std::string_view text) -> void
    {
    if(event == "Commander"sv)
      {
      commander(quoted_field(text, "FID"sv), quoted_field(text, "Name"sv));
      return;
      }
    if(event == "LoadGame"sv)
      {
      close_session();
      commander(quoted_field(text, "FID"sv), quoted_field(text, "Commander"sv));
      current->sessions.push_back(session_t{.from = at, .to = at});
      }

    json_t json{};
    if(glz::read_json(json, text))
      return;
    auto const * const object{json.get_if<object_t>()};
    if(object == nullptr)
      return;

    if(auto const it{std::ranges::find(simple_events, event, &simple_event_t::event)}; it != simple_events.end())
      {
      int64_t const gain{it->gain.empty() ? 0 : number(*object, it->gain)};
      int64_t const cost{it->cost.empty() ? 0 : number(*object, it->cost)};
      move(at, it->category, gain - cost, event);
      if(is_net_of_crew(event))
        net_sale_at = at;
      return;
      }

    if(event == "LoadGame"sv)
      {
      if(object->contains("Credits"))
        balance(at, number(*object, "Credits"sv), event);
      }
    else if(event == "MissionCompleted"sv)
      move(at, category_e::missions, number(*object, "Reward"sv) - number(*object, "Donated"sv), event);
    else if(event == "RedeemVoucher"sv)
      move(at, voucher_category(string_field(*object, "Type"sv)), number(*object, "Amount"sv), event);
    else if(event == "SellOrganicData"sv)
      {
      int64_t sum{};
      if(auto const * const items{array(*object, "BioData"sv)}; items != nullptr)
        for(json_t const & item: *items)
          if(auto const * const bio{item.get_if<object_t>()}; bio != nullptr)
            sum += number(*bio, "Value"sv) + number(*bio, "Bonus"sv);
      move(at, category_e::exobiology, sum, event);
      }
    else if(event == "NpcCrewPaidWage"sv)
      {
      if(at - net_sale_at > net_sale_window)
        move(at, category_e::crew, -number(*object, "Amount"sv), event);
      }
    else if(event == "EngineerContribution"sv)
      {
      if(string_field(*object, "Type"sv) == "Credits"sv)
        move(at, category_e::engineers, -number(*object, "Quantity"sv), event);
      }
    else if(event == "CarrierBankTransfer"sv)
      {
      move(at, category_e::carrier_transfer, number(*object, "Withdraw"sv) - number(*object, "Deposit"sv), event);
      balance(at, number(*object, "PlayerBalance"sv), event);
      }
    else if(event == "MarketBuy"sv)
      move(at, category_e::trade, -number(*object, "TotalCost"sv), commodity_id(string_field(*object, "Type"sv)), number(*object, "Count"sv));
    else if(event == "ColonisationContribution"sv)
      {
      if(current == nullptr)
        return;
      if(auto const * const items{array(*object, "Contributions"sv)}; items != nullptr)
        for(json_t const & item: *items)
          if(auto const * const goods{item.get_if<object_t>()}; goods != nullptr)
            current->records.push_back(record_t{
              .at = at,
              .kind = kind_e::contribution,
              .category = category_e::colonisation,
              .quantity = number(*goods, "Amount"sv),
              .what = commodity_id(string_field(*goods, "Name"sv))
            });
      }
    }
  };

///\brief Monday of the week a day is in
[[nodiscard]]
auto week_start(std::chrono::sys_days day) -> std::chrono::sys_days
  {
  std::chrono::weekday const wd{day};
  return day - std::chrono::days{(wd.c_encoding() + 6u) % 7u};
  }

///\brief the local calendar period a local day falls in: its first day, the next period's first day, its label
struct period_bounds_t
  {
  std::chrono::local_days from;
  std::chrono::local_days to;
  std::string label;
  };

[[nodiscard]]
auto bounds_of(std::chrono::local_days day, period_e period) -> period_bounds_t
  {
  using namespace std::chrono;
  year_month_day const ymd{day};
  switch(period)
    {
    case period_e::day:
      return {day, day + days{1}, std::format("{:%Y-%m-%d}", ymd)};
    case period_e::week:
      {
      local_days const monday{week_start(sys_days{day.time_since_epoch()}).time_since_epoch()};
      // ISO week: the week belongs to the year its Thursday is in
      year_month_day const thursday{monday + days{3}};
      local_days const first_monday{
        week_start(sys_days{(local_days{thursday.year() / January / 4}).time_since_epoch()}).time_since_epoch()
      };
      auto const number{(monday - first_monday).count() / 7 + 1};
      return {monday, monday + days{7}, std::format("{} W{:02}", int(thursday.year()), number)};
      }
    case period_e::month:
      {
      local_days const first{ymd.year() / ymd.month() / 1};
      return {first, local_days{(ymd.year() / ymd.month() / 1) + months{1}}, std::format("{:%Y-%m}", ymd)};
      }
    case period_e::quarter:
      {
      unsigned const q{(unsigned(ymd.month()) - 1u) / 3u};
      year_month const first{ymd.year(), month{q * 3u + 1u}};
      return {local_days{first / 1}, local_days{(first + months{3}) / 1}, std::format("{} Q{}", int(ymd.year()), q + 1u)};
      }
    case period_e::year:
      return {local_days{ymd.year() / 1 / 1}, local_days{(ymd.year() + years{1}) / 1 / 1}, std::format("{}", int(ymd.year()))};
    }
  return {day, day + days{1}, {}};
  }

[[nodiscard]]
auto to_local(std::chrono::time_zone const * zone, time_point_t at) -> std::chrono::local_seconds
  {
  if(zone == nullptr)
    return std::chrono::local_seconds{at.time_since_epoch()};
  return std::chrono::floor<std::chrono::seconds>(zone->to_local(at));
  }

[[nodiscard]]
auto to_sys(std::chrono::time_zone const * zone, std::chrono::local_seconds at) -> time_point_t
  {
  if(zone == nullptr)
    return time_point_t{at.time_since_epoch()};
  return std::chrono::floor<std::chrono::seconds>(zone->to_sys(at, std::chrono::choose::earliest));
  }

[[nodiscard]]
auto overlap(session_t const & session, time_point_t from, time_point_t to) -> std::chrono::seconds
  {
  auto const start{std::max(session.from, from)};
  auto const end{std::min(session.to, to)};
  return end > start ? end - start : std::chrono::seconds{};
  }
  }  // namespace

auto category_label(category_e category) -> std::string_view
  {
  switch(category)
    {
    case category_e::exploration:         return "Exploration";
    case category_e::exobiology:          return "Exobiology";
    case category_e::trade:               return "Trade";
    case category_e::missions:            return "Missions";
    case category_e::combat:              return "Bounties & bonds";
    case category_e::on_foot_goods:       return "On-foot goods";
    case category_e::other_income:        return "Other income";
    case category_e::ships_and_modules:   return "Ships & modules";
    case category_e::suits_and_weapons:   return "Suits & weapons";
    case category_e::upkeep:              return "Upkeep";
    case category_e::rebuy:               return "Rebuy";
    case category_e::fines:               return "Fines & bounties";
    case category_e::crew:                return "Crew";
    case category_e::engineers:           return "Engineers";
    case category_e::powerplay:           return "Powerplay";
    case category_e::carrier:             return "Carrier";
    case category_e::colonisation:        return "Colonisation goods";
    case category_e::colonisation_payout: return "Colonisation payouts*";
    case category_e::carrier_transfer:    return "Carrier bank";
    case category_e::squadron_bank:       return "Squadron bank*";
    case category_e::unexplained:         return "Unexplained*";
    }
  return {};
  }

auto scan_journal(std::string_view text) -> std::vector<commander_log_t>
  {
  reader_t reader;
  constexpr auto event_key{"\"event\":\""sv};
  constexpr auto stamp_key{"\"timestamp\":\""sv};
  while(not text.empty())
    {
    auto const eol{text.find('\n')};
    std::string_view const line{text.substr(0u, eol)};
    text.remove_prefix(eol == std::string_view::npos ? text.size() : eol + 1u);

    auto const stamp_at{line.find(stamp_key)};
    if(stamp_at == std::string_view::npos)
      continue;
    auto const at{parse_stamp(line.substr(stamp_at + stamp_key.size()))};
    if(not at)
      continue;

    auto const event_at{line.find(event_key, stamp_at)};
    if(event_at != std::string_view::npos)
      {
      auto const name_from{event_at + event_key.size()};
      auto const name_to{line.find('"', name_from)};
      std::string_view const event{
        name_to == std::string_view::npos ? std::string_view{} : line.substr(name_from, name_to - name_from)
      };
      if(std::ranges::find(special_events, event) != special_events.end()
         or std::ranges::find(simple_events, event, &simple_event_t::event) != simple_events.end())
        reader.line(*at, event, line);
      }
    // only after the line itself - a LoadGame ends the session before it at the line before it
    reader.last_line = std::max(reader.last_line, *at);
    }
  reader.close_session();
  return std::move(reader.logs);
  }

auto build_ledger(std::span<commander_log_t const> logs) -> ledger_t
  {
  ledger_t ledger;
  // the last price each commodity was bought for - what goods handed in at a construction site cost
  std::unordered_map<std::string, std::pair<int64_t, int64_t>> bought;

  std::optional<int64_t> reading;
  int64_t moved_since{};
  std::optional<time_point_t> contributed;
  time_point_t last_movement{};
  std::vector<gap_t> gaps;

  auto const book{[&ledger](time_point_t at, category_e category, int64_t amount, std::string_view what)
                  {
                    ledger.entries.push_back(
                      record_t{.at = at, .kind = kind_e::movement, .category = category, .amount = amount, .what = std::string{what}}
                    );
                  }};

  for(commander_log_t const & log: logs)
    {
    if(ledger.fid.empty())
      ledger.fid = log.fid;
    if(not log.name.empty())
      ledger.name = log.name;
    ledger.sessions.insert(ledger.sessions.end(), log.sessions.begin(), log.sessions.end());

    for(record_t const & record: log.records)
      switch(record.kind)
        {
        case kind_e::movement:
          ledger.entries.push_back(record);
          moved_since += record.amount;
          last_movement = record.at;
          if(record.category == category_e::trade and record.quantity > 0 and record.amount < 0)
            bought.insert_or_assign(record.what, std::pair{-record.amount, record.quantity});
          break;

        case kind_e::contribution:
          {
          contributed = record.at;
          auto const it{bought.find(record.what)};
          if(it == bought.end() or it->second.second == 0)
            break;
          // the cost moves over from trade - it nets out, the balance is not touched
          int64_t const cost{it->second.first * record.quantity / it->second.second};
          book(record.at, category_e::trade, cost, record.what);
          book(record.at, category_e::colonisation, -cost, record.what);
          break;
          }

        case kind_e::balance:
          {
          if(reading)
            {
            int64_t const residual{record.amount - (*reading + moved_since)};
            if(residual != 0)
              gaps.push_back(gap_t{
                .amount = residual,
                .read_at = record.at,
                .before = std::max(last_movement, ledger.balance_read_at),
                .contributed = contributed
              });
            }
          reading = record.amount;
          ledger.balance_read_at = record.at;
          moved_since = 0;
          contributed.reset();
          break;
          }
        }
    }

  for(size_t first{}; first < gaps.size();)
    {
    // a run of gaps close together that adds up to nothing is no money gone anywhere
    size_t last{first};
    int64_t sum{gaps[first].amount};
    while(sum != 0 and last + 1u < gaps.size() and gaps[last + 1u].read_at - gaps[first].read_at <= cancel_window)
      sum += gaps[++last].amount;
    if(sum == 0)
      {
      first = last + 1u;
      continue;
      }

    gap_t const & gap{gaps[first++]};
    if(gap.amount % squadron_step == 0)
      book(gap.before, category_e::squadron_bank, gap.amount, "balance reading");
    else if(gap.contributed and gap.amount > 0)
      book(*gap.contributed, category_e::colonisation_payout, gap.amount, "balance reading");
    else if(gap.amount >= squadron_large or gap.amount <= -squadron_large)
      {
      int64_t const round{(gap.amount + (gap.amount > 0 ? squadron_step : -squadron_step) / 2) / squadron_step * squadron_step};
      book(gap.before, category_e::squadron_bank, round, "balance reading");
      book(gap.before, category_e::unexplained, gap.amount - round, "balance reading");
      }
    else
      book(gap.before, category_e::unexplained, gap.amount, "balance reading");
    }

  // the residuals were booked a moment before the reading that found them, files' clocks may overlap
  std::ranges::stable_sort(ledger.entries, {}, &record_t::at);
  std::ranges::sort(ledger.sessions, {}, &session_t::from);
  ledger.balance_read = reading;
  if(reading)
    ledger.balance = *reading + moved_since;
  return ledger;
  }

auto summary_t::income() const noexcept -> int64_t
  {
  int64_t sum{};
  for(size_t ix{}; ix != category_count; ++ix)
    if(not is_transfer(category_e(ix)) and by_category[ix] > 0)
      sum += by_category[ix];
  return sum;
  }

auto summary_t::expenses() const noexcept -> int64_t
  {
  int64_t sum{};
  for(size_t ix{}; ix != category_count; ++ix)
    if(not is_transfer(category_e(ix)) and by_category[ix] < 0)
      sum += by_category[ix];
  return sum;
  }

auto summary_t::transfers() const noexcept -> int64_t
  { return by_category[size_t(category_e::carrier_transfer)] + by_category[size_t(category_e::squadron_bank)]; }

auto summary_t::per_hour() const noexcept -> std::optional<int64_t>
  {
  // under a few minutes the rate says more of the clock than of the play
  if(played < std::chrono::minutes{5})
    return std::nullopt;
  return int64_t(double(net()) * 3600.0 / double(played.count()));
  }

auto summarise(ledger_t const & ledger, period_e period, std::chrono::time_zone const * zone) -> std::vector<summary_t>
  {
  // keyed by the period's first local day, so they come out in order
  std::map<std::chrono::local_days, summary_t> periods;
  auto const period_of{[&periods, period, zone](time_point_t at) -> summary_t &
                       {
                         auto const day{std::chrono::floor<std::chrono::days>(to_local(zone, at))};
                         period_bounds_t bounds{bounds_of(day, period)};
                         auto [it, inserted]{periods.try_emplace(bounds.from)};
                         if(inserted)
                           {
                           it->second.from = to_sys(zone, bounds.from);
                           it->second.to = to_sys(zone, bounds.to);
                           it->second.label = std::move(bounds.label);
                           }
                         return it->second;
                       }};

  for(record_t const & entry: ledger.entries)
    {
    summary_t & summary{period_of(entry.at)};
    summary.by_category[size_t(entry.category)] += entry.amount;
    ++summary.count[size_t(entry.category)];
    }
  for(session_t const & session: ledger.sessions)
    for(time_point_t at{session.from}; at < session.to;)
      {
      summary_t & summary{period_of(at)};
      summary.played += overlap(session, summary.from, summary.to);
      at = summary.to;
      }

  // the balance at each period's end, walked back from the balance now
  if(ledger.balance)
    {
    int64_t balance{*ledger.balance};
    for(auto & [first_day, summary]: periods | std::views::reverse)
      {
      summary.closing_balance = balance;
      for(int64_t const amount: summary.by_category)
        balance -= amount;
      }
    }

  std::vector<summary_t> result;
  result.reserve(periods.size());
  for(auto & [first_day, summary]: periods | std::views::reverse)
    result.push_back(std::move(summary));
  return result;
  }

auto summarise_since(ledger_t const & ledger, time_point_t from) -> summary_t
  {
  summary_t summary{.from = from, .to = from};
  for(record_t const & entry: ledger.entries)
    if(entry.at >= from)
      {
      summary.by_category[size_t(entry.category)] += entry.amount;
      ++summary.count[size_t(entry.category)];
      summary.to = std::max(summary.to, entry.at);
      }
  for(session_t const & session: ledger.sessions)
    {
    summary.to = std::max(summary.to, session.to);
    summary.played += overlap(session, from, time_point_t::max());
    }
  summary.closing_balance = ledger.balance;
  return summary;
  }
  }  // namespace credits
