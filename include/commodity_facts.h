#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

///\brief facts of the game about the commodities a colony is built from - who produces each, and its type
///
/// The markets we saw cannot tell it: a port has several economies and we keep only the first. So this is
/// kept in the code, as the game states it. Filled from the colonisation lists so far; a commodity not here
/// simply has no economy shown.
namespace commodity_facts
  {
enum struct economy_e : uint8_t
  {
  agriculture = 1u << 0u,
  high_tech = 1u << 1u,
  industrial = 1u << 2u,
  military = 1u << 3u,
  refinery = 1u << 4u,
  extraction = 1u << 5u
  };

[[nodiscard]]
constexpr auto operator|(economy_e a, economy_e b) noexcept -> economy_e
  { return static_cast<economy_e>(uint8_t(a) | uint8_t(b)); }

struct fact_t
  {
  std::string_view key;
  std::string_view category;
  economy_e produced_by;
  ///\brief produced only at ports on the ground
  bool surface{};
  };

inline constexpr fact_t facts[]{
  {"fruitandvegetables", "Foods", economy_e::agriculture},
  {"combatstabilisers", "Medicines", economy_e::high_tech},
  {"evacuationshelter", "Consumer Items", economy_e::high_tech},
  {"microcontrollers", "Technology", economy_e::high_tech},
  {"structuralregulators", "Technology", economy_e::high_tech},
  {"basicmedicines", "Medicines", economy_e::high_tech | economy_e::industrial},
  {"battleweapons", "Weapons", economy_e::high_tech | economy_e::industrial | economy_e::military},
  {"reactivearmour", "Weapons", economy_e::high_tech | economy_e::military},
  {"buildingfabricators", "Machinery", economy_e::industrial},
  {"computercomponents", "Technology", economy_e::industrial},
  {"foodcartridges", "Foods", economy_e::industrial},
  {"survivalequipment", "Consumer Items", economy_e::industrial},
  {"emergencypowercells", "Machinery", economy_e::industrial, true},
  {"aluminium", "Metals", economy_e::refinery},
  {"copper", "Metals", economy_e::refinery},
  {"liquidoxygen", "Chemicals", economy_e::refinery},
  {"militarygradefabrics", "Textiles", economy_e::refinery},
  {"polymers", "Industrial Materials", economy_e::refinery},
  {"steel", "Metals", economy_e::refinery},
  {"surfacestabilisers", "Chemicals", economy_e::refinery},
  {"ceramiccomposites", "Industrial Materials", economy_e::refinery, true},
};

///\brief types of commodities the market dictionary has not met yet - by the internal name, which is not
/// always the one shown: Land Enrichment Systems is "terrainenrichmentsystems"
inline constexpr std::pair<std::string_view, std::string_view> categories_only[]{
  {"terrainenrichmentsystems", "Technology"},
};

[[nodiscard]]
constexpr auto find(std::string_view key) noexcept -> std::optional<fact_t>
  {
  for(fact_t const & f: facts)
    if(f.key == key)
      return f;
  return std::nullopt;
  }

[[nodiscard]]
constexpr auto category_of(std::string_view key) noexcept -> std::optional<std::string_view>
  {
  if(auto const f{find(key)}; f)
    return f->category;
  for(auto const & [k, category]: categories_only)
    if(k == key)
      return category;
  return std::nullopt;
  }
  }  // namespace commodity_facts
