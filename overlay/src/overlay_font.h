#pragma once

#include <cstddef>

namespace eht_overlay
  {
///\brief the overlay's face - Noto Sans Mono Regular, built into the layer from fonts/NotoSansMono-Regular.ttf
///\detail the layer runs inside the game's container, where the system's fonts are out of reach
extern unsigned char const overlay_font_data[];
extern std::size_t const overlay_font_size;
  }  // namespace eht_overlay
