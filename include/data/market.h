#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

///\brief database rows: commodity markets and trading - readings, the dictionary, supply for missions and trade rates
namespace info
  {
///\brief a commodity's name as a key the three sources agree on - a construction site's "$cmmcomposite_name;",
/// the market's "CMM Composite" and the hold's "cmmcomposite" all become "cmmcomposite"
[[nodiscard]]
inline auto commodity_key(std::string_view name) -> std::string
  {
  if(name.starts_with('$'))
    name.remove_prefix(1u);
  if(name.ends_with("_name;") or name.ends_with("_Name;"))
    name.remove_suffix(6u);
  std::string key;
  for(char const c: name)
    if((c >= 'a' and c <= 'z') or (c >= '0' and c <= '9'))
      key.push_back(c);
    else if(c >= 'A' and c <= 'Z')
      key.push_back(char(c - 'A' + 'a'));
  return key;
  }

///\brief when we last read this station's market
///\detail the contents themselves come from Market.json, which cannot be rebuilt, so the time of
/// the reading belongs to the database gathered live
struct market_info_t
  {
  uint64_t market_id;
  std::chrono::sys_seconds updated;
  ///\brief the controlling faction seen at the moment of this reading, to notice a takeover since -
  /// what is legal to trade here can depend on who runs the place, and the game never warns of it
  std::string controlling_faction{};
  };

///\brief the commodity dictionary; mean_price is the galactic average, a constant of the commodity
struct commodity_t
  {
  uint64_t id;
  std::string name;
  std::string category;
  uint32_t mean_price;
  ///\brief the commodity key of the game's internal name - the hold and the construction sites name a
  /// commodity by it, and it is not always the shown one: Land Enrichment Systems is
  /// "terrainenrichmentsystems". Empty in rows written before it was kept, until a market shows it again
  std::string key;
  };

///\brief the newest market reading, one row per commodity
struct market_item_t
  {
  int64_t oid{-1};
  uint64_t market_id;
  uint64_t commodity_id;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t stock;
  uint32_t demand;
  ///\brief whether the station really produces and buys this commodity - zero stock may be a passing gap
  bool producer;
  bool consumer;
  };

///\brief a market row already joined with the commodity dictionary, ready for the window
struct market_entry_t
  {
  std::string name;
  std::string category;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t mean_price;
  uint32_t stock;
  uint32_t demand;
  ///\brief whether the station really produces and buys this commodity
  bool producer;
  bool consumer;
  ///\brief the commodity key of the game's internal name, empty when the dictionary has not learnt it yet
  std::string key;
  };

///\brief how much of what has to be brought in total, once open missions are summed
struct cargo_need_t
  {
  std::string commodity;
  uint32_t count;
  };

///\brief a place where what a mission requires can be bought
struct supply_option_t
  {
  uint64_t market_id;
  std::string station;
  std::string station_type;
  std::string system;
  std::string commodity;
  uint32_t needed;
  uint32_t stock;
  uint32_t buy_price;
  };

///\brief whether the commodity cannot be bought, only mined
///
/// a station can pay very well for such a resource, but for a trader it is a dead end -
/// nobody will sell it to him. The list is fixed knowledge about the game, so it sits in code, not in the database
[[nodiscard]]
auto is_mining_only(std::string_view commodity) noexcept -> bool;

///\brief a trade rate: buy there, sell here
struct trade_option_t
  {
  uint64_t market_id;
  std::string station;
  std::string system;
  std::string commodity;
  uint32_t buy_price;
  uint32_t sell_price;
  uint32_t stock;
  uint32_t demand;
  };
  }  // namespace info
