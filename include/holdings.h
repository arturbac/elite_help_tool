#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief what is held of data, goods and assets: in the backpack, in the ship's locker and in our carriers' stores
///\detail the backpack and the locker are the game's own lists. A carrier's store is in no journal: what the backpack
/// and the locker have less of on leaving one of our carriers than on docking was left there, what they have more of
/// was taken from it - less what went over its bartender's counter - and a count may be set by hand
namespace holdings
  {
enum struct kind_e : uint8_t
  {
  data,
  goods,
  assets
  };

///\brief the kind of a micro resource from the category the game files it under - the locker's (Data, Item,
/// Component) or the bartender's (Data, Goods, Assets); consumables and the unknown are none of the three
[[nodiscard]]
constexpr auto kind_of(std::string_view category) noexcept -> std::optional<kind_e>
  {
  if(category == "Data")
    return kind_e::data;
  if(category == "Item" or category == "Goods")
    return kind_e::goods;
  if(category == "Component" or category == "Assets")
    return kind_e::assets;
  return std::nullopt;
  }

///\brief one kind in the backpack or the locker, by its internal name
struct held_t
  {
  std::string name;
  std::string shown;
  std::string category;
  uint32_t count{};
  };

using counts_t = std::map<std::string, int64_t, std::less<>>;

///\brief the data, goods and assets of some lists together, by internal name - what a visit's balance compares
[[nodiscard]]
auto totals(std::span<held_t const> backpack, std::span<held_t const> locker) -> counts_t;

///\brief what a visit to one of our carriers left in its store (positive) or took from it (negative)
///\param counter what its bartender's counter gave (bought, positive) or took (handed in, negative) meanwhile
[[nodiscard]]
auto visit_balance(counts_t const & docked, counts_t const & leaving, counts_t const & counter) -> counts_t;

struct row_t
  {
  kind_e kind;
  std::string name;
  std::string shown;
  uint32_t backpack{};
  uint32_t locker{};
  ///\brief one count a carrier, in the order the stores were given
  std::vector<int64_t> stores;
  ///\brief marked as junk by the commander
  bool junk{};
  };

///\brief every kind of data, goods and assets held anywhere, by kind and then name
///\param stores what each of our carriers keeps, by internal name
///\param names the readable name and the category of a kind the places themselves do not give
[[nodiscard]]
auto rows(
  std::span<held_t const> backpack,
  std::span<held_t const> locker,
  std::span<counts_t const> stores,
  std::map<std::string, std::pair<std::string, std::string>, std::less<>> const & names
) -> std::vector<row_t>;
///\brief marks the rows of the kinds named junk, and leaves those out when they are to be hidden
[[nodiscard]]
auto with_junk(std::vector<row_t> rows, std::set<std::string, std::less<>> const & junk, bool hide)
  -> std::vector<row_t>;
  }  // namespace holdings
