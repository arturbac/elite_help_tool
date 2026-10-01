#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <simple_enum/simple_enum.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief the player's money: where it came from and where it went, read from the journal alone
///
/// The journal names almost every credit the player earns or spends - a market sale, a mission's reward,
/// a refuel, a module bought - and twice in a while says what the balance itself is: on LoadGame at the start
/// of each session, and on every transfer to or from a fleet carrier's bank. Between two such readings the
/// movements named should add up to the difference; most of the time they do, to the credit. When they do
/// not, the difference is booked as well, so the totals always agree with the game, under what most likely
/// moved it:
/// - a squadron bank deposit or withdrawal: no event is ever written for one, and the amounts are round
///   millions as typed in the squadron's own screen;
/// - a colonisation payout: delivering to a construction site pays for the goods (about 1.3 times what they
///   were bought for, read off the history), but ColonisationContribution carries no amount;
/// - whatever else is left, mostly small and often explained by the readings' own timing: the balance
///   is read when the game writes LoadGame, a purchase made in the same second may land either side of it.
///
/// Crew wages are paid out of earnings and written as NpcCrewPaidWage; they come off the balance, except right
/// after a sale of exploration data or on-foot goods, whose sums the game writes already net of the crew's cut
namespace credits
  {
using time_point_t = std::chrono::sys_seconds;

///\brief where a movement belongs - the columns of the Credits window, in this order
enum struct category_e : uint8_t
  {
  exploration,
  exobiology,
  trade,
  missions,
  ///\brief bounty vouchers and combat bonds handed in
  combat,
  ///\brief on-foot goods and data sold at a bar, bought from a shop
  on_foot_goods,
  ///\brief community goal rewards, search and rescue, Powerplay salary and the like
  other_income,
  ships_and_modules,
  suits_and_weapons,
  ///\brief fuel, repairs, ammunition, limpets, ship and module transfers, taxis
  upkeep,
  ///\brief the insurance paid on a ship lost
  rebuy,
  fines,
  crew,
  engineers,
  powerplay,
  carrier,
  ///\brief goods bought for a construction site and handed in there - moved here from trade on delivery
  colonisation,
  ///\brief told from the balance, the journal names no amount - see the namespace's own note
  colonisation_payout,
  ///\brief to and from a fleet carrier's bank - the player's own money either way
  carrier_transfer,
  ///\brief told from the balance, the journal names no amount - see the namespace's own note
  squadron_bank,
  ///\brief the balance moved by this much and no event says why
  unexplained
  };

consteval auto adl_enum_bounds(category_e)
  {
  using enum category_e;
  return simple_enum::adl_info{exploration, unexplained};
  }

inline constexpr size_t category_count{size_t(category_e::unexplained) + 1u};

///\brief the column header shown for a category
[[nodiscard]]
auto category_label(category_e category) -> std::string_view;

///\brief the amount is not written anywhere in the journal, only the balance it moved
[[nodiscard]]
constexpr auto is_inferred(category_e category) noexcept -> bool
  {
  return category == category_e::colonisation_payout or category == category_e::squadron_bank
         or category == category_e::unexplained;
  }

///\brief moves the player's own money between pockets - not earned, not spent
[[nodiscard]]
constexpr auto is_transfer(category_e category) noexcept -> bool
  { return category == category_e::carrier_transfer or category == category_e::squadron_bank; }

enum struct kind_e : uint8_t
  {
  ///\brief credits gained (positive) or paid (negative)
  movement,
  ///\brief the balance as the game itself said it was - amount is the balance, not a change
  balance,
  ///\brief goods handed in at a construction site: what names the commodity, quantity how many
  contribution
  };

consteval auto adl_enum_bounds(kind_e)
  {
  using enum kind_e;
  return simple_enum::adl_info{movement, contribution};
  }

///\brief one line of the journal that matters for money, read
struct record_t
  {
  time_point_t at;
  kind_e kind{};
  category_e category{};
  int64_t amount{};
  ///\brief a market purchase's count of goods, a contribution's
  int64_t quantity{};
  ///\brief the event's name; for a market purchase or a contribution, the commodity's id in lower case
  std::string what;

  auto operator==(record_t const &) const -> bool = default;
  };

///\brief from LoadGame to the last line before the game stopped or the next LoadGame
struct session_t
  {
  time_point_t from;
  time_point_t to;

  auto operator==(session_t const &) const -> bool = default;
  };

///\brief what one journal file says of one commander's money
struct commander_log_t
  {
  std::string fid;
  std::string name;
  std::vector<record_t> records;
  std::vector<session_t> sessions;
  };

///\brief a whole journal file read for money, one entry per commander logged into it (practically always one)
///\detail only the lines of the events that move credits are parsed as JSON; the rest is passed over
/// by their event name, so the whole history of a long-running commander is read in a few seconds
[[nodiscard]]
auto scan_journal(std::string_view text) -> std::vector<commander_log_t>;

///\brief one commander's money, every file of theirs put together
struct ledger_t
  {
  std::string fid;
  std::string name;
  ///\brief movements only, oldest first - with what the balance said but no event did, and with goods
  /// handed in for colonisation moved over from trade
  std::vector<record_t> entries;
  std::vector<session_t> sessions;
  ///\brief the game's own reading, the latest one
  std::optional<int64_t> balance_read;
  time_point_t balance_read_at;
  ///\brief the balance now: the latest reading and every movement after it
  std::optional<int64_t> balance;
  };

///\brief puts together every file's log of one commander, in the files' order (oldest first)
[[nodiscard]]
auto build_ledger(std::span<commander_log_t const> logs) -> ledger_t;

enum struct period_e : uint8_t
  {
  day,
  week,
  month,
  quarter,
  year
  };

consteval auto adl_enum_bounds(period_e)
  {
  using enum period_e;
  return simple_enum::adl_info{day, year};
  }

///\brief what a stretch of time brought in and took away
struct summary_t
  {
  ///\brief the stretch's start, local midnight of its first day for a calendar period
  time_point_t from;
  time_point_t to;
  ///\brief "2026-10-01", "2026 W40", "2026-10", "2026 Q4", "2026"
  std::string label;
  std::array<int64_t, category_count> by_category{};
  std::array<uint32_t, category_count> count{};
  std::chrono::seconds played{};
  ///\brief the balance when the stretch ended, or now for the current one
  std::optional<int64_t> closing_balance;

  ///\brief the categories that came out ahead, together - transfers left aside
  [[nodiscard]]
  auto income() const noexcept -> int64_t;
  ///\brief the categories that came out behind, together (negative) - transfers left aside
  [[nodiscard]]
  auto expenses() const noexcept -> int64_t;
  [[nodiscard]]
  auto transfers() const noexcept -> int64_t;
  ///\brief earned less spent - what the player made, apart from moving money between pockets
  [[nodiscard]]
  auto net() const noexcept -> int64_t
    { return income() + expenses(); }
  ///\brief net per hour played, while there is any time played
  [[nodiscard]]
  auto per_hour() const noexcept -> std::optional<int64_t>;
  };

///\brief the ledger by calendar period of the zone given, newest first, only the periods with anything in them
///\param zone where a day starts; nullptr for UTC
[[nodiscard]]
auto summarise(ledger_t const & ledger, period_e period, std::chrono::time_zone const * zone) -> std::vector<summary_t>;

///\brief everything from a moment on - for the current session, its LoadGame
[[nodiscard]]
auto summarise_since(ledger_t const & ledger, time_point_t from) -> summary_t;
  }  // namespace credits
