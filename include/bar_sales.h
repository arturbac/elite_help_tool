#pragma once
#include <data/carrier.h>
#include <chrono>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

///\brief what a carrier's bar sold, read from its shelf
///
/// The journal never says a player bought something at one's bar. The shelf does: it is read at every visit
/// to the bartender, on arriving before anything is added and again after, so a fall in stock between two
/// readings is what was sold in between, and a rise what the owner added. The price is the one of the
/// earlier reading - the one the buyer saw
namespace bar
  {
///\brief one kind sold at a port's bartender, as a row of a sale
struct port_sale_row_t
  {
  int64_t sale_oid;
  uint64_t price;
  std::string name;
  uint32_t count;
  };

///\brief what a port's bartender pays, by the game's name of the kind - the known price list, without the few
/// rare kinds it has no price for
[[nodiscard]]
auto bartender_prices() -> std::span<std::pair<std::string_view, uint32_t> const>;

///\brief a port's bartender pays the same for a kind everywhere, and a sale gives only its whole sum. The price
/// list comes first; the kinds missing from it come out of the sales themselves: a sale of one kind gives its
/// price, and a sale in which all kinds but one are known gives that one, over and over until nothing more comes out
[[nodiscard]]
auto port_prices(std::span<port_sale_row_t const> rows) -> std::map<std::string, double>;

struct item_sales_t
  {
  std::string name;
  std::string localised;
  std::string category;
  ///\brief the latest reading
  uint32_t price;
  uint32_t stock;
  uint32_t sold;
  ///\brief each fall in stock at the price the shelf showed before it
  uint64_t revenue;
  ///\brief the absences it lay on the shelf through, and those it sold in - a price that puts buyers off
  /// shows as many of the first and few of the second
  uint32_t absences_listed;
  uint32_t absences_sold;
  ///\brief what players sold to the bar against its demand
  uint32_t bought_in;
  ///\brief what the owner put on the shelf: the stock of the first reading and every rise since
  uint32_t put_up;
  };

///\brief a gap between readings this long is an absence - restocking readings come seconds or minutes apart
inline constexpr std::chrono::minutes absence_gap{20};

///\brief the sales of every kind, from the readings of one carrier ordered by kind and time
[[nodiscard]]
auto sales(std::span<info::carrier_stock_t const> history) -> std::vector<item_sales_t>;

///\brief how many absences the readings span
[[nodiscard]]
auto absences(std::span<info::carrier_stock_t const> history) -> uint32_t;
///\brief what one kind is worth to the owner: the bar's price times the share of absences it sold in - a price
/// nobody pays is worth little. A kind never put on the shelf takes the port's price, when that is known
struct item_value_t
  {
  double value;
  ///\brief the value came from the bar; false for a port's price or for nothing known
  bool from_bar;
  ///\brief what one piece put on the bar has brought so far: the revenue over the pieces put up - zero off the bar
  double sold_per_piece;
  };

[[nodiscard]]
auto item_values(std::span<item_sales_t const> sales, std::map<std::string, double> const & port)
  -> std::map<std::string, item_value_t>;

///\brief one completed mission and one kind of its material reward - a mission with none has an empty name
struct mission_reward_row_t
  {
  uint64_t mission_id;
  std::string type;
  std::string name;
  ///\brief the game's category of the reward: Data, Item or Component - empty with no reward
  std::string category;
  uint32_t count;
  };

///\brief what the missions of one type brought in rewards of one category, valued at the bar
struct mission_value_t
  {
  std::string type;
  ///\brief all the missions of the type, those that gave nothing of the category too - the average is per mission taken
  uint32_t missions;
  double materials;
  ///\brief what the rewards brought at the bar, each piece at what one put up there has brought so far
  double sold;
  ///\brief the reward kinds with no value known - the total says less than it could
  uint32_t unvalued_kinds;
  ///\brief the kinds given most, by count, with how many
  std::vector<std::pair<std::string, uint32_t>> rewards;
  };

///\brief a mission's type without the game's prefix and version number - the kind of work it is
[[nodiscard]]
auto mission_kind(std::string_view type) -> std::string;

///\brief the mission types that gave rewards of the category, what those rewards are worth
[[nodiscard]]
auto mission_values(
  std::span<mission_reward_row_t const> rows, std::map<std::string, item_value_t> const & values, std::string_view category
) -> std::vector<mission_value_t>;
  }  // namespace bar
