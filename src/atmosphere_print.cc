#include <atmosphere_print.h>

#include <cmath>

namespace atmosphere_print
  {
auto judge(read_t const & read, std::span<planet_t const> planets) -> std::optional<result_t>
  {
  if(not read.ok)
    return std::nullopt;
  for(planet_t const & planet: planets)
    {
    if(std::fabs(planet.radius - static_cast<double>(read.radius)) > 2.0)
      continue;
    result_t out{.name = planet.name, .atmosphere_type = planet.atmosphere_type};
    if(not read.known)
      out.verdict = verdict_e::unknown;
    else if(not planet.atmosphere_type.empty() and read.kind != planet.atmosphere_type)
      out.verdict = verdict_e::differs;
    else
      out.verdict = read.second ? verdict_e::second : verdict_e::known;
    return out;
    }
  return std::nullopt;
  }
  }  // namespace atmosphere_print
