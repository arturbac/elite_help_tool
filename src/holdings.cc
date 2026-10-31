#include <holdings.h>

#include <algorithm>
#include <ranges>
#include <tuple>

namespace holdings
  {
auto on_bar(bar_t const & bar, std::string_view name) -> int64_t
  {
  // at a reading every kind not on the shelf stood at nought
  std::optional<std::chrono::sys_seconds> since{bar.read};
  int64_t count{};
  if(bar.read)
    if(auto const it{std::ranges::find(bar.shelf, name, &shelf_t::name)}; it != bar.shelf.end())
      count = it->stock;
  for(set_t const & set: bar.sets)
    if(set.name == name and (not since or set.when >= *since))
      {
      since = set.when;
      count = set.count;
      }
  for(move_t const & move: bar.moves)
    // a move in the second of the reading is taken as already in it
    if(move.name == name and (not since or move.when > *since))
      count += move.delta;
  return std::max<int64_t>(count, 0);
  }

auto rows(
  std::span<held_t const> backpack,
  std::span<held_t const> locker,
  bar_t const & bar,
  std::map<std::string, std::pair<std::string, std::string>, std::less<>> const & names
) -> std::vector<row_t>
  {
  // every name seen in any of the places, with what each place says it is
  std::map<std::string, row_t, std::less<>> by_name;
  auto const take = [&](std::string const & name, std::string const & shown, std::string_view category) -> row_t *
  {
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
    auto [it, fresh]{by_name.try_emplace(name, row_t{.kind = *kind, .name = name, .shown = known_shown})};
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

  std::map<std::string, std::pair<std::string, std::string>, std::less<>> on_shelf;
  for(shelf_t const & item: bar.shelf)
    on_shelf.try_emplace(item.name, item.shown, item.category);
  std::vector<std::string> bar_names;
  for(shelf_t const & item: bar.shelf)
    bar_names.push_back(item.name);
  for(set_t const & set: bar.sets)
    bar_names.push_back(set.name);
  for(move_t const & move: bar.moves)
    bar_names.push_back(move.name);
  std::ranges::sort(bar_names);
  auto const [last, end]{std::ranges::unique(bar_names)};
  bar_names.erase(last, end);
  for(std::string const & name: bar_names)
    if(int64_t const count{on_bar(bar, name)}; count != 0)
      {
      auto const shelf_it{on_shelf.find(name)};
      std::string const shown{shelf_it != on_shelf.end() ? shelf_it->second.first : std::string{}};
      std::string const category{shelf_it != on_shelf.end() ? shelf_it->second.second : std::string{}};
      if(row_t * row{take(name, shown, category)})
        row->bar = count;
      }

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
  }  // namespace holdings
