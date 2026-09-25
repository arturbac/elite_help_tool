#include <elite_data.h>
#include <span>
#include <simple_enum/enum_cast.hpp>
#include <stralgo/stralgo.h>

namespace info
  {

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

auto transform_mission_name(std::string_view input) -> std::string
  {
  std::string result;

  // 1. Usuwanie "Mission" (oraz opcjonalnego podkreślnika po nim)
  std::string_view working_view = input;
  if(working_view.starts_with("Mission_"))
    working_view.remove_prefix(8);
  else if(working_view.starts_with("Mission"))
    working_view.remove_prefix(7);

  // Rezerwujemy pamięć (bezpieczny zapas na dodatkowe spacje)
  result.reserve(working_view.size() * 2);

  for(std::size_t i = 0; i < working_view.size(); ++i)
    {
    char const c = working_view[i];

    // 2. Zamiana '_' na spację
    if(c == '_')
      {
      // Unikamy podwójnych spacji, jeśli po '_' następuje wielka litera
      if(result.empty() || result.back() != ' ')
        result.push_back(' ');
      continue;
      }

    // 3. Dodawanie spacji przed wielkimi literami (CamelCase -> Camel Case)
    // Logic error check: Zawsze sprawdzaj, czy nie dodajesz spacji na samym początku
    if(std::isupper(static_cast<unsigned char>(c)) && i > 0)
      {
      if(!result.empty() && result.back() != ' ')
        result.push_back(' ');
      }

    result.push_back(c);
    }

  // Opcjonalne: czyszczenie spacji na początku (jeśli Mission zostało usunięte niefortunnie)
  auto trimmed = result | std::views::drop_while(isspace);
  return std::string(trimmed.begin(), trimmed.end());
  }
  }  // namespace info
