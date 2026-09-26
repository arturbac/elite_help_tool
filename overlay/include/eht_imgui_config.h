#pragma once

namespace eht_overlay
  {
///\brief a failed imgui setup belongs in the log, not in a dead game
auto imgui_assert_failed(char const * expression, char const * file, int line) -> void;
  }  // namespace eht_overlay

#define IM_ASSERT(expression) \
  ((void)((expression) ? void() : eht_overlay::imgui_assert_failed(#expression, __FILE__, __LINE__)))

#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_DISABLE_DEBUG_TOOLS
