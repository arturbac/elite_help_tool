#pragma once

#include <spdlog/spdlog.h>

#include <exception>
#include <string_view>
#include <utility>

namespace eht
  {
///\brief the last net under a thread's body, a timer or a slot that runs the tool's logic
///
/// An exception that leaves a thread's body ends the whole process, one that leaves a slot runs through
/// Qt's event loop, which is undefined. This catches it, says where it happened and lets the caller go
/// on (spdlog catches its own failures). It is not a way of handling errors - those end as `expected_ec`
/// where there is enough context.
///\returns false when the function threw
template<typename function_t>
auto event_guard(std::string_view where, function_t && function) noexcept -> bool
  {
  try
    {
    std::forward<function_t>(function)();
    return true;
    }
  catch(std::exception const & error)
    {
    spdlog::error("{}: stopped by an exception: {}", where, error.what());
    }
  catch(...)
    {
    spdlog::error("{}: stopped by an unknown exception", where);
    }
  return false;
  }
  }  // namespace eht
