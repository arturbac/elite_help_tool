#include <glare.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <format>

namespace glare
  {
auto measure(std::span<uint8_t const> grey, uint8_t burnt_out) -> metrics_t
  {
  if(grey.empty())
    return {};
  std::array<uint64_t, 256> histogram{};
  uint64_t sum{};
  for(uint8_t const value: grey)
    {
    ++histogram[value];
    sum += value;
    }
  uint64_t const count{grey.size()};
  // the p99 is the first level with fewer than one pixel in a hundred above it
  uint64_t const above_allowed{count / 100u};
  uint64_t above{};
  size_t p99{255u};
  for(size_t level{255u}; level != 0u; --level)
    {
    if(above + histogram[level] > above_allowed)
      {
      p99 = level;
      break;
      }
    above += histogram[level];
    p99 = level - 1u;
    }
  uint64_t burnt{};
  for(size_t level{burnt_out}; level != histogram.size(); ++level)
    burnt += histogram[level];
  return metrics_t{
    .luma_mean = double(sum) / double(count) / 255.0,
    .luma_p99 = double(p99) / 255.0,
    .overexposed_pct = 100.0 * double(burnt) / double(count)
  };
  }

auto is_glare(metrics_t const & metrics, double overexposed_pct) noexcept -> bool
  { return metrics.overexposed_pct >= overexposed_pct; }

auto iso_utc(std::chrono::system_clock::time_point at) -> std::string
  { return std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::milliseconds>(at)); }

auto to_json(marker_t const & marker) -> std::string
  {
  std::string json;
  if(glz::write<glz::opts{.prettify = true}>(marker, json))
    return {};
  return json;
  }

auto file_name(marker_t const & marker) -> std::string
  { return std::format("{}_{}.json", marker.ts_utc, marker.source); }
  }  // namespace glare
