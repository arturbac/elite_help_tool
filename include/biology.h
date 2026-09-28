#pragma once

#include <elite_events.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
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

///\brief a sample analysed and not sold yet - it goes down with the commander if they die
struct unsold_t
  {
  std::string species;
  uint32_t value{};
  ///\brief the first-logged bonus - four times the value on top of it, when nobody had logged the species
  /// before (SellOrganicData shows it as Bonus)
  uint64_t bonus{};
  };

///\brief what goes down with the commander at death: samples analysed and not sold, bounties not handed in.
/// Combat bonds survive a death, so they are not here
struct at_risk_t
  {
  std::vector<unsold_t> samples;
  uint64_t bounties{};
  ///\brief the bodies scanned and not sold, priced as the exploration values have it - an estimate
  uint64_t cartography{};
  };

///\brief read back from the journals, the newest first, until a death - so it holds across restarts of the
/// tool without the database having to know
///\detail A sale at Vista Genomics ends the samples, as the game's one button sells all. A bounty counts
/// until its faction's vouchers are handed in; an empty faction in the hand-in is taken as all of them.
/// A scanned body counts until its system's cartographic data is sold, with the mapping value when the
/// body was mapped.
/// The journals may hold another account's sessions - only those of commander_fid count, all when empty
[[nodiscard]]
auto at_risk(std::filesystem::path const & journal_dir, std::string_view commander_fid) -> at_risk_t;

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
  ///\brief where it was - what tells the same find apart when two accounts' histories are joined
  uint64_t system_address{};
  uint32_t body_id{};
  };

///\brief adds another account's history to one's own, each find once
///\detail two accounts playing from one game profile read many of the same journals, so their
/// histories overlap - a find counted twice would weigh twice in every guess
auto merge_history(std::vector<species_record_t> & into, std::vector<species_record_t> && from) -> void;

///\brief one find of a species and the place it was made - a row of the codex
///\detail the field names are the columns of the query that reads them
struct find_t
  {
  std::string genus;
  std::string species;
  std::string system_name;
  std::string body_name;
  uint64_t system_address;
  uint32_t body_id;
  std::string planet_class;
  std::string atmosphere_type;
  std::string volcanism;
  double surface_temperature;
  double surface_gravity;
  double surface_pressure;
  std::string star_type;
  double distance_from_arrival_ls;
  double loc_x;
  double loc_y;
  double loc_z;
  ///\brief this commander finished the sample - not merely saw the species someone else took
  bool sampled;
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

///\brief what one scan of a genus on a world would add to what the finds so far say of where it grows
///\detail the first scan names the species and puts the world into the history, so a cheap species is still
/// worth it once where the history knows little - and not at all where it has seen the same many times
enum struct novelty_e : uint8_t
  {
  ///\brief the genus was never sampled anywhere
  never,
  ///\brief never under this atmosphere
  atmosphere,
  ///\brief under this atmosphere, but never on a world as warm, as cold, as heavy or as light
  warmer,
  colder,
  heavier,
  lighter,
  ///\brief within what was seen, but never under the light of this kind of star - the variant may be new
  star,
  ///\brief within what was seen, but seldom on a world like this
  few,
  ///\brief seen often enough on worlds like this that one scan more tells nothing
  known
  };

consteval auto adl_enum_bounds(novelty_e)
  {
  using enum novelty_e;
  return simple_enum::adl_info{never, known};
  }

struct knowledge_t
  {
  novelty_e novelty{novelty_e::known};
  ///\brief finds of the genus under this atmosphere within 5 K and a tenth of the gravity of this world
  uint32_t alike{};
  ///\brief how far past the finds - kelvin when warmer or colder, a share of the gravity when heavier or lighter
  double beyond{};
  };

///\brief how much the history knows of the genus on a world like this
///\param few below this many alike finds, a world counts as seldom seen
[[nodiscard]]
auto knowledge(std::string_view genus, conditions_t const & world, std::span<species_record_t const> history, uint32_t few)
  -> knowledge_t;
  }  // namespace bio
