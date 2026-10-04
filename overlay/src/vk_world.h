#pragma once

#include <edworld_share.h>

namespace eht_overlay
  {
///\brief the latest record of edworld, the d3d11 observer that may run in the game process beside the layer
///\detail the file is $EHT_WORLD_SHARE, or /dev/shm/edworld; without it (no edworld in the game) this answers
/// false at the cost of one failed open every two seconds, and the patches stay where the tool put them
[[nodiscard]]
auto world_record(edworld::share_t & copy) noexcept -> bool;
  }  // namespace eht_overlay
