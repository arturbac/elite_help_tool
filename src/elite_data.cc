#include <elite_data.h>
#include <simple_enum/enum_cast.hpp>
#include <stralgo/stralgo.h>

namespace info
  {
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
  }  // namespace info
