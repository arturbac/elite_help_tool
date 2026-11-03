#pragma once

#include <data/market.h>
#include <events/station.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

///\brief where a handful of commodities can be bought - the markets read, ranked by how many of them each one has
namespace buy_search
  {
///\brief the kinds of place a search can keep or leave out
enum struct place_e : uint8_t
  {
  space_station,
  outpost,
  surface_port,
  settlement,
  carrier,
  ///\brief a megaship, a building site, a place of unknown type - never offered as a source
  other
  };

[[nodiscard]]
auto place_of(std::string_view station_type) -> place_e;

///\brief whether a market row is the station buying the commodity - what it produces it takes back too, at a loss,
/// which corrects a purchase made too large and is no market for it
[[nodiscard]]
constexpr auto station_buys(bool producer, bool consumer) noexcept -> bool
  {
  return consumer and not producer;
  }

enum struct pad_e : uint8_t
  {
  none,
  small,
  medium,
  large
  };

///\brief the largest pad a place has - unknown when its pads were never read
[[nodiscard]]
auto largest_pad(
  std::optional<uint16_t> small, std::optional<uint16_t> medium, std::optional<uint16_t> large
) -> std::optional<pad_e>;

///\brief a commodity to buy, by its readable name, and how much of it - 0 for any stock at all
struct wanted_t
  {
  std::string commodity;
  uint32_t tonnes{};
  };

///\brief one commodity of a market, as the search reads it
struct offer_t
  {
  std::string commodity;
  int64_t buy_price{};
  int64_t stock{};
  int64_t mean_price{};
  };

///\brief a market holding at least one of the wanted commodities
struct place_t
  {
  uint64_t market_id{};
  std::string system;
  std::string station;
  std::string station_type;
  std::optional<pad_e> pad;
  double dist_ls{};
  ///\brief where its system lies, in light years
  std::optional<std::array<double, 3>> position;
  ///\brief light years from where the commander is; unknown when either position is not known
  std::optional<double> ly;
  std::chrono::sys_seconds updated{};
  ///\brief the wanted commodities it sells, in the order they were asked for
  std::vector<offer_t> offers;
  ///\brief empty for one's own reading, "eddn" for one heard on the relay
  std::string source;
  };

///\brief how the places that have as many of the wanted go
enum struct order_e : uint8_t
  {
  ///\brief the nearest first
  distance,
  ///\brief the lowest price first - for buying
  cheapest,
  ///\brief the highest price first - for selling
  dearest
  };

///\brief what the search keeps
struct filter_t
  {
  std::set<place_e> places;
  ///\brief the smallest of the largest pads accepted
  pad_e min_pad{pad_e::none};
  bool unknown_pads{true};
  std::chrono::days max_age{30};
  ///\brief whether the markets heard on EDDN's relay count
  bool eddn{true};
  ///\brief the farthest from where the commander is, in light years; a place of unknown distance is left out with
  /// it - none for no limit
  std::optional<double> max_ly{};
  ///\brief after the number of hits; a tie goes to the nearer
  order_e order{order_e::distance};
  };

///\brief the rows of the database gathered by market, with the distance from where the commander is
[[nodiscard]]
auto places_of(std::vector<info::buy_offer_t> const & rows, std::optional<std::array<double, 3>> const & here)
  -> std::vector<place_t>;

///\brief whether an offer covers the wanted amount: on sale, and as much in stock as wanted
[[nodiscard]]
auto covers(offer_t const & offer, wanted_t const & wanted) -> bool;

///\brief how many of the wanted commodities the place has in the wanted amounts
[[nodiscard]]
auto hits(place_t const & place, std::vector<wanted_t> const & wanted) -> unsigned;

///\brief the price of the wanted it has in the wanted amounts, each amount at least a tonne - an amount of 0 counts
/// as the price of one
[[nodiscard]]
auto price_sum(place_t const & place, std::vector<wanted_t> const & wanted) -> int64_t;

///\brief what the wanted amounts cost there, as price_sum - only when it has them all
[[nodiscard]]
auto cost(place_t const & place, std::vector<wanted_t> const & wanted) -> std::optional<int64_t>;

[[nodiscard]]
auto passes(place_t const & place, filter_t const & filter, std::chrono::sys_seconds now) -> bool;

///\brief the places kept by the filter that have at least one of the wanted, most hits first, then the nearest
[[nodiscard]]
auto rank(
  std::vector<place_t> places, std::vector<wanted_t> const & wanted, filter_t const & filter, std::chrono::sys_seconds now
) -> std::vector<place_t>;

///\brief two places that together have every wanted commodity
struct pair_t
  {
  std::size_t first{};
  std::size_t second{};
  ///\brief to the nearer of the two and on to the other; the light years between them count, the way to the first too
  double ly{};
  };

///\brief the best pairs of ranked places that together have all - none when one place has all, or positions are unknown
[[nodiscard]]
auto covering_pairs(
  std::vector<place_t> const & ranked,
  std::vector<wanted_t> const & wanted,
  std::size_t most
) -> std::vector<pair_t>;

///\brief the jumps a distance takes at the ship's range - a floor, a real route may need more
[[nodiscard]]
auto jumps(double ly, float range) -> std::optional<unsigned>;

///\brief the pads of every place one journal docked at, the last Docked giving them winning
[[nodiscard]]
auto pads_in_journal(std::istream & journal) -> std::map<uint64_t, events::landing_pads_t>;

///\brief what the list of markets to read says of a place
enum struct market_state_e : uint8_t
  {
  never_read,
  ///\brief someone else holds the place since the reading - after a war, say - so its market may differ
  owner_changed,
  read
  };

[[nodiscard]]
auto market_state(info::market_to_update_t const & place) -> market_state_e;

///\brief whether a place belongs on the list of markets to read at all - a building site, a carrier or a place of
/// unknown kind has no market of its own worth reading
[[nodiscard]]
auto worth_reading(info::market_to_update_t const & place) -> bool;

///\brief the pads found, and the newest journal there was to read
struct journal_pads_t
  {
  std::map<uint64_t, events::landing_pads_t> found;
  std::string newest_journal;
  };

///\brief the pads of the wanted places as the journals of the directory newer than `after` tell, newest journal first,
/// read until each is found or the journals run out
[[nodiscard]]
auto pads_from_journals(std::filesystem::path const & journal_dir, std::set<uint64_t> wanted, std::string_view after)
  -> journal_pads_t;
  }  // namespace buy_search
