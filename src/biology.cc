#include <biology.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <ranges>

using namespace std::string_view_literals;

namespace bio
  {
namespace
  {
  [[nodiscard]]
  constexpr auto radians(double degrees) noexcept -> double
    { return degrees * std::numbers::pi / 180.0; }

  struct colony_t
    {
    std::string_view genus;
    uint32_t range_m;
    };

  // the journal names some genera after their only species - "Luteolum Anemone", "Roseum Brain Tree" -
  // so a name is matched on the word the family is known by, wherever it stands
  constexpr std::array<colony_t, 21> colonies{
    {{"Aleoida"sv, 150u},  {"Bacterium"sv, 500u},   {"Cactoida"sv, 300u},   {"Clypeus"sv, 150u},
     {"Concha"sv, 150u},   {"Electricae"sv, 1000u}, {"Fonticulua"sv, 500u}, {"Frutexa"sv, 150u},
     {"Fumerola"sv, 100u}, {"Fungoida"sv, 300u},    {"Osseus"sv, 800u},     {"Recepta"sv, 150u},
     {"Stratum"sv, 500u},  {"Tubus"sv, 800u},       {"Tussock"sv, 200u},    {"Anemone"sv, 100u},
     {"Amphora"sv, 100u},  {"Bark Mound"sv, 100u},  {"Brain Tree"sv, 100u}, {"Crystalline Shard"sv, 100u},
     {"Tuber"sv, 100u}}
  };

  ///\brief a star type reduced to what its light is like - "K_OrangeGiant" and "K" shine alike for a plant
  [[nodiscard]]
  auto star_family(std::string_view star_type) noexcept -> std::string_view
    {
    if(star_type.empty())
      return {};
    if(star_type.starts_with("D"sv))
      return "D"sv;
    if(star_type == "TTS"sv or star_type == "AeBe"sv or star_type == "N"sv or star_type == "H"sv)
      return star_type;
    return star_type.substr(0, 1);
    }

  ///\brief the journal's atmosphere type as it stands - "CarbonDioxide" and "CarbonDioxideRich" grow different
  /// species, so the rich variant is not the plain gas
  [[nodiscard]]
  auto same_atmosphere(std::string_view a, std::string_view b) noexcept -> bool
    { return a == b; }
  }  // namespace

auto surface_distance_m(surface_point_t a, surface_point_t b, double radius_m) noexcept -> double
  {
  // haversine - over a few hundred metres the flat approximation would do, but not near the poles
  double const phi1{radians(a.latitude)};
  double const phi2{radians(b.latitude)};
  double const dphi{phi2 - phi1};
  double const dlambda{radians(b.longitude - a.longitude)};
  double const h{
    std::sin(dphi / 2.0) * std::sin(dphi / 2.0)
    + std::cos(phi1) * std::cos(phi2) * std::sin(dlambda / 2.0) * std::sin(dlambda / 2.0)
  };
  return 2.0 * radius_m * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0)));
  }

auto bearing_deg(surface_point_t a, surface_point_t b) noexcept -> double
  {
  double const phi1{radians(a.latitude)};
  double const phi2{radians(b.latitude)};
  double const dlambda{radians(b.longitude - a.longitude)};
  double const y{std::sin(dlambda) * std::cos(phi2)};
  double const x{std::cos(phi1) * std::sin(phi2) - std::sin(phi1) * std::cos(phi2) * std::cos(dlambda)};
  double const degrees{std::atan2(y, x) * 180.0 / std::numbers::pi};
  return std::fmod(degrees + 360.0, 360.0);
  }

auto colony_range_m(std::string_view genus) noexcept -> uint32_t
  {
  for(colony_t const & colony: colonies)
    if(genus.contains(colony.genus))
      return colony.range_m;
  return 0u;
  }

auto species_value(std::string_view species) noexcept -> std::optional<uint32_t>
  {
  if(auto it{std::ranges::find(organic_values, species, &organic_value_t::species)}; it != organic_values.end())
    return it->value;
  // the price list knows "Brain Tree" where the journal says "Roseum Brain Tree" - the kinds of those
  // families are priced alike, so the family's name at the end of the species is enough
  for(organic_value_t const & entry: organic_values)
    if(
      species.ends_with(entry.species) and species.size() > entry.species.size()
      and species[species.size() - entry.species.size() - 1u] == ' '
    )
      return entry.value;
  // and "Sinuous Tubers" against "Sinuous Tubers" but "Bark Mounds" the same - a plural slipped in
  if(species.ends_with('s'))
    return species_value(species.substr(0, species.size() - 1u));
  return {};
  }

auto conditions_of(star_system_t const & system, body_t const & body) -> std::optional<conditions_t>
  {
  auto const * const planet{std::get_if<planet_details_t>(&body.details)};
  if(planet == nullptr)
    return {};

  conditions_t result{
    .planet_class = planet->planet_class,
    .atmosphere_type = planet->atmosphere_type,
    .volcanism = planet->volcanism,
    .surface_temperature = planet->surface_temperature,
    .surface_gravity = planet->surface_gravity,
    .surface_pressure = planet->surface_pressure,
    .star_type = {}
  };
  if(planet->parent_star)
    if(auto star{system.body_by_id(*planet->parent_star)}; star != system.bodies.end())
      if(auto const * const details{std::get_if<star_details_t>(&star->details)}; details != nullptr)
        result.star_type = details->star_type;
  return result;
  }

auto record_of(star_system_t const & system, body_t const & body, std::string_view genus, std::string_view species)
  -> std::optional<species_record_t>
  {
  auto const world{conditions_of(system, body)};
  if(not world)
    return {};
  return species_record_t{
    .genus = std::string{genus},
    .species = std::string{species},
    .planet_class = world->planet_class,
    .atmosphere_type = world->atmosphere_type,
    .volcanism = world->volcanism,
    .surface_temperature = world->surface_temperature,
    .surface_gravity = world->surface_gravity,
    .surface_pressure = world->surface_pressure,
    .star_type = world->star_type
  };
  }

auto predict(std::string_view genus, conditions_t const & world, std::span<species_record_t const> history)
  -> std::vector<candidate_t>
  {
  // what the species were found under, gathered per species of the genus
  struct envelope_t
    {
    double t_min{1e9};
    double t_max{-1e9};
    double g_min{1e9};
    double g_max{-1e9};
    uint32_t same_atmosphere{};
    uint32_t fitting{};
    uint32_t same_class{};
    uint32_t same_star{};
    uint32_t total{};
    };

  std::map<std::string, envelope_t, std::less<>> species;
  for(species_record_t const & record: history)
    {
    if(record.genus != genus or record.species.empty())
      continue;
    envelope_t & e{species[record.species]};
    ++e.total;
    if(not same_atmosphere(record.atmosphere_type, world.atmosphere_type))
      continue;
    // the envelope is drawn from the finds under this atmosphere only - under another gas the same
    // species keeps to other temperatures, and mixing them would widen it into meaning nothing
    ++e.same_atmosphere;
    e.t_min = std::min(e.t_min, record.surface_temperature);
    e.t_max = std::max(e.t_max, record.surface_temperature);
    e.g_min = std::min(e.g_min, record.surface_gravity);
    e.g_max = std::max(e.g_max, record.surface_gravity);
    if(record.planet_class == world.planet_class)
      ++e.same_class;
    if(star_family(record.star_type) == star_family(world.star_type) and not world.star_type.empty())
      ++e.same_star;
    }

  std::vector<candidate_t> result;
  uint32_t fitting_total{};
  for(auto const & [name, e]: species)
    {
    candidate_t c{.species = name, .value = species_value(name).value_or(0u)};
    if(e.same_atmosphere == 0u)
      {
      c.fit = fit_e::unlike;
      c.seen = e.total;
      }
    else
      {
      // a little room around what was seen - a history of a few finds is narrower than the truth
      bool const warm_enough{world.surface_temperature >= e.t_min - 5.0 and world.surface_temperature <= e.t_max + 5.0};
      bool const light_enough{world.surface_gravity >= e.g_min * 0.9 and world.surface_gravity <= e.g_max * 1.1};
      c.fit = warm_enough and light_enough ? fit_e::fits : fit_e::near;
      c.seen = e.same_atmosphere;
      if(c.fit == fit_e::fits)
        fitting_total += c.seen;
      }
    result.push_back(std::move(c));
    }

  for(candidate_t & c: result)
    if(c.fit == fit_e::fits and fitting_total != 0u)
      c.share = double(c.seen) / double(fitting_total);

  std::ranges::sort(
    result,
    [](candidate_t const & a, candidate_t const & b)
    {
      if(a.fit != b.fit)
        return a.fit < b.fit;
      if(a.seen != b.seen)
        return a.seen > b.seen;
      return a.value > b.value;
    }
  );

  // what was never seen under this atmosphere says nothing once anything was
  if(not result.empty() and result.front().fit != fit_e::unlike)
    std::erase_if(result, [](candidate_t const & c) { return c.fit == fit_e::unlike; });
  return result;
  }
  }  // namespace bio
