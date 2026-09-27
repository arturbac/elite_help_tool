#pragma once

#include <elite_events.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief what sampling on foot needs to decide - where the next sample may go and what a genus is likely to be
///
/// The game says the genus from orbit and the species only after the first sample. Whether a landing is worth it
/// depends on the species, so the guess in between is built from what this commander found before: every species
/// sampled is written down beside the world it grew on, and the next world is compared against that history.
/// The more is sampled, the narrower the guess - nothing is taken from outside the journals.
namespace bio
  {
///\brief a place on a body's surface, in degrees
struct surface_point_t
  {
  double latitude{};
  double longitude{};
  };

///\brief the distance between two points on a sphere of the given radius, along its surface, in metres
[[nodiscard]]
auto surface_distance_m(surface_point_t a, surface_point_t b, double radius_m) noexcept -> double;

///\brief the course from a to b, degrees clockwise from north - the game's Heading is measured the same way
[[nodiscard]]
auto bearing_deg(surface_point_t a, surface_point_t b) noexcept -> double;

///\brief how far apart two samples of one genus have to be, in metres - the clonal colony range
///\detail a fact of the game, the same for every species of a genus; 0 for a name that is not known
[[nodiscard]]
auto colony_range_m(std::string_view genus) noexcept -> uint32_t;

///\brief the price of a species as the price list has it, with the journal's spelling of names allowed for
[[nodiscard]]
auto species_value(std::string_view species) noexcept -> std::optional<uint32_t>;

///\brief a species this commander sampled, together with the world it grew on
///\detail the field names are the columns of the query that reads them
struct species_record_t
  {
  std::string genus;
  std::string species;
  std::string planet_class;
  std::string atmosphere_type;
  std::string volcanism;
  ///\brief kelvin
  double surface_temperature;
  ///\brief m/s², as the journal writes it
  double surface_gravity;
  ///\brief pascal
  double surface_pressure;
  ///\brief the first star up the body's parents - the one whose light it gets; empty when not scanned
  std::string star_type;
  };

///\brief the same description for a world not sampled yet
struct conditions_t
  {
  std::string planet_class;
  std::string atmosphere_type;
  std::string volcanism;
  double surface_temperature{};
  double surface_gravity{};
  double surface_pressure{};
  std::string star_type;
  };

///\brief the conditions of a planet out of the scans of its system
[[nodiscard]]
auto conditions_of(star_system_t const & system, body_t const & body) -> std::optional<conditions_t>;

///\brief the record written for a sampled species on a scanned planet
[[nodiscard]]
auto record_of(star_system_t const & system, body_t const & body, std::string_view genus, std::string_view species)
  -> std::optional<species_record_t>;

///\brief how well a species found before fits the world in question
enum struct fit_e : uint8_t
  {
  ///\brief found on the same kind of atmosphere, and the temperature and the gravity lie within what was seen
  fits,
  ///\brief the atmosphere was seen, but the world is warmer, colder, heavier or lighter than anything before
  near,
  ///\brief found under this genus, never under this atmosphere
  unlike
  };

consteval auto adl_enum_bounds(fit_e)
  {
  using enum fit_e;
  return simple_enum::adl_info{fits, unlike};
  }

///\brief one guess at what a genus is on a given world
struct candidate_t
  {
  std::string species;
  uint32_t value{};
  fit_e fit{fit_e::unlike};
  ///\brief how many times this species was sampled on a world fitting as well as this one
  uint32_t seen{};
  ///\brief the share of the fitting history this species takes, 0..1 - how the finds so far split
  double share{};
  };

///\brief the species the genus is likely to be on this world, best first
///\detail only species of the genus that were sampled before appear; the ones never seen under this atmosphere
/// come last and only when nothing better is known
[[nodiscard]]
auto predict(std::string_view genus, conditions_t const & world, std::span<species_record_t const> history)
  -> std::vector<candidate_t>;
  }  // namespace bio
