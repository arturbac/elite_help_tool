#pragma once
#include <cstdint>
#include <string>
#include <vector>

///\brief journal events: colonisation - claims and construction sites
namespace events
  {

///\brief one commodity a construction site needs, and how much of it has come
struct construction_resource_t
  {
  std::string Name;
  std::string Name_Localised;
  uint32_t RequiredAmount{};
  uint32_t ProvidedAmount{};
  uint32_t Payment{};
  };

///\brief the whole state of a construction site - written on docking at it, and again every little while
struct colonisation_construction_depot_t
  {
  uint64_t MarketID{};
  double ConstructionProgress{};
  bool ConstructionComplete{};
  bool ConstructionFailed{};
  std::vector<construction_resource_t> ResourcesRequired;
  };

struct construction_contribution_t
  {
  std::string Name;
  std::string Name_Localised;
  uint32_t Amount{};
  };

///\brief cargo handed in at a construction site
struct colonisation_contribution_t
  {
  uint64_t MarketID{};
  std::vector<construction_contribution_t> Contributions;
  };

///\brief a system claimed for colonisation - by the commander of the session
struct colonisation_system_claim_t
  {
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

///\brief a claim given up
struct colonisation_system_claim_release_t
  {
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

  }  // namespace events
