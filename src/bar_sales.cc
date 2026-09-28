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
