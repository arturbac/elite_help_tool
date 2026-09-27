#pragma once

namespace eht_overlay
  {
///\brief true once for every press of the screenshot key made while this game's window is the active one
///\detail The first call starts a thread of its own watching the keyboard through the X server the game
/// draws on - under XWayland it hears keys only while one of its windows has the focus. Nothing here
/// ever blocks: no X server, no libxcb or no key asked for simply means no screenshots
[[nodiscard]]
auto take_screenshot_press() noexcept -> bool;
  }  // namespace eht_overlay
