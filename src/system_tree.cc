#include <system_tree.h>

#include <algorithm>
#include <ranges>

namespace system_tree
  {
namespace
  {
  ///\brief a name of letters only, two or more - `AB`, `ABCD`; never a lone star's own name
  [[nodiscard]]
  auto is_letters(std::string_view prefix) -> bool
    {
    return prefix.size() >= 2u and std::ranges::all_of(prefix, [](char c) { return c >= 'A' and c <= 'Z'; });
    }

  [[nodiscard]]
  auto contains(std::vector<std::size_t> const & outer, std::vector<std::size_t> const & inner) -> bool
    {
    return std::ranges::includes(outer, inner);
    }

  [[nodiscard]]
  auto disjoint(std::vector<std::size_t> const & a, std::vector<std::size_t> const & b) -> bool
    {
    return std::ranges::none_of(a, [&](std::size_t x) { return std::ranges::binary_search(b, x); });
    }
  }  // namespace

auto members(barycentre_t const & barycentre, std::span<star_t const> stars) -> std::vector<std::size_t>
  {
  std::vector<std::size_t> out;
  bool const lettered{is_letters(barycentre.prefix)};
  for(std::size_t ix{}; ix != stars.size(); ++ix)
    {
    star_t const & star{stars[ix]};
    bool const circles{star.barycentre and *star.barycentre == barycentre.body_id};
    bool const named{lettered and star.short_name.size() == 1u
                     and barycentre.prefix.find(star.short_name.front()) != std::string_view::npos};
    if(circles or named)
      out.push_back(ix);
    }
  return out;
  }

auto order(std::span<star_t const> stars, std::span<barycentre_t const> barycentres) -> layout_t
  {
  struct group_t
    {
    std::size_t index{};
    std::vector<std::size_t> stars;
    std::optional<std::size_t> parent;
    ///\brief its rows: its own and those of barycentres among the same stars (a pair of planets of CD, its CD 2
    /// and CD 3 under CD's own CD 1), in the order given
    std::vector<std::size_t> rows;
    };

  // the barycentres whose stars are known, the smallest first, so each finds the one round it after itself
  std::vector<group_t> groups;
  std::vector<std::size_t> unplaced;
  for(std::size_t ix{}; ix != barycentres.size(); ++ix)
    {
    auto stars_of{members(barycentres[ix], stars)};
    if(stars_of.size() < (barycentres[ix].has_row ? 1u : 2u))
      {
      if(barycentres[ix].has_row)
        unplaced.push_back(ix);
      continue;
      }
    std::vector<std::size_t> rows;
    if(barycentres[ix].has_row)
      rows.push_back(ix);
    // among the same stars as one before it: one bracket, its rows together
    if(auto const same{std::ranges::find(groups, stars_of, &group_t::stars)}; same != groups.end())
      {
      same->rows.insert(std::ranges::upper_bound(same->rows, ix), rows.begin(), rows.end());
      continue;
      }
    groups.push_back(group_t{.index = ix, .stars = std::move(stars_of), .rows = std::move(rows)});
    }
  std::ranges::stable_sort(groups, {}, [](group_t const & g) { return g.stars.size(); });

  // only a tree: a barycentre crossing one already taken (sharing some stars, not all) goes after everything
  std::vector<group_t> taken;
  for(group_t & group: groups)
    {
    bool const fits{std::ranges::all_of(
      taken, [&](group_t const & t) { return contains(group.stars, t.stars) or disjoint(group.stars, t.stars); }
    )};
    if(fits)
      taken.push_back(std::move(group));
    else
      unplaced.insert(unplaced.end(), group.rows.begin(), group.rows.end());
    }
  std::ranges::sort(unplaced);
  for(std::size_t ix{}; ix != taken.size(); ++ix)
    for(std::size_t up{ix + 1u}; up != taken.size(); ++up)
      if(contains(taken[up].stars, taken[ix].stars))
        {
        taken[ix].parent = up;
        break;
        }

  // each star in the smallest barycentre round it
  std::vector<std::optional<std::size_t>> star_group(stars.size());
  for(std::size_t ix{taken.size()}; ix-- != 0u;)
    for(std::size_t const star: taken[ix].stars)
      star_group[star] = ix;

  // a node: a star (false, its index) or a barycentre (true, its index in taken); the stars' order its key
  using node_t = std::pair<bool, std::size_t>;
  auto const key = [&](node_t const & node) -> std::size_t
  { return node.first ? taken[node.second].stars.front() : node.second; };
  auto const children = [&](std::optional<std::size_t> parent) -> std::vector<node_t>
  {
    std::vector<node_t> out;
    for(std::size_t ix{}; ix != taken.size(); ++ix)
      if(taken[ix].parent == parent)
        out.emplace_back(true, ix);
    for(std::size_t ix{}; ix != stars.size(); ++ix)
      if(star_group[ix] == parent)
        out.emplace_back(false, ix);
    std::ranges::sort(out, {}, key);
    return out;
  };

  layout_t layout;
  // emits a barycentre's subtree; gives its depth and the row a bracket round it ticks (its own, else its last)
  auto const emit = [&](this auto const & self, std::size_t group) -> std::pair<int, std::size_t>
  {
    std::size_t const first{layout.rows.size()};
    int depth{};
    std::vector<std::size_t> ticks;
    for(node_t const & node: children(group))
      if(node.first)
        {
        auto const [inner, tick]{self(node.second)};
        depth = std::max(depth, inner + 1);
        ticks.push_back(tick);
        }
      else
        {
        ticks.push_back(layout.rows.size());
        layout.rows.push_back(row_t{.star = true, .index = node.second});
        }
    barycentre_t const & own{barycentres[taken[group].index]};
    if(std::vector<std::size_t> const & rows{taken[group].rows}; not rows.empty())
      {
      ticks.push_back(layout.rows.size());
      layout.rows.push_back(
        row_t{.star = false, .index = rows.front(), .with = {rows.begin() + 1, rows.end()}, .centre = own.body_id}
      );
      }
    std::size_t const last{layout.rows.size() - 1u};
    if(last > first)
      layout.brackets.push_back(
        bracket_t{.barycentre = own.body_id, .first = first, .last = last, .depth = depth, .ticks = std::move(ticks)}
      );
    return {depth, last};
  };
  for(node_t const & node: children(std::nullopt))
    if(node.first)
      emit(node.second);
    else
      layout.rows.push_back(row_t{.star = true, .index = node.second});
  for(std::size_t const ix: unplaced)
    layout.rows.push_back(row_t{.star = false, .index = ix, .centre = barycentres[ix].body_id});
  return layout;
  }

auto in_planet_place(std::string_view short_name, std::optional<uint32_t> parent_star) -> bool
  {
  if(not parent_star)
    return false;
  std::string_view const last{short_name.substr(short_name.rfind(' ') == std::string_view::npos ? 0u : short_name.rfind(' ') + 1u)};
  return not last.empty() and std::ranges::all_of(last, [](char c) { return c >= '0' and c <= '9'; });
  }
  }  // namespace system_tree
