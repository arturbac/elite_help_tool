#pragma once

#include <cstdint>
#include <string>

namespace eht
  {
///\brief a colour, written in the file as "#rrggbb" so that it can be read and picked by eye
///\detail how it is spelled in JSON is in json_glaze.h - without that glaze writes it as an object
struct colour_t
  {
  uint32_t rgb{};

  auto read(std::string const & text) -> void;
  [[nodiscard]]
  auto write() const -> std::string;
  };
  }  // namespace eht
