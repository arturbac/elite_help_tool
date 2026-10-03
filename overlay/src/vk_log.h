#pragma once

#include <format>
#include <string_view>
#include <utility>

namespace eht_overlay
  {
///\brief EHT_OVERLAY_DEBUG set to anything but 0 - the layer and the plugin each read it once
[[nodiscard]]
auto debug_enabled() noexcept -> bool;

auto log_line(std::string_view text) -> void;

///\brief what the exception in flight says - only inside a catch block
[[nodiscard]]
auto exception_text() noexcept -> std::string_view;

template<typename... args_t>
auto log(std::format_string<args_t...> fmt, args_t &&... args) -> void
  {
  if(debug_enabled()) [[unlikely]]
    log_line(std::format(fmt, std::forward<args_t>(args)...));
  }

///\brief a part of the overlay gave up - said always, not only with EHT_OVERLAY_DEBUG; called once per
/// failure, never per frame, and never throws on the game's frame path
template<typename... args_t>
auto report(std::format_string<args_t...> fmt, args_t &&... args) noexcept -> void
  {
  try
    {
    log_line(std::format(fmt, std::forward<args_t>(args)...));
    }
  catch(...)
    {
    log_line("a failure could not be described");
    }
  }
  }  // namespace eht_overlay
