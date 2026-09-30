#pragma once

///\brief the definitions behind json_io.h - included only by the json_io_*.cc files that instantiate them
#include <json_io.h>
#include <json_glaze.h>

namespace eht::json
  {
// error_on_missing_keys is off by default - spelled out, since the journal depends on it
inline constexpr glz::opts lenient_opts{.error_on_unknown_keys = false, .error_on_missing_keys = false};

template<typename T>
auto read_lenient(T & value, std::string const & text) -> error_t
  {
  if(auto const err{glz::read<lenient_opts>(value, text)}; err) [[unlikely]]
    return error_t{glz::format_error(err, text)};
  return {};
  }

template<typename T>
auto read_file_lenient(T & value, std::string const & path) -> error_t
  {
  std::string buffer;
  if(auto const err{glz::read_file_json<lenient_opts>(value, path, buffer)}; err) [[unlikely]]
    return error_t{glz::format_error(err, buffer)};
  return {};
  }
  }  // namespace eht::json
