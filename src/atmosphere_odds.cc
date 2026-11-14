#include <atmosphere_odds.h>
#include <star_census.h>

#include <algorithm>
#include <ranges>
#include <set>
#include <utility>

namespace atmosphere_odds
  {
namespace
  {
  constexpr std::string_view not_scanned{"not scanned"};

  auto parent_of(body_t const & body, std::map<std::pair<uint64_t, uint32_t>, body_t const *> const & index) -> parent_e
    {
    if(body.parent_planet < 0)
      return parent_e::star;
    auto const it{index.find({body.address, static_cast<uint32_t>(body.parent_planet)})};
    if(it == index.end() or it->second->planet_class.empty())
      return parent_e::unknown;
    std::string_view const kind{it->second->planet_class};
    // Sudarsky class I gas giant, gas giant with water based life, helium rich gas giant, water giant
    if(kind.contains("giant"))
      return parent_e::gas_giant;
    if(kind == "Icy body")
      return parent_e::icy_body;
    return parent_e::rocky_body;
    }

  auto shares(prepared_t const & prepared, std::string_view atmosphere, auto const & key_of) -> std::vector<share_row_t>
    {
    std::vector<share_row_t> rows;
    std::map<std::string, uint32_t> all;
    uint32_t total{};
    uint32_t own{};
    for(world_t const & world: prepared.worlds)
      {
      std::string key{key_of(world)};
      ++all[key];
      ++total;
      if(world.atmosphere != atmosphere)
        continue;
      ++own;
      auto const it{std::ranges::find(rows, key, &share_row_t::key)};
      if(it == rows.end())
        rows.push_back(share_row_t{.key = std::move(key), .worlds = 1u});
      else
        ++it->worlds;
      }
    for(share_row_t & row: rows)
      {
      row.share = double(row.worlds) / own;
      row.background = double(all[row.key]) / total;
      }
    return rows;
    }
  }  // namespace

auto parent_name(parent_e parent) noexcept -> std::string_view
  {
  switch(parent)
    {
    case parent_e::star:       return "a star";
    case parent_e::gas_giant:  return "a gas giant, as its moon";
    case parent_e::icy_body:   return "an icy body, as its moon";
    case parent_e::rocky_body: return "a rocky or metal body, as its moon";
    case parent_e::unknown:    break;
    }
  return "not known";
  }

auto merge(census_t into, census_t const & other) -> census_t
  {
  std::map<uint64_t, size_t> systems;
  for(auto const & [at, system]: into.systems | std::views::enumerate)
    systems.emplace(system.address, size_t(at));
  for(system_t const & system: other.systems)
    {
    auto const [it, added]{systems.emplace(system.address, into.systems.size())};
    if(added)
      {
      into.systems.push_back(system);
      continue;
      }
    system_t & known{into.systems[it->second]};
    if(known.arrival_type.empty())
      known.arrival_type = system.arrival_type;
    known.body_count = std::max(known.body_count, system.body_count);
    }
  std::set<std::pair<uint64_t, uint32_t>> bodies;
  for(body_t const & body: into.bodies)
    bodies.emplace(body.address, body.body_id);
  for(body_t const & body: other.bodies)
    if(bodies.emplace(body.address, body.body_id).second)
      into.bodies.push_back(body);
  return into;
  }

auto prepare(census_t const & census) -> prepared_t
  {
  prepared_t prepared{.systems_known = uint32_t(census.systems.size())};
  std::map<std::pair<uint64_t, uint32_t>, body_t const *> index;
  std::map<uint64_t, uint32_t> known;
  std::map<uint64_t, body_t const *> nearest_star;
  for(body_t const & body: census.bodies)
    {
    index.emplace(std::pair{body.address, body.body_id}, &body);
    ++known[body.address];
    if(body.star)
      if(auto & nearest{nearest_star[body.address]}; not nearest or body.distance_ls < nearest->distance_ls)
        nearest = &body;
    }
  for(system_t const & system: census.systems)
    {
    if(system.body_count == 0u or known[system.address] < system.body_count)
      continue;
    std::string_view type{system.arrival_type};
    if(type.empty())
      if(auto const it{nearest_star.find(system.address)}; it != nearest_star.end())
        type = it->second->star_type;
    if(not type.empty())
      prepared.full.emplace(system.address, std::string{star_census::map_class(type)});
    }
  for(body_t const & body: census.bodies)
    {
    if(body.star or not body.landable or body.atmosphere_type.empty() or body.atmosphere_type == "None")
      continue;
    std::string parent_star;
    if(body.parent_star >= 0)
      if(
        auto const it{index.find({body.address, static_cast<uint32_t>(body.parent_star)})};
        it != index.end() and it->second->star
      )
        parent_star = star_census::map_class(it->second->star_type);
    prepared.worlds.push_back(
      world_t{
        .address = body.address,
        .atmosphere = body.atmosphere_type,
        .parent_star = std::move(parent_star),
        .parent = parent_of(body, index)
      }
    );
    }
  return prepared;
  }

auto atmospheres(prepared_t const & prepared) -> std::vector<atmosphere_row_t>
  {
  std::map<std::string, std::pair<uint32_t, std::set<uint64_t>>> found;
  for(world_t const & world: prepared.worlds)
    {
    auto & [worlds, systems]{found[world.atmosphere]};
    ++worlds;
    if(prepared.full.contains(world.address))
      systems.insert(world.address);
    }
  std::vector<atmosphere_row_t> rows;
  for(auto const & [atmosphere, seen]: found)
    rows.push_back(
      atmosphere_row_t{
        .atmosphere = atmosphere,
        .worlds = seen.first,
        .systems_with = uint32_t(seen.second.size()),
        .chance = prepared.full.empty() ? 0.0 : double(seen.second.size()) / prepared.full.size()
      }
    );
  std::ranges::stable_sort(rows, [](auto const & a, auto const & b) { return a.worlds < b.worlds; });
  return rows;
  }

auto by_arrival_star(prepared_t const & prepared, std::string_view atmosphere) -> std::vector<class_row_t>
  {
  std::vector<class_row_t> rows;
  auto const row_of = [&rows](std::string const & key) -> class_row_t &
  {
    auto const it{std::ranges::find(rows, key, &class_row_t::key)};
    return it != rows.end() ? *it : rows.emplace_back(class_row_t{.key = key});
  };
  for(auto const & [address, key]: prepared.full)
    ++row_of(key).systems;
  std::set<uint64_t> with;
  for(world_t const & world: prepared.worlds)
    if(world.atmosphere == atmosphere and prepared.full.contains(world.address))
      with.insert(world.address);
  for(uint64_t const address: with)
    ++row_of(prepared.full.at(address)).systems_with;
  double const average{prepared.full.empty() ? 0.0 : double(with.size()) / prepared.full.size()};
  for(class_row_t & row: rows)
    {
    row.chance = double(row.systems_with) / row.systems;
    row.lift = average > 0.0 ? row.chance / average : 0.0;
    row.weak = row.systems < weak_below;
    }
  std::ranges::sort(
    rows, [](auto const & a, auto const & b) { return star_census::rank(a.key) < star_census::rank(b.key); }
  );
  return rows;
  }

auto by_parent_star(prepared_t const & prepared, std::string_view atmosphere) -> std::vector<share_row_t>
  {
  auto rows{shares(
    prepared,
    atmosphere,
    [](world_t const & world) { return world.parent_star.empty() ? std::string{not_scanned} : world.parent_star; }
  )};
  auto const order = [](share_row_t const & row) { return row.key == not_scanned ? 1000 : star_census::rank(row.key); };
  std::ranges::sort(rows, [&order](auto const & a, auto const & b) { return order(a) < order(b); });
  return rows;
  }

auto by_parent_body(prepared_t const & prepared, std::string_view atmosphere) -> std::vector<share_row_t>
  {
  auto rows{shares(prepared, atmosphere, [](world_t const & world) { return std::string{parent_name(world.parent)}; })};
  auto const order = [](share_row_t const & row)
  {
    for(auto const parent: {parent_e::star, parent_e::gas_giant, parent_e::icy_body, parent_e::rocky_body})
      if(row.key == parent_name(parent))
        return int(parent);
    return int(parent_e::unknown);
  };
  std::ranges::sort(rows, [&order](auto const & a, auto const & b) { return order(a) < order(b); });
  return rows;
  }
  }  // namespace atmosphere_odds
