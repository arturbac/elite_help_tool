#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

///\brief facts of the game about the commodities a colony is built from - who produces each, and its type
///
/// The markets we saw cannot tell it: a port has several economies and we keep only the first. So this is
/// kept in the code, as the game states it. The producers follow RavenColonial's table (RavenColonialWeb,
/// src/types.ts, mapSourceEconomy); a commodity not here simply has no economy shown.
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
  ///\brief produced only at ports in orbit
  bool orbital{};
  };

namespace detail
  {
inline constexpr economy_e agri{economy_e::agriculture};
inline constexpr economy_e ht{economy_e::high_tech};
inline constexpr economy_e ind{economy_e::industrial};
inline constexpr economy_e mil{economy_e::military};
inline constexpr economy_e ref{economy_e::refinery};
inline constexpr economy_e ext{economy_e::extraction};
  }  // namespace detail

inline constexpr fact_t facts[]{
  // agriculture
  {"animalmeat", "Foods", detail::agri},
  {"beer", "Legal Drugs", detail::agri},
  {"coffee", "Foods", detail::agri},
  {"fish", "Foods", detail::agri},
  {"fruitandvegetables", "Foods", detail::agri},
  {"grain", "Foods", detail::agri},
  {"tea", "Foods", detail::agri},
  {"water", "Chemicals", detail::agri},
  {"wine", "Legal Drugs", detail::agri},
  {"liquor", "Legal Drugs", detail::agri | detail::ind},
  // high tech
  {"advancedcatalysers", "Technology", detail::ht},
  {"agriculturalmedicines", "Medicines", detail::ht},
  {"autofabricators", "Technology", detail::ht},
  {"bioreducinglichen", "Technology", detail::ht},
  {"combatstabilisers", "Medicines", detail::ht},
  {"evacuationshelter", "Consumer Items", detail::ht},
  {"hazardousenvironmentsuits", "Technology", detail::ht},
  {"heliostaticfurnaces", "Machinery", detail::ht},
  {"medicaldiagnosticequipment", "Technology", detail::ht},
  {"microcontrollers", "Technology", detail::ht},
  {"pesticides", "Chemicals", detail::ht},
  {"resonatingseparators", "Technology", detail::ht},
  {"robotics", "Technology", detail::ht},
  {"structuralregulators", "Technology", detail::ht},
  // Land Enrichment Systems, in the game's internal name
  {"terrainenrichmentsystems", "Technology", detail::ht},
  {"basicmedicines", "Medicines", detail::ht | detail::ind},
  {"battleweapons", "Weapons", detail::ht | detail::ind | detail::mil},
  {"nonlethalweapons", "Weapons", detail::ht | detail::mil},
  {"reactivearmour", "Weapons", detail::ht | detail::mil},
  // Muon Imager, in the game's internal name
  {"mutomimager", "Technology", detail::ht | detail::ind, true},
  // industrial
  {"buildingfabricators", "Machinery", detail::ind},
  {"computercomponents", "Technology", detail::ind},
  {"cropharvesters", "Machinery", detail::ind},
  {"foodcartridges", "Foods", detail::ind},
  {"geologicalequipment", "Machinery", detail::ind},
  {"mineralextractors", "Machinery", detail::ind},
  {"powergenerators", "Machinery", detail::ind},
  {"survivalequipment", "Consumer Items", detail::ind},
  {"thermalcoolingunits", "Machinery", detail::ind},
  {"waterpurifiers", "Machinery", detail::ind},
  {"emergencypowercells", "Machinery", detail::ind, true},
  // refinery
  {"aluminium", "Metals", detail::ref},
  {"copper", "Metals", detail::ref},
  {"liquidoxygen", "Chemicals", detail::ref},
  {"militarygradefabrics", "Textiles", detail::ref},
  {"polymers", "Industrial Materials", detail::ref},
  {"semiconductors", "Industrial Materials", detail::ref},
  {"steel", "Metals", detail::ref},
  {"superconductors", "Industrial Materials", detail::ref},
  {"surfacestabilisers", "Chemicals", detail::ref},
  {"titanium", "Metals", detail::ref},
  {"tritium", "Chemicals", detail::ref},
  {"ceramiccomposites", "Industrial Materials", detail::ref, true},
  {"cmmcomposite", "Industrial Materials", detail::ref, true},
  {"insulatingmembrane", "Industrial Materials", detail::ref, false, true},
  // every economy but agriculture
  {"biowaste", "Waste", detail::ht | detail::ind | detail::mil | detail::ref | detail::ext},
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
  return std::nullopt;
  }
  }  // namespace commodity_facts
