#pragma once

#include <cstdint>
#include <string_view>

///\brief the tint the game's own system map gives a star of the journal's StarType, as 0xRRGGBB - the system's picture
/// on the overlay and the sky album's index draw their stars in it
[[nodiscard]]
auto star_colour(std::string_view type) noexcept -> uint32_t;
