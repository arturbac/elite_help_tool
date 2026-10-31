#include <holdings.h>

#include <algorithm>
#include <ranges>
#include <set>
#include <tuple>

namespace holdings
  {
auto totals(std::span<held_t const> backpack, std::span<held_t const> locker) -> counts_t
  {
  counts_t result;
  for(std::span<held_t const> list: {backpack, locker})
    for(held_t const & item: list)
      if(kind_of(item.category))
        result[item.name] += item.count;
  return result;
  }

auto visit_balance(counts_t const & docked, counts_t const & leaving) -> counts_t
  {
  std::set<std::string, std::less<>> names;
  for(counts_t const * list: {&docked, &leaving})
    for(auto const & [name, count]: *list)
      names.insert(name);
  auto const of = [](counts_t const & list, std::string const & name) -> int64_t
  {
    auto const it{list.find(name)};
    return it == list.end() ? 0 : it->second;
  };
  // what the lists lost went into the store, what they gained came out of it
  counts_t result;
  for(std::string const & name: names)
    if(int64_t const delta{of(docked, name) - of(leaving, name)}; delta != 0)
      result[name] = delta;
  return result;
  }

auto rows(
  std::span<held_t const> backpack,
  std::span<held_t const> locker,
  std::span<counts_t const> stores,
  std::map<std::string, std::pair<std::string, std::string>, std::less<>> const & names
) -> std::vector<row_t>
  {
  std::map<std::string, row_t, std::less<>> by_name;
  auto const take = [&](std::string const & name, std::string const & shown, std::string_view category) -> row_t *
  {
    // a kind already seen in another place is known, whatever this one says of it
    if(auto const seen{by_name.find(name)}; seen != by_name.end())
      {
      if(seen->second.shown.empty())
        seen->second.shown = shown;
      return &seen->second;
      }
    std::string_view known_category{category};
    std::string known_shown{shown};
    if(auto const it{names.find(name)}; it != names.end())
      {
      if(known_shown.empty())
        known_shown = it->second.first;
      if(known_category.empty())
        known_category = it->second.second;
      }
    auto const kind{kind_of(known_category)};
    if(not kind)
      return nullptr;
    auto [it, fresh]{by_name.try_emplace(
      name, row_t{.kind = *kind, .name = name, .shown = known_shown, .stores = std::vector<int64_t>(stores.size())}
    )};
    if(it->second.shown.empty())
      it->second.shown = known_shown;
    return &it->second;
  };

  for(held_t const & item: backpack)
    if(row_t * row{take(item.name, item.shown, item.category)})
      row->backpack += item.count;
  for(held_t const & item: locker)
    if(row_t * row{take(item.name, item.shown, item.category)})
      row->locker += item.count;
  for(auto const & [ix, store]: stores | std::views::enumerate)
    for(auto const & [name, count]: store)
      if(count > 0)
        if(row_t * row{take(name, {}, {})})
          row->stores[size_t(ix)] += count;

  std::vector<row_t> result;
  result.reserve(by_name.size());
  for(auto & [name, row]: by_name)
    {
    if(row.shown.empty())
      row.shown = name;
    result.push_back(std::move(row));
    }
  std::ranges::sort(result, {}, [](row_t const & r) { return std::tie(r.kind, r.shown); });
  return result;
  }

auto with_junk(std::vector<row_t> rows, std::set<std::string, std::less<>> const & junk, bool hide)
  -> std::vector<row_t>
  {
  for(row_t & row: rows)
    row.junk = junk.contains(row.name);
  if(hide)
    std::erase_if(rows, [](row_t const & row) { return row.junk; });
  return rows;
  }
  }  // namespace holdings
