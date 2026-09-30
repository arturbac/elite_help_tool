#pragma once

///\brief the one way into glaze for a .cc file of EHT - never <glaze/glaze.hpp> on its own
///\detail glaze says nothing when a type's JSON spelling is not in sight: an enum without simple_enum's
/// adapter goes out as its number, a colour without its meta as an object, and a read of what the other
/// spelling wrote fails. Both live here, so a file that includes this one cannot get either wrong.
/// No header includes it: glaze is all headers and costs every file that sees it about as much again
/// as the rest of what it includes - the types read from the journal go through json_io.h instead
#include <simple_enum/glaze_json_enum_name.hpp>

#include <colour.h>

template<>
struct glz::meta<eht::colour_t>
  {
  static constexpr auto value{glz::custom<&eht::colour_t::read, &eht::colour_t::write>};
  };
