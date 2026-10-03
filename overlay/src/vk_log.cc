#include "vk_log.h"

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace eht_overlay
  {
auto debug_enabled() noexcept -> bool
  {
  static bool const enabled{[]
                            {
                              char const * const value{std::getenv("EHT_OVERLAY_DEBUG")};
                              return value != nullptr and *value != '\0' and *value != '0';
                            }()};
  return enabled;
  }

auto log_line(std::string_view text) -> void
  {
  std::fprintf(stderr, "[eht-overlay] %.*s\n", static_cast<int>(text.size()), text.data());
  std::fflush(stderr);
  }

auto exception_text() noexcept -> std::string_view
  {
  try
    {
    throw;
    }
  catch(std::exception const & error)
    {
    return error.what();
    }
  catch(...)
    {
    return "unknown exception";
    }
  }
  }  // namespace eht_overlay
