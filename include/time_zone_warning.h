#pragma once

namespace eht
  {
///\brief the machine's time zone could not be read, so times are shown in UTC - said once per process,
/// the places that fall back to UTC call it every time
auto warn_no_time_zone() noexcept -> void;
  }  // namespace eht
