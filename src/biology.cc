#include <biology.h>
#include <exploration_value.h>

#include <algorithm>
#include <fstream>
#include <json_glaze.h>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <ranges>
#include <set>
#include <sstream>
#include <tuple>

using namespace std::string_view_literals;

namespace bio
  {
namespace detail
  {
  ///\brief the two fields of a ScanOrganic line the count of unsold samples needs - glaze wants a type with
  /// linkage, so it stays out of the anonymous namespace
  struct analysed_t
    {
    std::string Species_Localised;
    std::string ScanType;
    bool WasLogged{true};
    };

  struct commander_line_t
    {
    std::string FID;
    };

  struct faction_reward_t
    {
    std::string Faction;
    uint64_t Reward{};
    };

  struct bounty_line_t
    {
    std::vector<faction_reward_t> Rewards;
    };

  struct faction_amount_t
    {
    std::string Faction;
    uint64_t Amount{};
    };

  struct redeem_line_t
    {
    std::string Type;
    std::vector<faction_amount_t> Factions;
    };

  struct scan_line_t
    {
    std::string StarSystem;
    std::string BodyName;
    std::string StarType;
    double StellarMass{};
    std::string PlanetClass;
    double MassEM{};
    std::string TerraformState;
    bool WasDiscovered{true};
    bool WasMapped{true};
    };

  struct body_line_t
    {
    std::string BodyName;
    uint16_t ProbesUsed{};
    uint16_t EfficiencyTarget{};
    };

  struct sold_system_t
    {
    std::string SystemName;
    };

  ///\brief a single sale names systems plainly, a multiple one as objects - both are read, whichever it is
  struct sale_line_t
    {
    std::vector<std::string> Systems;
    std::vector<glz::generic> Discovered;
    };

  ///\brief a sale with its sums - Discovered is objects in the multiple sale, plain names in the single one
  struct priced_sale_line_t
    {
    std::string timestamp;
    std::vector<std::string> Systems;
    std::vector<glz::generic> Discovered;
    uint64_t BaseValue{};
    uint64_t Bonus{};
    uint64_t TotalEarnings{};
    };
  }  // namespace detail

namespace
  {
  ///\brief what a scanned body brings at the cartographer's - the discovery bonus when nobody had it,
  /// the mapping value only when it was mapped, and only the full +25% efficiency bonus when the DSS
  /// probes used did not exceed the efficiency target it was actually mapped with
  [[nodiscard]]
  auto body_price(detail::scan_line_t const & scan, std::optional<bool> mapped_efficiently) -> uint64_t
    {
    if(not scan.StarType.empty())
      return exploration::star_value(scan.StarType, scan.StellarMass, not scan.WasDiscovered);
    auto const it{std::ranges::find(exploration_values, scan.PlanetClass, &planet_value_info_t::planet_class)};
    if(it == exploration_values.end())
      return 0u;
    bool const terraformable{not scan.TerraformState.empty()};
    if(mapped_efficiently)
      return exploration::calculate_value(
        *it, scan.MassEM, terraformable, not scan.WasDiscovered, not scan.WasMapped, *mapped_efficiently
      );
    return exploration::scanned_value(*it, scan.MassEM, terraformable, not scan.WasDiscovered);
    }

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

auto knowledge(std::string_view genus, conditions_t const & world, std::span<species_record_t const> history, uint32_t few)
  -> knowledge_t
  {
  bool any{};
  uint32_t under_atmosphere{};
  uint32_t same_star{};
  uint32_t alike{};
  double t_min{1e9};
  double t_max{-1e9};
  double g_min{1e9};
  double g_max{-1e9};
  for(species_record_t const & record: history)
    {
    if(record.genus != genus or record.species.empty())
      continue;
    any = true;
    if(not same_atmosphere(record.atmosphere_type, world.atmosphere_type))
      continue;
    ++under_atmosphere;
    t_min = std::min(t_min, record.surface_temperature);
    t_max = std::max(t_max, record.surface_temperature);
    g_min = std::min(g_min, record.surface_gravity);
    g_max = std::max(g_max, record.surface_gravity);
    if(not world.star_type.empty() and star_family(record.star_type) == star_family(world.star_type))
      ++same_star;
    if(std::abs(record.surface_temperature - world.surface_temperature) <= 5.0
       and std::abs(record.surface_gravity - world.surface_gravity) <= 0.1 * world.surface_gravity)
      ++alike;
    }

  if(not any)
    return knowledge_t{.novelty = novelty_e::never};
  if(under_atmosphere == 0u)
    return knowledge_t{.novelty = novelty_e::atmosphere};

  // a kelvin or two of the scanner's rounding is no new ground, nor a few hundredths of the gravity
  constexpr double kelvin_margin{1.0};
  constexpr double gravity_margin{0.02};
  if(world.surface_temperature > t_max + kelvin_margin)
    return knowledge_t{.novelty = novelty_e::warmer, .alike = alike, .beyond = world.surface_temperature - t_max};
  if(world.surface_temperature < t_min - kelvin_margin)
    return knowledge_t{.novelty = novelty_e::colder, .alike = alike, .beyond = t_min - world.surface_temperature};
  if(world.surface_gravity > g_max * (1.0 + gravity_margin))
    return knowledge_t{.novelty = novelty_e::heavier, .alike = alike, .beyond = world.surface_gravity / g_max - 1.0};
  if(world.surface_gravity < g_min * (1.0 - gravity_margin) and world.surface_gravity > 0.0)
    return knowledge_t{.novelty = novelty_e::lighter, .alike = alike, .beyond = 1.0 - world.surface_gravity / g_min};
  if(not world.star_type.empty() and same_star == 0u)
    return knowledge_t{.novelty = novelty_e::star, .alike = alike};
  if(alike < few)
    return knowledge_t{.novelty = novelty_e::few, .alike = alike};
  return knowledge_t{.novelty = novelty_e::known, .alike = alike};
  }

auto at_risk(std::filesystem::path const & journal_dir, std::string_view commander_fid) -> at_risk_t
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  // the names carry the date, so the order of the names is the order of the sessions
  std::ranges::sort(journals, std::ranges::greater{});

  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  at_risk_t result;
  bool samples_sold{};
  // the systems whose data was sold later than the line being read, and the bodies mapped later - the
  // value tells whether that mapping used no more probes than its efficiency target
  std::set<std::string> systems_sold;
  std::map<std::string, bool> mapped;
  std::set<std::string> priced;
  bool all_handed_in{};
  // the factions whose vouchers were handed in later than the line being read
  std::set<std::string> handed_in;

  // a death is rarely further back than a few weeks of play; the bound only keeps a commander who never
  // died from reading the whole archive twice a minute
  for(std::filesystem::path const & path: journals | std::views::take(300))
    {
    std::ifstream in{path, std::ios::binary};
    std::vector<std::string> lines;
    bool ours{commander_fid.empty()};
    for(std::string line; std::getline(in, line);)
      {
      if(line.contains("\"event\":\"Commander\""))
        {
        detail::commander_line_t commander{};
        if(not glz::read<opts>(commander, line))
          ours = commander_fid.empty() or commander.FID == commander_fid;
        continue;
        }
      if(
        line.contains("\"ScanOrganic\"") or line.contains("\"SellOrganicData\"") or line.contains("\"Died\"")
        or line.contains("\"event\":\"Bounty\"") or line.contains("\"RedeemVoucher\"")
        or line.contains("\"event\":\"Scan\"") or line.contains("\"event\":\"SAAScanComplete\"")
        or line.contains("SellExplorationData\"")
      )
        lines.push_back(std::move(line));
      }
    // another account's session - its deaths and sales are not this commander's
    if(not ours)
      continue;

    for(std::string const & line: lines | std::views::reverse)
      {
      if(line.contains("\"event\":\"Died\""))
        return result;
      if(line.contains("\"event\":\"SellOrganicData\""))
        samples_sold = true;
      else if(line.contains("SellExplorationData\""))
        {
        detail::sale_line_t sale{};
        if(glz::read<opts>(sale, line))
          continue;
        for(std::string & name: sale.Systems)
          systems_sold.insert(std::move(name));
        for(glz::generic const & item: sale.Discovered)
          if(auto const * const object{item.get_if<glz::generic::object_t>()}; object != nullptr)
            if(auto const it{object->find("SystemName")}; it != object->end())
              if(auto const * const name{it->second.get_if<std::string>()}; name != nullptr)
                systems_sold.insert(*name);
        }
      else if(line.contains("\"event\":\"SAAScanComplete\""))
        {
        detail::body_line_t body{};
        if(not glz::read<opts>(body, line))
          {
          bool const efficient{body.ProbesUsed <= body.EfficiencyTarget};
          mapped.insert_or_assign(std::move(body.BodyName), efficient);
          }
        }
      else if(line.contains("\"event\":\"Scan\""))
        {
        detail::scan_line_t scan{};
        if(glz::read<opts>(scan, line) or systems_sold.contains(scan.StarSystem) or not priced.insert(scan.BodyName).second)
          continue;
        auto const mapped_it{mapped.find(scan.BodyName)};
        std::optional<bool> const mapped_efficiently{
          mapped_it != mapped.end() ? std::optional{mapped_it->second} : std::nullopt
        };
        result.cartography += body_price(scan, mapped_efficiently);
        }
      else if(line.contains("\"event\":\"RedeemVoucher\""))
        {
        detail::redeem_line_t redeem{};
        if(glz::read<opts>(redeem, line) or redeem.Type != "bounty")
          continue;
        for(detail::faction_amount_t const & faction: redeem.Factions)
          if(faction.Faction.empty())
            all_handed_in = true;
          else
            handed_in.insert(faction.Faction);
        }
      else if(line.contains("\"event\":\"Bounty\""))
        {
        detail::bounty_line_t bounty{};
        if(all_handed_in or glz::read<opts>(bounty, line))
          continue;
        for(detail::faction_reward_t const & reward: bounty.Rewards)
          if(not handed_in.contains(reward.Faction))
            result.bounties += reward.Reward;
        }
      else if(not samples_sold)
        {
        detail::analysed_t scan{};
        if(glz::read<opts>(scan, line))
          continue;
        if(scan.ScanType == "Analyse")
          {
          uint32_t const value{species_value(scan.Species_Localised).value_or(0u)};
          result.samples.push_back(
            unsold_t{.species = scan.Species_Localised, .value = value, .bonus = scan.WasLogged ? 0u : uint64_t{value} * 4u}
          );
          }
        }
      }
    }
  return result;
  }

auto cartography_sales(std::filesystem::path const & journal_dir, std::string_view commander_fid)
  -> std::vector<cartography_sale_t>
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals);

  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  std::vector<cartography_sale_t> result;
  // the bodies scanned and not sold yet, by system and name - a later scan of the same body replaces the earlier
  std::map<std::string, std::map<std::string, detail::scan_line_t>> unsold;
  std::map<std::string, bool> mapped;

  for(std::filesystem::path const & path: journals)
    {
    std::ifstream in{path, std::ios::binary};
    bool ours{commander_fid.empty()};
    for(std::string line; std::getline(in, line);)
      {
      if(line.contains("\"event\":\"Commander\""))
        {
        detail::commander_line_t commander{};
        if(not glz::read<opts>(commander, line))
          ours = commander_fid.empty() or commander.FID == commander_fid;
        continue;
        }
      // another account's session - its scans and sales are not this commander's
      if(not ours)
        continue;

      if(line.contains("\"event\":\"Scan\""))
        {
        detail::scan_line_t scan{};
        if(not glz::read<opts>(scan, line))
          unsold[scan.StarSystem].insert_or_assign(scan.BodyName, std::move(scan));
        }
      else if(line.contains("\"event\":\"SAAScanComplete\""))
        {
        detail::body_line_t body{};
        if(not glz::read<opts>(body, line))
          {
          bool const efficient{body.ProbesUsed <= body.EfficiencyTarget};
          mapped.insert_or_assign(std::move(body.BodyName), efficient);
          }
        }
      else if(line.contains("SellExplorationData\""))
        {
        detail::priced_sale_line_t sale{};
        if(glz::read<opts>(sale, line))
          continue;
        cartography_sale_t record{.base_value = sale.BaseValue, .bonus = sale.Bonus, .total = sale.TotalEarnings};
        std::chrono::sys_seconds when{};
        std::istringstream stamp{sale.timestamp};
        if(stamp >> std::chrono::parse("%FT%TZ", when))
          record.when = when;

        record.systems = std::move(sale.Systems);
        for(glz::generic const & item: sale.Discovered)
          if(auto const * const object{item.get_if<glz::generic::object_t>()}; object != nullptr)
            {
            if(auto const it{object->find("SystemName")}; it != object->end())
              if(auto const * const name{it->second.get_if<std::string>()}; name != nullptr)
                record.systems.push_back(*name);
            if(auto const it{object->find("NumBodies")}; it != object->end())
              if(auto const * const count{it->second.get_if<double>()}; count != nullptr)
                record.bodies += uint32_t(*count);
            }
          else if(auto const * const name{item.get_if<std::string>()}; name != nullptr)
            record.systems.push_back(*name);
        std::ranges::sort(record.systems);
        record.systems.erase(std::ranges::unique(record.systems).begin(), record.systems.end());

        // what was scanned in these systems is sold now, at the price the bodies had
        for(std::string const & system: record.systems)
          if(auto const found{unsold.find(system)}; found != unsold.end())
            {
            for(auto const & [name, scan]: found->second)
              {
              auto const mapped_it{mapped.find(name)};
              std::optional<bool> const mapped_efficiently{
                mapped_it != mapped.end() ? std::optional{mapped_it->second} : std::nullopt
              };
              if(uint64_t const price{body_price(scan, mapped_efficiently)}; price != 0u)
                {
                record.estimate += price;
                ++record.priced;
                }
              }
            unsold.erase(found);
            }
        result.push_back(std::move(record));
        }
      }
    }
  return result;
  }

auto estimate_accuracy(std::span<cartography_sale_t const> sales) -> std::optional<estimate_accuracy_t>
  {
  std::vector<double> ratios;
  for(cartography_sale_t const & sale: sales)
    if(sale.systems.size() == 1u and sale.estimate != 0u)
      ratios.push_back(double(sale.total) / double(sale.estimate));
  if(ratios.empty())
    return std::nullopt;
  std::ranges::sort(ratios);
  size_t const half{ratios.size() / 2u};
  double const median{ratios.size() % 2u == 1u ? ratios[half] : (ratios[half - 1u] + ratios[half]) / 2.0};
  return estimate_accuracy_t{
    .sales = ratios.size(), .median = median, .lowest = ratios.front(), .highest = ratios.back()
  };
  }

auto merge_history(std::vector<species_record_t> & into, std::vector<species_record_t> && from) -> void
  {
  // the keys of everything already in, and of everything added - the other galaxy can hold a find twice too
  std::set<std::tuple<uint64_t, uint32_t, std::string>> known;
  for(species_record_t const & record: into)
    known.emplace(record.system_address, record.body_id, record.species);
  for(species_record_t & record: from)
    if(known.emplace(record.system_address, record.body_id, record.species).second)
      into.push_back(std::move(record));
  }
  }  // namespace bio
