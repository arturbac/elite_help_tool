#pragma once
#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

///\brief database rows: colonisation - claimed systems and construction sites with what they need
namespace info
  {
///\brief a system claimed for colonisation by one of our commanders, and whether the claim was given up
struct colony_claim_t
  {
  uint64_t system_address;
  std::string system;
  std::string commander;
  std::chrono::sys_seconds claimed;
  bool released;
  };

///\brief a construction site's state as the game last told it
struct construction_depot_t
  {
  uint64_t market_id;
  uint64_t system_address;
  double progress;
  bool complete;
  bool failed;
  std::chrono::sys_seconds updated;
  };

///\brief names in the order a reader expects - "CMM Composite" after "Ceramic Composites", not before
[[nodiscard]]
auto names_before(std::string_view a, std::string_view b) noexcept -> bool;

///\brief the order of a site's list: by type, then by name, the way the game's own list reads
[[nodiscard]]
auto needs_before(std::string_view category_a, std::string_view name_a, std::string_view category_b, std::string_view name_b) noexcept
  -> bool;

///\brief one commodity a construction site needs - required, and provided so far
struct construction_need_t
  {
  int64_t oid{-1};
  uint64_t market_id;
  ///\brief commodity_key of the name - what the market and the hold are matched by
  std::string key;
  std::string commodity;
  uint32_t required;
  uint32_t provided;
  uint32_t payment;
  };

///\brief cargo handed in at a construction site
struct construction_delivery_t
  {
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t market_id;
  std::string key;
  uint32_t amount;
  std::string commander;
  };

///\brief a construction site the commander gave up on - the game never says a site lapsed unless it is
/// visited, so the choice is the commander's; it cannot be rebuilt from journals
struct construction_abandoned_t
  {
  uint64_t market_id;
  std::chrono::sys_seconds marked;
  };

///\brief a construction site under way in one of our systems, with what it still needs
struct construction_site_t
  {
  construction_depot_t depot;
  std::string name;
  std::string system;
  std::vector<construction_need_t> needs;
  bool abandoned{};
  };

///\brief a site's name as shown - without the "Planetary/Orbital Construction Site: " the game puts before
/// it, which tells the builder nothing; the stored name keeps it, as the game's destination does
[[nodiscard]]
inline auto shown_name(construction_site_t const & site) -> std::string
  {
  std::string_view name{site.name};
  for(std::string_view const prefix: {std::string_view{"Planetary Construction Site: "},
                                      std::string_view{"Orbital Construction Site: "}})
    if(name.starts_with(prefix))
      name.remove_prefix(prefix.size());
  return name.empty() ? std::format("site {}", site.depot.market_id) : std::string{name};
  }
  }  // namespace info
