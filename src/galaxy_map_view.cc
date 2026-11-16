#include <galaxy_map_view.h>
#include <star_census.h>

#include <algorithm>
#include <cctype>
#include <format>
#include <ranges>
#include <regex>

namespace galaxy_map_view
  {
namespace
  {
  [[nodiscard]]
  auto lower(std::string_view text) -> std::string
    {
    std::string out{text};
    for(char & c: out)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
    }

  [[nodiscard]]
  auto rare_share(code_odds_t const & o) -> double
    { return o.full == 0u ? 0.0 : double(o.with_rare) / o.full; }

  [[nodiscard]]
  auto atmosphere_share(code_odds_t const & o) -> double
    { return o.full == 0u ? 0.0 : double(o.with_atmosphere) / o.full; }
  }  // namespace

auto parse(std::string_view name) -> std::optional<procedural_t>
  {
  static std::regex const procedural{R"(^(.+) ([A-Za-z]{2}-[A-Za-z]) ([A-Ha-h])(\d+(?:-\d+)?)$)"};
  std::match_results<std::string_view::const_iterator> m;
  if(not std::regex_match(name.begin(), name.end(), m, procedural))
    return std::nullopt;
  return procedural_t{
    .sector = m[1].str(),
    .block = m[2].str(),
    .mass_code = static_cast<char>(std::tolower(static_cast<unsigned char>(m[3].str().front()))),
    .number = m[4].str()
  };
  }

auto mass_code_odds(atmosphere_odds::census_t const & census, atmosphere_odds::prepared_t const & prepared, uint32_t rare_worlds)
  -> odds_t
  {
  std::map<std::string, uint32_t> worlds;
  std::map<uint64_t, std::set<std::string>> atmospheres;
  for(atmosphere_odds::world_t const & world: prepared.worlds)
    {
    ++worlds[world.atmosphere];
    atmospheres[world.address].insert(world.atmosphere);
    }
  std::map<char, std::map<std::string, uint32_t>> stars;
  odds_t odds;
  for(atmosphere_odds::system_t const & system: census.systems)
    {
    auto const name{parse(system.name)};
    if(not name)
      continue;
    code_odds_t & o{odds[name->mass_code]};
    o.code = name->mass_code;
    ++o.systems;
    auto const full{prepared.full.find(system.address)};
    std::string const star{
      full != prepared.full.end() ? full->second
      : system.arrival_type.empty() ? std::string{}
                                    : std::string{star_census::map_class(system.arrival_type)}
    };
    if(not star.empty())
      ++stars[o.code][star];
    if(full == prepared.full.end())
      continue;
    ++o.full;
    if(auto const it{atmospheres.find(system.address)}; it != atmospheres.end())
      {
      ++o.with_atmosphere;
      if(std::ranges::any_of(it->second, [&](std::string const & a) { return worlds[a] <= rare_worlds; }))
        ++o.with_rare;
      }
    }
  for(auto & [code, o]: odds)
    {
    o.stars.assign(stars[code].begin(), stars[code].end());
    std::ranges::stable_sort(o.stars, std::ranges::greater{}, &std::pair<std::string, uint32_t>::second);
    }
  return odds;
  }

auto known(atmosphere_odds::census_t const & census) -> known_t
  {
  known_t out;
  for(atmosphere_odds::system_t const & system: census.systems)
    {
    if(system.name.empty())
      continue;
    out.names.emplace(lower(system.name), system.name);
    if(auto const name{parse(system.name)}; name)
      out.sectors.emplace(lower(name->sector), name->sector);
    }
  return out;
  }

auto full_name(std::string_view label, known_t const & known) -> std::string
  {
  std::string const key{lower(label)};
  if(auto const it{known.names.find(key)}; it != known.names.end())
    return it->second;
  auto const name{parse(label)};
  if(not name)
    return std::string{label};
  auto const sector{known.sectors.find(lower(name->sector))};
  std::string block{name->block};
  for(char & c: block)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return std::format(
    "{} {} {}{}", sector != known.sectors.end() ? sector->second : name->sector, block, name->mass_code, name->number
  );
  }

auto view(std::span<std::string const> labels, odds_t const & odds, known_t const & known) -> view_t
  {
  view_t out;
  std::set<std::string> seen;
  std::map<std::string, uint32_t> sectors;
  std::map<char, group_t> groups;
  for(std::string const & label: labels)
    {
    if(label.find('?') != std::string::npos or label.find(':') != std::string::npos or not seen.insert(label).second)
      continue;
    auto const name{parse(label)};
    if(not name)
      continue;
    ++out.names;
    ++sectors[name->sector];
    group_t & g{groups[name->mass_code]};
    g.code = name->mass_code;
    ++g.in_view;
    if(not known.names.contains(lower(label)))
      g.unvisited.push_back(label);
    else
      g.visited.push_back(label);
    }
  if(not sectors.empty())
    out.sector = std::ranges::max_element(sectors, {}, &std::pair<std::string const, uint32_t>::second)->first;
  for(auto & [code, g]: groups)
    {
    if(auto const it{odds.find(code)}; it != odds.end())
      g.odds = it->second;
    g.odds.code = code;
    std::ranges::sort(g.unvisited);
    std::ranges::sort(g.visited);
    out.groups.push_back(std::move(g));
    }
  std::ranges::stable_sort(
    out.groups, std::ranges::greater{}, [](group_t const & g) { return std::pair{rare_share(g.odds), atmosphere_share(g.odds)}; }
  );
  return out;
  }
  }  // namespace galaxy_map_view
