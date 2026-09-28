#include <territory.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <map>
#include <ranges>

namespace territory
  {
namespace
  {
  ///\brief whether a list of states as the database keeps it - "CivilUnrest, Bust, Retreat" - holds this one
  [[nodiscard]]
  auto has_state(std::string_view states, std::string_view state) -> bool
    {
    for(auto const part: std::views::split(states, std::string_view{", "}))
      if(std::string_view{part.begin(), part.end()} == state)
        return true;
    return false;
    }

  [[nodiscard]]
  auto find_faction(system_t const & system, std::string_view name) -> faction_t const *
    {
    auto const it{std::ranges::find(system.factions, name, &faction_t::name)};
    return it != system.factions.end() ? &*it : nullptr;
    }

  ///\brief the last two words of a procedural name - "BD-I a64-1", "PI-T d3-51" - the letters of a boxel and
  /// the mass code with its numbers
  [[nodiscard]]
  auto is_procedural_tail(std::string_view boxel, std::string_view code) -> bool
    {
    bool const boxel_like{boxel.size() == 4u and boxel[2] == '-'};
    bool const code_like{
      code.size() >= 2u and code[0] >= 'a' and code[0] <= 'h' and std::isdigit(static_cast<unsigned char>(code[1]))
    };
    return boxel_like and code_like;
    }

  ///\brief the sector of a procedural name, empty for a proper one - "Mabozho", "Pethes"
  [[nodiscard]]
  auto sector_of(std::string_view name) -> std::string_view
    {
    auto const code_at{name.rfind(' ')};
    if(code_at == std::string_view::npos)
      return {};
    auto const boxel_at{name.rfind(' ', code_at - 1u)};
    if(boxel_at == std::string_view::npos or boxel_at == 0u)
      return {};
    if(not is_procedural_tail(name.substr(boxel_at + 1u, code_at - boxel_at - 1u), name.substr(code_at + 1u)))
      return {};
    return name.substr(0u, boxel_at);
    }
  }  // namespace

auto tick_seen(system_t const & system, std::optional<std::chrono::sys_seconds> wave) noexcept -> tick_seen_e
  {
  // with no wave known there is nothing to wait for
  if(not wave)
    return tick_seen_e::known;
  if(system.changed and *system.changed >= *wave)
    return tick_seen_e::known;
  if(system.seen and *system.seen >= *wave)
    return tick_seen_e::unchanged;
  return tick_seen_e::not_seen;
  }

auto lead(system_t const & system) -> std::optional<lead_t>
  {
  faction_t const * const controller{find_faction(system, system.controlling)};
  if(controller == nullptr)
    return std::nullopt;

  faction_t const * rival{};
  for(faction_t const & faction: system.factions)
    if(&faction != controller and (rival == nullptr or faction.influence > rival->influence))
      rival = &faction;
  if(rival == nullptr)
    return std::nullopt;

  return lead_t{.rival = rival->name, .margin = controller->influence - rival->influence};
  }

auto standings(std::span<system_t const> systems, std::span<std::string const> own) -> std::vector<standing_t>
  {
  std::vector<standing_t> result;
  result.reserve(own.size());
  for(std::string const & name: own)
    {
    standing_t standing{.faction = name};
    for(system_t const & system: systems)
      {
      faction_t const * const faction{find_faction(system, name)};
      if(faction == nullptr)
        continue;
      ++standing.present;

      if(system.controlling == name)
        {
        ++standing.controls;
        if(
          auto const held{lead(system)}; held and (not standing.thinnest_lead or held->margin < *standing.thinnest_lead)
        )
          {
          standing.thinnest_lead = held->margin;
          standing.thinnest_system = system.name;
          standing.thinnest_rival = held->rival;
          }
        continue;
        }

      // behind the faction the game names as controlling - the one a conflict for control would be fought with
      faction_t const * const controller{find_faction(system, system.controlling)};
      if(controller == nullptr)
        continue;
      double const gap{controller->influence - faction->influence};
      if(not standing.closest_gap or gap < *standing.closest_gap)
        {
        standing.closest_gap = gap;
        standing.closest_system = system.name;
        standing.closest_controller = controller->name;
        }
      }
    result.push_back(std::move(standing));
    }
  return result;
  }

auto notes(system_t const & system, std::span<std::string const> own, double retreat_below) -> std::vector<std::string>
  {
  std::vector<std::string> result;
  for(war_t const & war: system.wars)
    {
    if(not war.active)
      {
      result.push_back(std::format("{} announced: {} - {}", war.war_type, war.faction1, war.faction2));
      continue;
      }
    std::string const when{
      war.ticks_left == 0u ? std::string{"decided at the next war tick"}
                           : std::format("{} more war ticks", war.ticks_left)
    };
    result.push_back(
      std::format("{}: {} {}:{} {}, {}", war.war_type, war.faction1, war.won_days1, war.won_days2, war.faction2, when)
    );
    }

  for(faction_t const & faction: system.factions)
    {
    if(has_state(faction.active, "Retreat"))
      result.push_back(std::format("{} retreats at {:.1f}%", faction.name, faction.influence));
    else if(has_state(faction.pending, "Retreat"))
      result.push_back(std::format("{} to retreat at {:.1f}%", faction.name, faction.influence));
    // one's own faction before the game says anything - by then the days to climb back are already counting
    else if(faction.influence < retreat_below and std::ranges::contains(own, faction.name))
      result.push_back(
        std::format(
          "{} low at {:.1f}% - below {:.1f}% a retreat follows", faction.name, faction.influence, retreat_below
        )
      );
    }
  return result;
  }

auto common_sector(std::span<system_t const> systems) -> std::string
  {
  std::map<std::string_view, size_t> counts;
  for(system_t const & system: systems)
    if(auto const sector{sector_of(system.name)}; not sector.empty())
      ++counts[sector];

  auto const best{std::ranges::max_element(counts, {}, [](auto const & entry) { return entry.second; })};
  if(best == counts.end() or best->second < 2u)
    return {};
  return std::string{best->first};
  }

auto short_name(std::string_view name, std::string_view sector) -> std::string
  {
  if(
    not sector.empty() and name.size() > sector.size() + 1u and name.starts_with(sector) and name[sector.size()] == ' '
  )
    return std::string{name.substr(sector.size() + 1u)};
  return std::string{name};
  }

auto distance(
  std::optional<std::array<double, 3>> const & from, std::optional<std::array<double, 3>> const & to
) noexcept -> std::optional<double>
  {
  if(not from or not to)
    return std::nullopt;
  double const dx{(*from)[0] - (*to)[0]};
  double const dy{(*from)[1] - (*to)[1]};
  double const dz{(*from)[2] - (*to)[2]};
  return std::sqrt(dx * dx + dy * dy + dz * dz);
  }
  }  // namespace territory
