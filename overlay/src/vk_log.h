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

template<typename... args_t>
auto log(std::format_string<args_t...> fmt, args_t &&... args) -> void
  {
  if(debug_enabled()) [[unlikely]]
    log_line(std::format(fmt, std::forward<args_t>(args)...));
  }
  }  // namespace eht_overlay
