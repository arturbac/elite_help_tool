#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

///\brief how much of a list stands in the room a stack has left
namespace overlay
  {
struct fit_line_t
  {
  float height{};
  ///\brief what a "... and N more" in place of this line would count; 0 is a line never given up
  uint32_t more{};
  ///\brief a heading - not worth showing without the line under it, which may in turn hold on to the next
  bool with_next{};
  };

///\brief how many of the lines to draw: all when they fit, otherwise as many as leave room for the line that
/// says how many more there are
[[nodiscard]]
inline auto lines_that_fit(std::span<fit_line_t const> lines, float room, float more_height) -> size_t
  {
  float total{};
  for(fit_line_t const & line: lines)
    total += line.height;
  if(total <= room)
    return lines.size();

  float used{};
  for(size_t ix{}; ix != lines.size(); ++ix)
    {
    // the line and all it holds on to, one with_next after another, go together or not at all
    float needed{more_height};
    for(size_t k{ix}; k != lines.size(); ++k)
      {
      needed += lines[k].height;
      if(not lines[k].with_next)
        break;
      }
    if(lines[ix].more != 0u and used + needed > room)
      return ix;
    used += lines[ix].height;
    }
  return lines.size();
  }
  }  // namespace overlay
