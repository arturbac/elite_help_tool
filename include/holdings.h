#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief what is held of data, goods and assets: in the backpack, in the ship's locker and on one's own carrier's bar
///\detail the backpack and the locker are the game's own lists. The bar is known exactly only at a reading of its
/// shelf; after it, what was handed in at the bar adds to it and what was bought there takes from it, and a count
/// set by hand - for what other commanders bought, which no journal says - starts the item afresh
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

///\brief one kind on the bar's shelf at its last reading
struct shelf_t
  {
  std::string name;
  std::string shown;
  std::string category;
  uint32_t stock{};
  };

///\brief something handed in at the bar (positive) or bought there (negative)
struct move_t
  {
  std::chrono::sys_seconds when;
  std::string name;
  int64_t delta{};
  };

///\brief a count on the bar set by hand
struct set_t
  {
  std::chrono::sys_seconds when;
  std::string name;
  int64_t count{};
  };

///\brief the bar of one carrier as far as it is known
struct bar_t
  {
  ///\brief when the shelf was last read; nullopt before the first reading
  std::optional<std::chrono::sys_seconds> read;
  std::vector<shelf_t> shelf;
  std::vector<set_t> sets;
  std::vector<move_t> moves;
  };

///\brief the count of a kind on the bar: the later of the reading and the count set by hand, then every move after it
[[nodiscard]]
auto on_bar(bar_t const & bar, std::string_view name) -> int64_t;

struct row_t
  {
  kind_e kind;
  std::string name;
  std::string shown;
  uint32_t backpack{};
  uint32_t locker{};
  int64_t bar{};
  };

///\brief every kind of data, goods and assets held anywhere of the three places, by kind and then name
///\param names the readable name and the category of a kind the places themselves do not give
[[nodiscard]]
auto rows(
  std::span<held_t const> backpack,
  std::span<held_t const> locker,
  bar_t const & bar,
  std::map<std::string, std::pair<std::string, std::string>, std::less<>> const & names
) -> std::vector<row_t>;
  }  // namespace holdings
