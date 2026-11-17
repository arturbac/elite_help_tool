#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

///\brief the print of a planet's atmosphere as edworld_eht read it off the game's render (`atmosphere` in its shm_dir,
/// src/edworld_share.h of edworld) beside what the journal says of the planet: is it a case never seen
namespace atmosphere_print
  {
///\brief one read: the planet's radius (the journal's Radius), the hue, whether it is one of edworld's prints (its kind
/// as the journal's AtmosphereType) and its kind's second one
struct read_t
  {
  bool ok{};
  bool known{};
  bool second{};
  std::string kind;
  float radius{};
  float r_g{};
  float b_g{};
  int64_t unix_ms{};
  };

///\brief a planet of the system: its name and radius (metres) and the journal's AtmosphereType
struct planet_t
  {
  std::string_view name;
  double radius{};
  std::string_view atmosphere_type;
  };

enum struct verdict_e : uint8_t
  {
  known,
  second,   ///< a print edworld knows as its kind's second (a volcanic neon, a cold ammonia)
  unknown,  ///< a print edworld does not know: a case never seen
  differs   ///< a known print of another kind than the journal's
  };

struct result_t
  {
  std::string_view name;
  std::string_view atmosphere_type;
  verdict_e verdict{};
  };

///\brief the planet of the read (by its radius, the float edworld has of it within 2 m) and the verdict; none for a read
/// without a hue or of a planet not in the system
[[nodiscard]]
auto judge(read_t const & read, std::span<planet_t const> planets) -> std::optional<result_t>;
  }  // namespace atmosphere_print
