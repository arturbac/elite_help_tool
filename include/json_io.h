#pragma once

#include <string>

///\brief reading JSON with glaze out of sight
///\detail glaze is all headers, heavy to parse and heavier to instantiate. The types defined in headers
/// are read through these two templates instead: their definition is in src/json_io_impl.h, and each type
/// is instantiated once, explicitly, in a json_io_*.cc file beside the others of its kind - so neither the
/// headers of the types nor the files that read them see glaze at all. A type nobody instantiated is a
/// link error, never a second, silent copy
namespace eht::json
  {
///\brief why a read failed, in glaze's own words and with the place in the text - empty when it did not
struct error_t
  {
  std::string what;

  [[nodiscard]]
  explicit operator bool() const noexcept
    { return not what.empty(); }
  };

///\brief one JSON object into the value: keys the type lacks are skipped, and fields the text lacks are left
/// as the value had them - the journal adds keys with every update and leaves out whatever does not apply
///\param text a std::string rather than a view, since glaze reads up to the terminating zero
template<typename T>
[[nodiscard]]
auto read_lenient(T & value, std::string const & text) -> error_t;

///\brief the same, the whole file at the path being the object - the files the game keeps beside the journals
template<typename T>
[[nodiscard]]
auto read_file_lenient(T & value, std::string const & path) -> error_t;
  }  // namespace eht::json
