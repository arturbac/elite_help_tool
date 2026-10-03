#include <time_zone_warning.h>

#include <spdlog/spdlog.h>

#include <atomic>

namespace eht
  {
auto warn_no_time_zone() noexcept -> void
  {
  static std::atomic_flag said;
  if(not said.test_and_set())
    spdlog::warn("the time zone could not be read, local times are shown in UTC");
  }
  }  // namespace eht
