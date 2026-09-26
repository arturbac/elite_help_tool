#include <elite_data.h>

#include <algorithm>
#include <array>
#include <string_view>
#include <span>
#include <simple_enum/enum_cast.hpp>
#include <stralgo/stralgo.h>

namespace info
  {
using namespace std::string_view_literals;

auto distance(space_location_t const & a, space_location_t const & b) -> double
  {
  double const dx = a[0] - b[0];
  double const dy = a[1] - b[1];
  double const dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
  }

[[nodiscard]]
constexpr auto to_lower(std::string_view input) -> std::string
  {
  auto lowered = input | std::views::transform(stralgo::to_lower);

  return std::string(lowered.begin(), lowered.end());
  }

auto faction_info_t::operator==(faction_info_t const & rh) const noexcept -> bool
  {
  return government == rh.government and allegiance == rh.allegiance and happiness == rh.happiness
         and reputation == rh.reputation;
  }

auto to_native(events::faction_info_t && faction) -> faction_info_t
  {
  faction_info_t result{.name = std::move(faction.Name), .oid = -1, .reputation = faction.MyReputation};
  if(auto castres{simple_enum::enum_cast<government_e>(to_lower(faction.Government))}; castres)
    result.government = *castres;

  if(auto castres{simple_enum::enum_cast<allegiance_e>(to_lower(faction.Allegiance))}; castres)
    result.allegiance = *castres;

  if(auto castres{simple_enum::enum_cast<happiness_e>(to_lower(faction.Happiness_Localised))}; castres)
    result.happiness = *castres;

  return result;
  }

auto format_population(uint64_t value) -> std::string
  {
  struct step_t
    {
    uint64_t unit;
    char suffix;
    };

  // largest first, so that a billion does not come out as a thousand million
  for(step_t const & step: {step_t{1'000'000'000u, 'B'}, step_t{1'000'000u, 'M'}, step_t{1'000u, 'k'}})
    {
    if(value < step.unit)
      continue;

    double const scaled{double(value) / double(step.unit)};

    // a decimal place only means something for single digits - at 44M nobody cares about the 44.3
    return scaled < 10.0 ? std::format("{:.1f}{}", scaled, step.suffix)
                         : std::format("{:.0f}{}", scaled, step.suffix);
    }

  return std::format("{}", value);
  }

auto join_states(std::span<events::faction_state_entry_t const> states) -> std::string
  {
  std::string result;
  for(events::faction_state_entry_t const & state: states)
    {
    if(not result.empty())
      result.append(", ");
    result.append(state.State);
    }
  return result;
  }

auto conflict_t::operator==(conflict_t const & rh) const noexcept -> bool
  {
  return system_address == rh.system_address and war_type == rh.war_type and status == rh.status
         and faction1 == rh.faction1 and stake1 == rh.stake1 and won_days1 == rh.won_days1 and faction2 == rh.faction2
         and stake2 == rh.stake2 and won_days2 == rh.won_days2;
  }

auto to_conflict(uint64_t system_address, std::chrono::sys_seconds timestamp, events::conflict_t const & conflict)
  -> conflict_t
  {
  return conflict_t{
    .system_address = system_address,
    .timestamp = timestamp,
    .war_type = conflict.WarType,
    .status = conflict.Status,
    .faction1 = conflict.Faction1.Name,
    .stake1 = conflict.Faction1.Stake,
    .won_days1 = conflict.Faction1.WonDays,
    .faction2 = conflict.Faction2.Name,
    .stake2 = conflict.Faction2.Stake,
    .won_days2 = conflict.Faction2.WonDays
  };
  }

auto to_influence(
  int64_t faction_oid,
  uint64_t system_address,
  std::chrono::sys_seconds timestamp,
  events::faction_info_t const & faction
) -> faction_influence_t
  {
  return faction_influence_t{
    .faction_oid = faction_oid,
    .system_address = system_address,
    .timestamp = timestamp,
    .influence = faction.Influence,
    .faction_state = faction.FactionState,
    .pending_states = join_states(faction.PendingStates),
    .active_states = join_states(faction.ActiveStates),
    .recovering_states = join_states(faction.RecoveringStates)
  };
  }

namespace
  {
  ///\brief sorted, because the search is binary; it grows as Artur reports further ones
  constexpr std::array mining_only_commodities{
    "Alexandrite"sv,   "Bastnasite"sv, "Benitoite"sv, "Bromellite"sv,  "Deuterium"sv,          "Diamond"sv,
    "Grandidierite"sv, "Helium"sv,     "Helium-3"sv,  "Iridium"sv,     "Low Temp. Diamonds"sv, "Magnesite"sv,
    "Monazite"sv,      "Musgravite"sv, "Olivine"sv,   "Painite"sv,     "Periclase Dunite"sv,
  "Platinum"sv,   "Quartz Pyroxenite"sv,
    "Rhodplumsite"sv,  "Ruby"sv,       "Sapphire"sv,  "Serendibite"sv, "Thortveitite"sv,       "Void Opal"sv,
  };
  }  // namespace

auto is_mining_only(std::string_view commodity) noexcept -> bool
  { return std::ranges::binary_search(mining_only_commodities, commodity); }

auto transform_mission_name(std::string_view input) -> std::string
  {
  std::string result;

  // 1. drop "Mission" (and the underscore after it, if any)
  std::string_view working_view = input;
  if(working_view.starts_with("Mission_"))
    working_view.remove_prefix(8);
  else if(working_view.starts_with("Mission"))
    working_view.remove_prefix(7);

  // reserve memory, with safe room for the added spaces
  result.reserve(working_view.size() * 2);

  for(std::size_t i = 0; i < working_view.size(); ++i)
    {
    char const c = working_view[i];

    // 2. turn '_' into a space
    if(c == '_')
      {
      // avoid double spaces when a capital letter follows the '_'
      if(result.empty() || result.back() != ' ')
        result.push_back(' ');
      continue;
      }

    // 3. put a space before capital letters (CamelCase -> Camel Case), but only where a word really
    // begins - a capital following another capital belongs to the same run, and splitting those
    // turned the board tag of "Mission_OnFoot_Massacre_MB" into "M B"
    // logic error check: always make sure no space is added at the very beginning
    if(std::isupper(static_cast<unsigned char>(c)) && i > 0
       && !std::isupper(static_cast<unsigned char>(working_view[i - 1])))
      {
      if(!result.empty() && result.back() != ' ')
        result.push_back(' ');
      }

    result.push_back(c);
    }

  // optional: clean the leading spaces, in case dropping Mission left an awkward one
  auto trimmed = result | std::views::drop_while(isspace);
  return std::string(trimmed.begin(), trimmed.end());
  }
  }  // namespace info
