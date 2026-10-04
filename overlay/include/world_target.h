#pragma once

#include "edworld_share.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
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

///\brief the record for one destination; known is false when the tool has no record of the system
[[nodiscard]]
inline auto make_target(
  uint64_t system_address, bool known, std::string_view allegiance, std::string_view name, int64_t unix_ms
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
  std::size_t const n{std::min(name.size(), sizeof t.name - 1u)};
  std::memcpy(t.name, name.data(), n);
  t.name[n] = '\0';
  return t;
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
