#pragma once

#include "edworld_share.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

///\brief what EHT tells edworld (the d3d11 proxy in the game) of a jump's destination, so the proxy draws the
/// emblem from the tool's own record of the system instead of asking EDSM
namespace overlay::world
  {
///\brief the allegiance as edworld counts it: 0 unknown, 1 Federation, 2 Empire, 3 Alliance, 4 Independent, 5 other
[[nodiscard]]
constexpr auto allegiance_code(std::string_view allegiance) noexcept -> uint8_t
  {
  if(allegiance == "Federation")
    return 1u;
  if(allegiance == "Empire")
    return 2u;
  if(allegiance == "Alliance")
    return 3u;
  if(allegiance == "Independent")
    return 4u;
  return allegiance.empty() ? 0u : 5u;
  }

///\brief one faction of the destination as the tool lists it under the jump panel
struct faction_entry_t
  {
  std::string_view name;
  ///\brief the active states, else the recovering ones, with the pushes on the bars
  std::string_view states;
  double influence;
  ///\brief as edworld counts it (allegiance_code)
  uint8_t allegiance;
  ///\brief at the last tick (overlay::trend_e's order): 0 unknown, 1 up, 2 flat, 3 down
  uint8_t trend;
  bool controlling;
  };

///\brief copies text into a fixed field, cut to fit and NUL-terminated
template<std::size_t n>
inline auto copy_text(char (&field)[n], std::string_view text) noexcept -> void
  {
  std::size_t const length{std::min(text.size(), n - 1u)};
  std::memcpy(field, text.data(), length);
  field[length] = '\0';
  }

///\brief the record for one destination; known is false when the tool has no record of the system
///\param factions_known the tool's influence readings of the system - factions then is its whole list, by
/// influence (the first max_target_factions kept), and an empty one says the system is uninhabited
[[nodiscard]]
inline auto make_target(
  uint64_t system_address,
  bool known,
  std::string_view allegiance,
  std::string_view name,
  int64_t unix_ms,
  bool factions_known = false,
  std::span<faction_entry_t const> factions = {}
) noexcept -> edworld::target_t
  {
  edworld::target_t t{};
  t.magic = edworld::target_magic;
  t.version = edworld::target_version;
  t.size = sizeof(edworld::target_t);
  t.system_address = system_address;
  t.unix_ms = unix_ms;
  t.known = known ? 1u : 0u;
  t.allegiance = known ? allegiance_code(allegiance) : 0u;
  copy_text(t.name, name);
  t.factions_known = known and factions_known ? 1u : 0u;
  if(t.factions_known != 0u)
    for(faction_entry_t const & faction:
        factions.first(std::min<std::size_t>(factions.size(), edworld::max_target_factions)))
      {
      edworld::target_faction_t & entry{t.factions[t.faction_count++]};
      copy_text(entry.name, faction.name);
      copy_text(entry.states, faction.states);
      entry.influence = static_cast<float>(faction.influence);
      entry.allegiance = faction.allegiance <= 5u ? faction.allegiance : 5u;
      entry.trend = faction.trend <= 3u ? faction.trend : 0u;
      entry.controlling = faction.controlling ? 1u : 0u;
      }
  return t;
  }

///\brief whether the shared record still tells this system: another writer of the same file (a second tool,
/// another commander's) may have put its own there since
[[nodiscard]]
inline auto holds(edworld::target_t & shared, uint64_t system_address) noexcept -> bool
  {
  return std::atomic_ref<uint64_t>{shared.system_address}.load(std::memory_order_acquire) == system_address;
  }

///\brief puts the record into the shared one under its seqlock: odd while inside, even when done
inline auto write_target(edworld::target_t & shared, edworld::target_t const & value) noexcept -> void
  {
  std::atomic_ref<uint32_t> sequence{shared.sequence};
  uint32_t const before{sequence.load(std::memory_order_relaxed)};
  sequence.store(before | 1u, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  edworld::target_t copy{value};
  copy.sequence = before | 1u;
  std::memcpy(&shared, &copy, sizeof copy);
  std::atomic_thread_fence(std::memory_order_release);
  sequence.store((before | 1u) + 1u, std::memory_order_release);
  }
  }  // namespace overlay::world
