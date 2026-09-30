#include <picture_records.h>

#include <json_glaze.h>

#include <algorithm>
#include <cstdint>
#include <format>
#include <fstream>
#include <map>
#include <optional>
#include <ranges>
#include <set>

// named, not anonymous - glaze's reflection needs the types to have linkage
namespace pictures::detail
  {
struct organic_line_t
  {
  std::string timestamp;
  std::string ScanType;
  std::string Genus_Localised;
  std::string Species_Localised;
  std::string Variant_Localised;
  uint64_t SystemAddress{};
  uint32_t Body{};
  };

struct system_line_t
  {
  std::string timestamp;
  std::string StarSystem;
  uint64_t SystemAddress{};
  };

struct scanner_line_t
  {
  std::string timestamp;
  std::string BodyName;
  uint64_t SystemAddress{};
  };

struct scan_line_t
  {
  std::string BodyName;
  uint32_t BodyID{};
  uint64_t SystemAddress{};
  std::string StarSystem;
  double DistanceFromArrivalLS{};
  std::string StarType;
  uint32_t Subclass{};
  std::string Luminosity;
  double StellarMass{};
  double Radius{};
  double SurfaceTemperature{};
  std::string PlanetClass;
  std::string Atmosphere;
  double SurfaceGravity{};
  bool WasDiscovered{};
  };

struct approach_line_t
  {
  uint64_t SystemAddress{};
  std::string Body;
  std::optional<uint32_t> BodyID;
  };
  }  // namespace pictures::detail

namespace pictures
  {
namespace
  {
///\brief "2026-09-28T22:35:12Z" as a file names it - 20260928-223512; empty for anything else
[[nodiscard]]
auto key_of(std::string_view timestamp) -> std::string
  {
  if(timestamp.size() < 19u)
    return {};
  return std::format(
    "{}{}{}-{}{}{}",
    timestamp.substr(0, 4),
    timestamp.substr(5, 2),
    timestamp.substr(8, 2),
    timestamp.substr(11, 2),
    timestamp.substr(14, 2),
    timestamp.substr(17, 2)
  );
  }

///\brief a file's moment as the lists write it - 2026-09-28 22:35:12
[[nodiscard]]
auto taken_of(std::string_view key) -> std::string
  {
  if(key.size() != 15u)
    return {};
  return std::format(
    "{}-{}-{} {}:{}:{}",
    key.substr(0, 4),
    key.substr(4, 2),
    key.substr(6, 2),
    key.substr(9, 2),
    key.substr(11, 2),
    key.substr(13, 2)
  );
  }

[[nodiscard]]
auto is_key(std::string_view text) -> bool
  {
  if(text.size() != 15u or text[8] != '-')
    return false;
  for(size_t ix{}; ix != text.size(); ++ix)
    if(ix != 8u and (text[ix] < '0' or text[ix] > '9'))
      return false;
  return true;
  }

///\brief a name as file_safe left it, read back as well as it can be
[[nodiscard]]
auto readable(std::string_view safe) -> std::string
  {
  std::string text{safe};
  std::ranges::replace(text, '_', ' ');
  return text;
  }

///\brief what the journals say of the moments the pictures were taken for, read in one pass
struct facts_t
  {
  std::map<std::string, std::vector<detail::organic_line_t>> organics;
  std::map<std::string, std::vector<detail::system_line_t>> jumps;
  std::map<std::string, std::vector<detail::scanner_line_t>> scanners;
  std::map<uint64_t, std::string> system_names;
  std::map<std::pair<uint64_t, uint32_t>, std::string> body_names;
  ///\brief the scans of the systems the album has pictures of, by the body's name
  std::map<std::string, detail::scan_line_t> scans;
  };

[[nodiscard]]
auto read_facts(
  std::filesystem::path const & journal_dir, std::set<std::string> const & keys, std::set<std::string> const & systems
) -> facts_t
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals);

  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  facts_t facts;
  auto const wanted = [&](std::string const & line) -> std::string
  {
    // "timestamp" is the first field the game writes
    constexpr std::string_view head{"{ \"timestamp\":\""};
    if(not line.starts_with(head))
      return {};
    std::string key{key_of(std::string_view{line}.substr(head.size()))};
    return keys.contains(key) ? key : std::string{};
  };

  for(std::filesystem::path const & path: journals)
    {
    std::ifstream in{path, std::ios::binary};
    for(std::string line; std::getline(in, line);)
      {
      if(line.contains("\"event\":\"ScanOrganic\""))
        {
        if(std::string const key{wanted(line)}; not key.empty())
          if(detail::organic_line_t organic{}; not glz::read<opts>(organic, line))
            facts.organics[key].push_back(std::move(organic));
        }
      else if(
        line.contains("\"event\":\"FSDJump\"") or line.contains("\"event\":\"Location\"")
        or line.contains("\"event\":\"CarrierJump\"")
      )
        {
        detail::system_line_t system{};
        if(glz::read<opts>(system, line))
          continue;
        facts.system_names[system.SystemAddress] = system.StarSystem;
        if(line.contains("\"event\":\"FSDJump\""))
          if(std::string const key{wanted(line)}; not key.empty())
            facts.jumps[key].push_back(std::move(system));
        }
      else if(line.contains("\"event\":\"SAAScanComplete\"") or line.contains("\"event\":\"SAASignalsFound\""))
        {
        if(std::string const key{wanted(line)}; not key.empty())
          if(detail::scanner_line_t scanner{}; not glz::read<opts>(scanner, line))
            facts.scanners[key].push_back(std::move(scanner));
        }
      else if(line.contains("\"event\":\"Scan\""))
        {
        detail::scan_line_t scan{};
        if(glz::read<opts>(scan, line))
          continue;
        facts.body_names[{scan.SystemAddress, scan.BodyID}] = scan.BodyName;
        if(systems.contains(file_safe(scan.StarSystem)))
          facts.scans[scan.BodyName] = std::move(scan);
        }
      else if(line.contains("\"event\":\"ApproachBody\"") or line.contains("\"event\":\"Touchdown\""))
        {
        detail::approach_line_t approach{};
        if(not glz::read<opts>(approach, line) and approach.BodyID and not approach.Body.empty())
          facts.body_names[{approach.SystemAddress, *approach.BodyID}] = approach.Body;
        }
      }
    }
  return facts;
  }
  }  // namespace

auto file_safe(std::string_view text) -> std::string
  {
  std::string result;
  for(char c: text)
    if((c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '-')
      result.push_back(c);
    else if(not result.empty() and result.back() != '_')
      result.push_back('_');
  while(not result.empty() and result.back() == '_')
    result.pop_back();
  return result;
  }

auto stem(std::chrono::sys_seconds at, std::string_view name) -> std::string
  { return std::format("{:%Y%m%d-%H%M%S}_{}", at, file_safe(name)); }

auto star_detail(
  std::string_view star_type,
  uint32_t subclass,
  std::string_view luminosity,
  double solar_masses,
  double temperature_k,
  double radius_m
) -> std::string
  {
  return std::format(
    "{}{} {}, {:.2f} solar masses{}, radius {:.0f} km",
    star_type,
    subclass,
    luminosity,
    solar_masses,
    // a black hole's temperature is written as nought
    temperature_k > 0.0 ? std::format(", {:.0f} K", temperature_k) : std::string{},
    radius_m / 1'000.0
  );
  }

auto planet_detail(std::string_view planet_class, std::string_view atmosphere, double gravity_ms2, double temperature_k)
  -> std::string
  {
  std::string detail{planet_class};
  if(not atmosphere.empty())
    detail += std::format(", {}", atmosphere);
  detail += std::format(", {:.2f} g, {:.0f} K", gravity_ms2 / 9.80665, temperature_k);
  return detail;
  }

auto pictures_under(std::filesystem::path const & codex_dir, std::string_view sub) -> std::vector<std::string>
  {
  std::vector<std::string> files;
  std::error_code ec;
  for(auto const & entry: std::filesystem::recursive_directory_iterator{codex_dir / sub, ec})
    if(entry.is_regular_file(ec) and entry.path().extension() == ".jpg")
      files.push_back(entry.path().lexically_relative(codex_dir).generic_string());
  std::ranges::sort(files);
  return files;
  }

auto rebuild_codex(std::vector<std::string> const & files, std::filesystem::path const & journal_dir)
  -> std::vector<codex_record_t>
  {
  // pictures/20260927-052256_Aleoida_Coronamus_128.jpg - the sample's moment, the species, a number
  struct name_t
    {
    std::string file;
    std::string key;
    std::string species;
    };
  std::vector<name_t> names;
  std::set<std::string> keys;
  for(std::string const & file: files)
    {
    std::string const stem_part{std::filesystem::path{file}.stem().string()};
    size_t const first{stem_part.find('_')};
    size_t const last{stem_part.rfind('_')};
    std::string const key{stem_part.substr(0, first)};
    std::string species{
      first != std::string::npos and last > first ? stem_part.substr(first + 1u, last - first - 1u) : std::string{}
    };
    names.push_back(name_t{.file = file, .key = key, .species = std::move(species)});
    if(is_key(key))
      keys.insert(key);
    }

  facts_t const facts{keys.empty() ? facts_t{} : read_facts(journal_dir, keys, {})};
  std::vector<codex_record_t> records;
  for(name_t const & name: names)
    {
    codex_record_t record{.file = name.file, .taken = taken_of(name.key), .species = readable(name.species)};
    if(auto const it{facts.organics.find(name.key)}; it != facts.organics.end())
      for(detail::organic_line_t const & organic: it->second)
        if(file_safe(organic.Species_Localised) == name.species)
          {
          record.system_address = organic.SystemAddress;
          record.body_id = organic.Body;
          record.genus = organic.Genus_Localised;
          record.species = organic.Species_Localised;
          record.variant = organic.Variant_Localised;
          record.scan = organic.ScanType;
          if(auto const system{facts.system_names.find(organic.SystemAddress)}; system != facts.system_names.end())
            record.system = system->second;
          if(auto const body{facts.body_names.find({organic.SystemAddress, organic.Body})}; body != facts.body_names.end())
            record.body = body->second;
          break;
          }
    records.push_back(std::move(record));
    }
  return records;
  }

auto rebuild_sky(std::vector<std::string> const & files, std::filesystem::path const & journal_dir)
  -> std::vector<sky_record_t>
  {
  // sky/<system>/20260928-223512_<body>[-1.0s].jpg - the jump's or the scanner's moment, the body, the series' step
  struct name_t
    {
    std::string file;
    std::string system;
    std::string key;
    std::string body;
    std::string step;
    };
  std::vector<name_t> names;
  std::set<std::string> keys;
  std::set<std::string> systems;
  for(std::string const & file: files)
    {
    std::filesystem::path const path{file};
    std::string stem_part{path.stem().string()};
    name_t name{.file = file, .system = path.parent_path().filename().string(), .key = {}, .body = {}, .step = {}};
    // a series' step ends the name: -1.0s - the point is its own, file_safe keeps no other
    if(size_t const dash{stem_part.rfind('-')};
       dash != std::string::npos and stem_part.ends_with('s') and stem_part.find('.', dash) != std::string::npos)
      {
      name.step = stem_part.substr(dash + 1u, stem_part.size() - dash - 1u);
      stem_part.resize(dash);
      }
    if(size_t const under{stem_part.find('_')}; under != std::string::npos and is_key(stem_part.substr(0, under)))
      {
      name.key = stem_part.substr(0, under);
      name.body = stem_part.substr(under + 1u);
      keys.insert(name.key);
      }
    else
      name.body = stem_part;
    systems.insert(name.system);
    names.push_back(std::move(name));
    }

  facts_t const facts{keys.empty() ? facts_t{} : read_facts(journal_dir, keys, systems)};
  std::vector<sky_record_t> records;
  for(name_t const & name: names)
    {
    sky_record_t record{
      .file = name.file,
      .taken = taken_of(name.key) + (name.step.empty() ? std::string{} : std::format("  ({} after)", name.step)),
      .kind = {},
      .system = readable(name.system),
      .body = readable(name.body),
      .detail = {},
      .first = false
    };

    // the jump into the system: the picture is of its arrival star
    if(auto const jump{facts.jumps.find(name.key)}; jump != facts.jumps.end())
      for(detail::system_line_t const & system: jump->second)
        if(file_safe(system.StarSystem) == name.system)
          {
          record.kind = "star";
          record.system = system.StarSystem;
          for(auto const & [body, scan]: facts.scans)
            if(scan.SystemAddress == system.SystemAddress and not scan.StarType.empty() and scan.DistanceFromArrivalLS == 0.0)
              {
              record.body = body;
              record.detail = star_detail(
                scan.StarType, scan.Subclass, scan.Luminosity, scan.StellarMass, scan.SurfaceTemperature, scan.Radius
              );
              record.first = not scan.WasDiscovered;
              break;
              }
          break;
          }

    // the scanner closing on a body: the picture is of that body
    if(record.kind.empty())
      if(auto const scanner{facts.scanners.find(name.key)}; scanner != facts.scanners.end())
        for(detail::scanner_line_t const & seen: scanner->second)
          if(file_safe(seen.BodyName) == name.body)
            {
            record.kind = "planet";
            record.body = seen.BodyName;
            if(auto const system{facts.system_names.find(seen.SystemAddress)}; system != facts.system_names.end())
              record.system = system->second;
            if(auto const scan{facts.scans.find(seen.BodyName)}; scan != facts.scans.end())
              {
              record.detail = planet_detail(
                scan->second.PlanetClass, scan->second.Atmosphere, scan->second.SurfaceGravity, scan->second.SurfaceTemperature
              );
              record.first = not scan->second.WasDiscovered;
              }
            break;
            }
    records.push_back(std::move(record));
    }
  return records;
  }
  }  // namespace pictures
