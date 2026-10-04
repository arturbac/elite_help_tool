#pragma once

#include "edworld_share.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>

///\brief following a cockpit panel on screen, from what edworld publishes
///\detail the cockpit camera does not sit fixed to the ship: it lags and swings when the ship turns, and with
/// it every panel of the cockpit moves on the screen. A patch placed at a fixed spot of the screen then slides
/// off the panel it covers. edworld reads, from the game's own draw of each panel, where the panel's origin
/// lands on the screen; the patch moves by as much as that point moved from where it stands at rest
namespace overlay::world
  {
///\brief which panel to follow and where its origin stands when the patch is at its own place
struct follow_t
  {
  ///\brief the size of the interface surface the panel draws, which names the panel; 0 = follow nothing
  uint32_t width{};
  uint32_t height{};
  ///\brief the origin's place at rest, in normalised device coordinates of the whole surface (x right, y up)
  float rest_x{};
  float rest_y{};
  };

///\brief how far the panel moved, in normalised device coordinates
struct shift_t
  {
  float x{};
  float y{};
  };

///\brief a record older than this is a game that stopped drawing the panel - the patch goes back to its place
inline constexpr int64_t fresh_ms{250};

///\brief a consistent copy of the published record: the writer may be inside it, so the copy is retried
/// until the sequence is even and unchanged across it
///\returns false when no consistent copy came in a few tries, or the record is not edworld's
[[nodiscard]]
inline auto read_record(edworld::share_t const & shared, edworld::share_t & copy) noexcept -> bool
  {
  constexpr int attempts{8};
  for(int attempt{}; attempt != attempts; ++attempt)
    {
    // the mapping is read-only: the builtin loads through a const pointer, which atomic_ref cannot
    uint32_t const before{__atomic_load_n(&shared.sequence, __ATOMIC_ACQUIRE)};
    if(before % 2u != 0u)
      continue;
    std::memcpy(&copy, &shared, sizeof copy);
    std::atomic_thread_fence(std::memory_order_acquire);
    if(__atomic_load_n(&shared.sequence, __ATOMIC_RELAXED) == before)
      return copy.magic == edworld::share_magic and copy.version == edworld::share_version
             and copy.panel_count <= edworld::max_panels;
    }
  return false;
  }

///\brief the panel named by its surface's size, the one nearest its rest place when several share the size
[[nodiscard]]
inline auto panel_shift(edworld::share_t const & record, follow_t const & follow, int64_t now_ms) noexcept
  -> std::optional<shift_t>
  {
  if(follow.width == 0u or follow.height == 0u)
    return std::nullopt;
  if(now_ms - record.unix_ms > fresh_ms or record.unix_ms - now_ms > fresh_ms)
    return std::nullopt;
  std::optional<shift_t> best;
  float best_distance{};
  for(uint32_t i{}; i != record.panel_count; ++i)
    {
    edworld::panel_t const & panel{record.panels[i]};
    if(panel.surface_width != follow.width or panel.surface_height != follow.height)
      continue;
    float const w{panel.anchor_clip[3]};
    if(not(w > 1e-4f))
      continue;
    shift_t const shift{panel.anchor_clip[0] / w - follow.rest_x, panel.anchor_clip[1] / w - follow.rest_y};
    if(not std::isfinite(shift.x) or not std::isfinite(shift.y))
      continue;
    float const distance{shift.x * shift.x + shift.y * shift.y};
    if(not best or distance < best_distance)
      {
      best = shift;
      best_distance = distance;
      }
    }
  return best;
  }
  }  // namespace overlay::world
