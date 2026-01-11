#include <elite_data.h>
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
         and influence == rh.influence and reputation == rh.reputation;
  }

auto to_native(events::faction_info_t && faction) -> faction_info_t
  {
  faction_info_t result{
    .name = std::move(faction.Name), .oid = -1, .influence = faction.Influence, .reputation = faction.MyReputation
  };
  if(auto castres{simple_enum::enum_cast<government_e>(to_lower(faction.Government))}; castres)
    result.government = *castres;

  if(auto castres{simple_enum::enum_cast<allegiance_e>(to_lower(faction.Allegiance))}; castres)
    result.allegiance = *castres;

  if(auto castres{simple_enum::enum_cast<happiness_e>(to_lower(faction.Happiness_Localised))}; castres)
    result.happiness = *castres;

  return result;
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
