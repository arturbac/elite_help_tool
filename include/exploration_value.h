#pragma once
#include <star_system.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

// what exploration is worth - scans, mapping, organic samples

struct planet_value_info_t
  {
  std::string_view planet_class;
  double base_value;
  double terraform_bonus{0.0};
  };

///\brief the value of an organic sample after analysis, in credits
struct organic_value_t
  {
  std::string_view species;
  uint32_t value;
  };

///\brief the species price list, names as in Species_Localised of the ScanOrganic event
inline constexpr std::array<organic_value_t, 96> organic_values{
  {
   {"Aleoida Arcus", 7'252'500},
   {"Aleoida Coronamus", 6'284'600},
   {"Aleoida Gravis", 12'934'900},
   {"Aleoida Laminiae", 3'385'200},
   {"Aleoida Spica", 3'385'200},
   {"Amphora plant", 1'628'800},
   {"Anemone", 1'499'900},
   {"Bacterium Acies", 1'000'000},
   {"Bacterium Alcyoneum", 1'658'500},
   {"Bacterium Aurasus", 1'000'000},
   {"Bacterium Bullaris", 1'152'500},
   {"Bacterium Cerbrus", 1'689'800},
   {"Bacterium Informem", 8'418'000},
   {"Bacterium Nebulus", 5'289'900},
   {"Bacterium Omentum", 4'638'900},
   {"Bacterium Scopulum", 4'934'500},
   {"Bacterium Tela", 1'949'000},
   {"Bacterium Verrata", 3'897'000},
   {"Bacterium Vesicula", 1'000'000},
   {"Bacterium Volu", 7'774'700},
   {"Bark Mounds", 1'471'900},
   {"Brain Tree", 1'593'700},
   {"Cactoida Cortexum", 3'667'600},
   {"Cactoida Lapis", 2'483'600},
   {"Cactoida Peperatis", 2'483'600},
   {"Cactoida Pullulanta", 3'667'600},
   {"Cactoida Vermis", 16'202'800},
   {"Clypeus Lacrimam", 8'418'000},
   {"Clypeus Margaritus", 11'873'200},
   {"Clypeus Speculumi", 16'202'800},
   {"Concha Aureolas", 7'774'700},
   {"Concha Biconcavis", 19'010'800},
   {"Concha Labiata", 2'352'400},
   {"Concha Renibus", 4'572'400},
   {"Crystalline Shards", 1'628'800},
   {"Electricae Pluma", 6'284'600},
   {"Electricae Radialem", 6'284'600},
   {"Fonticulua Campestris", 1'000'000},
   {"Fonticulua Digitos", 1'804'100},
   {"Fonticulua Fluctus", 20'000'000},
   {"Fonticulua Lapida", 3'111'000},
   {"Fonticulua Segmentatus", 19'010'800},
   {"Fonticulua Upupam", 5'727'600},
   {"Frutexa Acus", 7'774'700},
   {"Frutexa Collum", 1'639'800},
   {"Frutexa Fera", 1'632'500},
   {"Frutexa Flabellum", 1'808'900},
   {"Frutexa Flammasis", 10'326'000},
   {"Frutexa Metallicum", 1'632'500},
   {"Frutexa Sponsae", 5'988'000},
   {"Fumerola Aquatis", 6'284'600},
   {"Fumerola Carbosis", 6'284'600},
   {"Fumerola Extremus", 16'202'800},
   {"Fumerola Nitris", 7'500'900},
   {"Fungoida Bullarum", 3'703'200},
   {"Fungoida Gelata", 3'330'300},
   {"Fungoida Setisis", 1'670'100},
   {"Fungoida Stabitis", 2'680'300},
   {"Osseus Cornibus", 1'483'000},
   {"Osseus Discus", 12'934'900},
   {"Osseus Fractus", 4'027'800},
   {"Osseus Pellebantus", 9'739'000},
   {"Osseus Pumice", 3'156'300},
   {"Osseus Spiralis", 2'404'700},
   {"Recepta Conditivus", 14'313'700},
   {"Recepta Deltahedronix", 16'202'800},
   {"Recepta Umbrux", 12'934'900},
   {"Sinuous Tubers", 1'514'500},
   {"Stratum Araneamus", 2'448'900},
   {"Stratum Cucumisis", 16'202'800},
   {"Stratum Excutitus", 2'448'900},
   {"Stratum Frigus", 2'637'500},
   {"Stratum Laminamus", 2'788'300},
   {"Stratum Limaxus", 1'362'000},
   {"Stratum Paleas", 1'362'000},
   {"Stratum Tectonicas", 19'010'800},
   {"Tubus Cavas", 11'873'200},
   {"Tubus Compagibus", 7'774'700},
   {"Tubus Conifer", 2'415'500},
   {"Tubus Rosarium", 2'637'500},
   {"Tubus Sororibus", 5'727'600},
   {"Tussock Albata", 3'252'500},
   {"Tussock Capillum", 7'025'800},
   {"Tussock Caputus", 3'472'400},
   {"Tussock Catena", 1'766'600},
   {"Tussock Cultro", 1'766'600},
   {"Tussock Divisa", 1'766'600},
   {"Tussock Ignis", 1'849'000},
   {"Tussock Pennata", 5'853'800},
   {"Tussock Pennatis", 1'000'000},
   {"Tussock Propagito", 1'000'000},
   {"Tussock Serrati", 4'447'100},
   {"Tussock Stigmasis", 19'010'800},
   {"Tussock Triticum", 7'774'700},
   {"Tussock Ventusa", 3'227'700},
   {"Tussock Virgam", 14'313'700},
  }
};

///\brief the range of values for a name out of the journal
///\detail for a species (after ScanOrganic) this is a single value, for a genus alone the span of the whole family.
/// Genus names from the journal do not always match the price list - "Brain Trees" against "Brain Tree",
/// "Luteolum Anemone" against "Anemone" - so the match falls through successive rules.
[[nodiscard]]
auto organic_value_range(std::string_view name) noexcept -> std::optional<std::pair<uint32_t, uint32_t>>;

///\brief the k of each planet class and what terraformability adds to it - the values in common use among
/// explorers (EDDiscovery, MattG's formula); checked against single system sales, see doc/exploration.md
inline constexpr std::array<planet_value_info_t, 19> exploration_values{
  {{"Metal rich body", 21'790.0},
   {"High metal content body", 9'654.0, 100'677.0},  // the bonus added when terraformable
   {"Rocky body", 300.0, 93'328.0},
   {"Icy body", 300.0},
   {"Rocky ice body", 300.0},
   {"Earthlike body", 64'831.0 + 116'295.0},  // an Earth-like is always "terraformed" by the definition of the base value
   {"Water world", 64'831.0, 116'295.0},
   {"Ammonia world", 96'932.0},
   {"Water giant", 300.0},
   {"Water giant with life", 300.0},
   {"Gas giant with water based life", 300.0},
   {"Gas giant with ammonia based life", 300.0},
   {"Sudarsky class I gas giant", 1'656.0},
   {"Sudarsky class II gas giant", 9'654.0},
   {"Sudarsky class III gas giant", 300.0},
   {"Sudarsky class IV gas giant", 300.0},
   {"Sudarsky class V gas giant", 300.0},
   {"Helium rich gas giant", 300.0},
   {"Helium gas giant", 300.0}}
};

namespace exploration
  {
[[nodiscard]]
auto is_high_value_star(std::string_view star_class) noexcept -> bool;

[[nodiscard]]
auto extract_mass_code(std::string_view name) noexcept -> char;

[[nodiscard]]
auto system_approx_value(std::string_view star_class, std::string_view system_name) noexcept -> planet_value_e;
[[nodiscard]]
///\brief before a body is actually mapped, this is a best-case estimate ("worth this much if mapped
/// efficiently") - pass the real outcome (ProbesUsed <= EfficiencyTarget) once SAAScanComplete is known
auto aprox_value(body_t const & body, bool efficiency_bonus = true) noexcept -> uint32_t;
///\brief a star's value by its class and mass; the discovery bonus when nobody had it before
///\detail any star but the one the jump arrives at pays a third more - DistanceFromArrivalLS above zero
[[nodiscard]]
auto star_value(
  std::string_view star_type, double stellar_mass, bool is_first_discoverer = false, bool is_arrival_star = true
) noexcept -> uint32_t;
///\brief a planet scanned and not mapped - what the FSS scan alone brings
[[nodiscard]]
auto scanned_value(planet_value_info_t const & info, double mass_em, bool is_terraformable, bool is_first_discoverer)
  -> uint32_t;
[[nodiscard]]
auto calculate_value(
  planet_value_info_t const & info,
  double mass_em,
  bool is_terraformable,
  bool is_first_discoverer,
  bool is_first_mapper,
  bool efficiency_bonus
) -> uint32_t;

[[nodiscard]]
constexpr auto get_star_icon(std::string_view star_type) -> std::string_view
  {
  using namespace std::literals;

  if(star_type.starts_with("D"sv))
    return "⚪"sv;  // White Dwarfs
  if(star_type == "Neutron"sv)
    return "⚡"sv;  // Neutron Stars
  if(star_type == "BlackHole"sv)
    return "🕳"sv;  // Black Holes

  if(star_type.find("Giant"sv) != std::string_view::npos)
    return "✺"sv;

  if(star_type.starts_with("L"sv) or star_type.starts_with("T"sv) or star_type.starts_with("Y"sv))
    return "🌑"sv;

  // (KGBFOAM)
  return "☀"sv;
  }

[[nodiscard]]
constexpr auto get_planet_icon(std::string_view planet_class) -> std::string_view
  {
  using namespace std::literals;

  if(planet_class == "Earthlike body"sv)
    return "🌎"sv;
  if(planet_class.contains("Water world"sv))
    return "💧"sv;
  if(planet_class == "Ammonia world"sv)
    return "☣"sv;

  if(planet_class == "Metal rich body"sv)
    return "◈"sv;
  if(planet_class == "High metal content body"sv)
    return "🔘"sv;

  if(planet_class.contains("gas giant"sv))
    return "◎"sv;

  if(planet_class.contains("Icy"sv))
    return "❄"sv;

  if(planet_class == "Rocky body"sv)
    return "●"sv;

  return "○"sv;
  }
  }  // namespace exploration
