#include <bar_sales.h>
#include <algorithm>
#include <set>

namespace bar
  {
auto port_prices(std::span<port_sale_row_t const> rows) -> std::map<std::string, double>
  {
  struct sale_t
    {
    uint64_t price;
    std::vector<std::pair<std::string, uint32_t>> items;
    };
  std::map<int64_t, sale_t> by_sale;
  for(port_sale_row_t const & row: rows)
    {
    auto & sale{by_sale[row.sale_oid]};
    sale.price = row.price;
    if(row.count != 0u)
      sale.items.emplace_back(row.name, row.count);
    }

  std::map<std::string, double> known;
  for(bool found{true}; found;)
    {
    found = false;
    for(auto const & [oid, sale]: by_sale)
      {
      std::pair<std::string, uint32_t> const * unknown{};
      size_t unknown_count{};
      double rest{static_cast<double>(sale.price)};
      for(auto const & item: sale.items)
        if(auto it{known.find(item.first)}; it != known.end())
          rest -= it->second * item.second;
        else
          {
          unknown = &item;
          ++unknown_count;
          }
      // a negative rest is a sale that does not add up - better no price than a wrong one
      if(unknown_count == 1u and rest > 0.0)
        {
        known.emplace(unknown->first, rest / unknown->second);
        found = true;
        }
      }
    }
  return known;
  }

namespace
  {
  auto is_absence(std::chrono::sys_seconds from, std::chrono::sys_seconds to) -> bool { return to - from >= absence_gap; }

  ///\brief readings with nothing on the whole shelf - the game writes the file before the bar has loaded, and
  /// a fall to zero followed by a rise minutes later is no sale
  auto empty_readings(std::span<info::carrier_stock_t const> history) -> std::set<std::chrono::sys_seconds>
    {
    std::map<std::chrono::sys_seconds, bool> anything;
    for(info::carrier_stock_t const & reading: history)
      anything[reading.timestamp] = anything[reading.timestamp] or reading.stock != 0u or reading.demand != 0u;
    std::set<std::chrono::sys_seconds> empty;
    for(auto const & [moment, something]: anything)
      if(not something)
        empty.insert(moment);
    return empty;
    }
  }  // namespace

auto sales(std::span<info::carrier_stock_t const> history) -> std::vector<item_sales_t>
  {
  std::vector<item_sales_t> result;
  auto const empty{empty_readings(history)};
  info::carrier_stock_t const * previous{};
  for(info::carrier_stock_t const & reading: history)
    {
    if(empty.contains(reading.timestamp))
      continue;
    if(previous == nullptr or previous->name != reading.name)
      {
      result.push_back(item_sales_t{
        .name = reading.name,
        .localised = reading.localised,
        .category = reading.category,
        .price = reading.price,
        .stock = reading.stock,
        .sold = 0u,
        .revenue = 0u,
        .absences_listed = 0u,
        .absences_sold = 0u,
        .bought_in = 0u
      });
      previous = &reading;
      continue;
      }

    item_sales_t & item{result.back()};
    uint32_t const sold{previous->stock > reading.stock ? previous->stock - reading.stock : 0u};
    item.sold += sold;
    item.revenue += uint64_t(sold) * previous->price;
    item.bought_in += previous->demand > reading.demand ? previous->demand - reading.demand : 0u;
    if(is_absence(previous->timestamp, reading.timestamp) and previous->stock != 0u)
      {
      ++item.absences_listed;
      item.absences_sold += sold != 0u ? 1u : 0u;
      }
    item.price = reading.price;
    item.stock = reading.stock;
    previous = &reading;
    }
  return result;
  }

auto item_values(std::span<item_sales_t const> sales, std::map<std::string, double> const & port)
  -> std::map<std::string, item_value_t>
  {
  std::map<std::string, item_value_t> values;
  for(auto const & [name, price]: port)
    values[name] = item_value_t{.value = price, .from_bar = false};
  for(item_sales_t const & item: sales)
    if(item.absences_listed != 0u)
      values[item.name] = item_value_t{
        .value = double(item.price) * item.absences_sold / item.absences_listed, .from_bar = true
      };
  return values;
  }

auto mission_kind(std::string_view type) -> std::string
  {
  if(type.starts_with("Mission_"))
    type.remove_prefix(8);
  // the numbers are the game's versions of one kind of mission - Heist_Covert_004 and _007 are the same work
  while(true)
    {
    auto const cut{type.find_last_of('_')};
    if(cut == std::string_view::npos or cut + 1u == type.size())
      break;
    auto const tail{type.substr(cut + 1u)};
    if(not std::ranges::all_of(tail, [](char c) { return c >= '0' and c <= '9'; }))
      break;
    type = type.substr(0, cut);
    }
  return std::string{type};
  }

auto mission_values(std::span<mission_reward_row_t const> rows, std::map<std::string, item_value_t> const & values)
  -> std::vector<mission_value_t>
  {
  struct bucket_t
    {
    std::set<uint64_t> missions;
    uint64_t credits{};
    double materials{};
    std::set<std::string> unvalued;
    std::map<std::string, uint32_t> given;
    };
  std::map<std::string, bucket_t> by_type;
  for(mission_reward_row_t const & row: rows)
    {
    bucket_t & bucket{by_type[mission_kind(row.type)]};
    // a mission comes once per kind of its reward, its credits count once
    if(bucket.missions.insert(row.mission_id).second)
      bucket.credits += row.reward;
    if(row.name.empty() or row.count == 0u)
      continue;
    bucket.given[row.name] += row.count;
    if(auto it{values.find(row.name)}; it != values.end())
      bucket.materials += it->second.value * row.count;
    else
      bucket.unvalued.insert(row.name);
    }

  std::vector<mission_value_t> result;
  for(auto & [type, bucket]: by_type)
    {
    std::vector<std::pair<std::string, uint32_t>> given{bucket.given.begin(), bucket.given.end()};
    std::ranges::sort(given, std::ranges::greater{}, &std::pair<std::string, uint32_t>::second);
    result.push_back(mission_value_t{
      .type = type,
      .missions = uint32_t(bucket.missions.size()),
      .credits = bucket.credits,
      .materials = bucket.materials,
      .unvalued_kinds = uint32_t(bucket.unvalued.size()),
      .rewards = std::move(given)
    });
    }
  return result;
  }

auto absences(std::span<info::carrier_stock_t const> history) -> uint32_t
  {
  auto const empty{empty_readings(history)};
  std::set<std::chrono::sys_seconds> moments;
  for(info::carrier_stock_t const & reading: history)
    if(not empty.contains(reading.timestamp))
      moments.insert(reading.timestamp);
  uint32_t count{};
  for(auto it{moments.begin()}; it != moments.end() and std::next(it) != moments.end(); ++it)
    count += is_absence(*it, *std::next(it)) ? 1u : 0u;
  return count;
  }
  }  // namespace bar
