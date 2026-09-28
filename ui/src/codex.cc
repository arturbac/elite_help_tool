#include <codex.h>
#include <picture_records.h>
#include <eht_settings.h>

#include <glaze/glaze.hpp>
#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <algorithm>
#include <chrono>
#include <format>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace codex_files
  {
auto codex_dir() -> std::filesystem::path
  { return std::filesystem::absolute(eht::settings()->exploration.codex_dir); }

auto spool_dir() -> std::filesystem::path
  { return std::filesystem::path{overlay::default_spool_path()}; }

auto file_safe(std::string_view text) -> std::string
  { return pictures::file_safe(text); }
  }  // namespace codex_files

namespace
  {
using codex_files::codex_dir;
using codex_files::file_safe;
using codex_files::spool_dir;

[[nodiscard]]
auto html(std::string_view text) -> std::string
  {
  std::string result;
  result.reserve(text.size());
  for(char c: text)
    switch(c)
      {
      case '&': result += "&amp;"; break;
      case '<': result += "&lt;"; break;
      case '>': result += "&gt;"; break;
      case '"': result += "&quot;"; break;
      default:  result.push_back(c);
      }
  return result;
  }

[[nodiscard]]
auto credits(uint32_t value) -> std::string
  {
  if(value >= 1'000'000u)
    return std::format("{:.2f}M", double(value) / 1'000'000.0);
  return std::format("{}k", (value + 500u) / 1000u);
  }

///\brief the families as the codex groups them - the main genera by the first word, the lone species by their own name
[[nodiscard]]
auto family_of(std::string_view species) -> std::string
  {
  static constexpr std::array<std::string_view, 15> genera{
    "Aleoida",
    "Bacterium",
    "Cactoida",
    "Clypeus",
    "Concha",
    "Electricae",
    "Fonticulua",
    "Frutexa",
    "Fumerola",
    "Fungoida",
    "Osseus",
    "Recepta",
    "Stratum",
    "Tubus",
    "Tussock"
  };
  for(std::string_view genus: genera)
    if(species.starts_with(genus))
      return std::string{genus};
  // the journal's "Roseum Brain Tree" and the price list's "Brain Tree" belong together
  for(std::string_view lone: {"Amphora", "Anemone", "Bark Mound", "Brain Tree", "Crystalline Shard", "Tuber"})
    if(species.contains(lone))
      return std::string{lone} + (lone.ends_with('s') ? "" : "s");
  return std::string{species};
  }

struct range_t
  {
  double low{1e18};
  double high{-1e18};

  auto add(double value) -> void
    {
    low = std::min(low, value);
    high = std::max(high, value);
    }
  };

///\brief "CarbonDioxide 4, Ammonia 2" - what was seen, the commonest first
[[nodiscard]]
auto tally(std::map<std::string, uint32_t> const & counts) -> std::string
  {
  std::vector<std::pair<std::string, uint32_t>> sorted{counts.begin(), counts.end()};
  std::ranges::sort(sorted, std::ranges::greater{}, &std::pair<std::string, uint32_t>::second);
  std::string result;
  for(auto const & [name, count]: sorted)
    {
    if(not result.empty())
      result += ", ";
    result += std::format("{} {}", html(name.empty() ? std::string{"none"} : name), count);
    }
  return result;
  }

constexpr std::string_view page_style{R"css(
:root { color-scheme: dark; }
body { margin: 0; background: #0c0e11; color: #d6dbe1; font: 15px/1.45 system-ui, sans-serif; }
header { padding: 20px 24px 8px; border-bottom: 1px solid #262b31; }
h1 { margin: 0 0 4px; color: #ff9a2e; font-weight: 600; letter-spacing: .02em; }
.summary { color: #9aa4ae; }
nav { padding: 10px 24px; position: sticky; top: 0; background: #0c0e11ee; border-bottom: 1px solid #1d2126; }
nav a { color: #ffb562; margin-right: 12px; text-decoration: none; white-space: nowrap; }
main { padding: 8px 24px 40px; max-width: 1500px; }
h2 { color: #ff9a2e; margin: 28px 0 6px; border-bottom: 1px solid #262b31; padding-bottom: 4px; }
h2 small, h3 small { color: #8b949e; font-weight: 400; }
.species { background: #13171b; border: 1px solid #22282e; border-radius: 6px; padding: 10px 14px; margin: 10px 0; }
h3 { margin: 0 0 4px; color: #e8edf2; }
.worth { color: #3cb371; }
.poor { color: #8a8a8a; }
.facts { color: #aab3bc; font-size: 14px; }
.facts b { color: #c9d1d9; font-weight: 500; }
.pictures { display: flex; flex-wrap: wrap; gap: 8px; margin: 8px 0; }
figure { margin: 0; width: 220px; }
figure img { width: 220px; height: 220px; object-fit: cover; border-radius: 4px; border: 1px solid #2a3037; }
figcaption { font-size: 12px; color: #9aa4ae; }
details { margin-top: 6px; }
summary { cursor: pointer; color: #9ad1ff; }
table { border-collapse: collapse; margin-top: 6px; font-size: 13px; }
th, td { text-align: left; padding: 2px 10px 2px 0; border-bottom: 1px solid #1d2126; white-space: nowrap; }
th { color: #8b949e; font-weight: 500; }
.missing { color: #6f7780; font-size: 14px; }
.missing span { margin-right: 14px; white-space: nowrap; }
@media (max-width: 700px) { main, header, nav { padding-left: 12px; padding-right: 12px; } figure, figure img { width: 44vw; height: auto; } }
)css"};
  }  // namespace

template<>
struct glz::meta<codex_t::picture_t>
  {
  using T = codex_t::picture_t;
  static constexpr auto value{glz::object(
    "file",
    &T::file,
    "taken",
    &T::taken,
    "system",
    &T::system,
    "body",
    &T::body,
    "system_address",
    &T::system_address,
    "body_id",
    &T::body_id,
    "genus",
    &T::genus,
    "species",
    &T::species,
    "variant",
    &T::variant,
    "scan",
    &T::scan,
    "latitude",
    &T::latitude,
    "longitude",
    &T::longitude
  )};
  };

codex_t::codex_t() = default;

auto codex_t::page_path() -> std::filesystem::path { return codex_dir() / "index.html"; }

auto codex_t::load_pictures() -> void
  {
  pictures_loaded_ = true;
  std::filesystem::path const list{codex_dir() / "pictures.json"};
  std::error_code ec;
  if(not std::filesystem::exists(list, ec))
    return describe_unlisted();
  std::string buffer;
  if(
    auto const err{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(pictures_, list.string(), buffer)};
    err
  )
    {
    // kept aside rather than written over - whatever it still holds may be read by hand
    std::filesystem::path const broken{list.string() + ".broken"};
    std::filesystem::rename(list, broken, ec);
    spdlog::error("codex: {} could not be read, kept as {}; the pictures are described again", list.string(), broken.string());
    pictures_.clear();
    }
  describe_unlisted();
  }

auto codex_t::describe_unlisted() -> void
  {
  // every picture is named after its sample's moment in the journal, so one missing from the list - a list
  // lost, broken, or a picture copied back in - is described again out of the journals
  std::vector<std::string> unlisted;
  for(std::string & file: pictures::pictures_under(codex_dir(), "pictures"))
    if(std::ranges::none_of(pictures_, [&](picture_t const & picture) { return picture.file == file; }))
      unlisted.push_back(std::move(file));
  if(unlisted.empty() or journal_dir_.empty())
    return;
  for(pictures::codex_record_t & record: pictures::rebuild_codex(unlisted, journal_dir_))
    pictures_.push_back(picture_t{
      .file = std::move(record.file),
      .taken = std::move(record.taken),
      .system = std::move(record.system),
      .body = std::move(record.body),
      .system_address = record.system_address,
      .body_id = record.body_id,
      .genus = std::move(record.genus),
      .species = std::move(record.species),
      .variant = std::move(record.variant),
      .scan = std::move(record.scan),
      .latitude = 0.0,
      .longitude = 0.0
    });
  std::ranges::sort(pictures_, {}, &picture_t::file);
  spdlog::info("codex: {} pictures described again out of the journals", unlisted.size());
  save_pictures();
  }

auto codex_t::save_pictures() const -> void
  {
  std::filesystem::path const list{codex_dir() / "pictures.json"};
  std::string text;
  if(auto const err{glz::write<glz::opts{.prettify = true}>(pictures_, text)}; err)
    return;
  std::filesystem::path const partial{list.string() + ".part"};
    {
    std::ofstream out{partial, std::ios::binary | std::ios::trunc};
    out << text << '\n';
    if(not out)
      {
      spdlog::error("codex: could not write {}", partial.string());
      return;
      }
    }
  std::error_code ec;
  std::filesystem::rename(partial, list, ec);
  if(ec)
    spdlog::error("codex: could not put {} in place: {}", list.string(), ec.message());
  }

auto codex_t::ask_for_picture(current_state_t::organic_scan_seen_t const & scan) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->exploration.capture)
    return;
  // at the third sample the commander holds up the sealed canister and the tool resets - the view is
  // blocked every time, and the first two have already shown the plant
  if(scan.sample >= 3u)
    return;

  std::error_code ec;
  std::filesystem::create_directories(spool_dir(), ec);
  if(ec)
    {
    spdlog::error("codex: cannot make {}: {}", spool_dir().string(), ec.message());
    return;
    }

  // the number is the moment, so a restarted tool never repeats one the layer has already served
  uint64_t const id{uint64_t(
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
  )};
  std::string const taken{std::format("{:%Y-%m-%d %H:%M:%S}", scan.timestamp)};
  std::string const stem{
    std::format("{:%Y%m%d-%H%M%S}_{}_{}", scan.timestamp, file_safe(scan.scan.Species_Localised), id % 1000u)
  };

  request_ = overlay::capture_t{
    .id = id,
    .path = (spool_dir() / (stem + ".ppm")).string(),
    .size = cfg->exploration.capture_size,
    .delay_ms = cfg->exploration.capture_delay_ms
  };
  pending_.push_back(
    pending_t{
      .spool = request_.path,
      .picture = picture_t{
        .file = "pictures/" + stem + ".jpg",
        .taken = taken,
        .system = scan.system_name,
        .body = scan.body_name,
        .system_address = scan.scan.SystemAddress,
        .body_id = scan.scan.Body,
        .genus = scan.scan.Genus_Localised,
        .species = scan.scan.Species_Localised,
        .variant = scan.scan.Variant_Localised,
        .scan = std::string{simple_enum::enum_name(scan.scan.ScanType)},
        .latitude = scan.point ? scan.point->latitude : 0.0,
        .longitude = scan.point ? scan.point->longitude : 0.0
      },
      // the layer waits before it takes it, and the wait is no sign of a missing layer
      .asked = std::chrono::steady_clock::now() + std::chrono::milliseconds{cfg->exploration.capture_delay_ms}
    }
  );
  spdlog::info("codex: picture of {} asked for", scan.scan.Species_Localised);
  }

auto codex_t::collect() -> bool
  {
  if(pending_.empty())
    return false;
  if(not pictures_loaded_)
    load_pictures();

  bool added{};
  auto const now{std::chrono::steady_clock::now()};
  std::erase_if(
    pending_,
    [&](pending_t & p) -> bool
    {
      std::error_code ec;
      if(not std::filesystem::exists(p.spool, ec))
        {
        // a layer too old to take pictures, or a game not running - the moment is gone either way
        if(now - p.asked > std::chrono::seconds{20})
          {
          spdlog::warn(
            "codex: no picture came for {} - is the layer installed and the game running?", p.picture.species
          );
          return true;
          }
        return false;
        }

      std::filesystem::path const target{codex_dir() / p.picture.file};
      std::filesystem::create_directories(target.parent_path(), ec);
      QImage const image{QString::fromStdString(p.spool.string())};
      if(image.isNull())
        spdlog::error("codex: the picture {} could not be read", p.spool.string());
      else if(not image.save(
                QString::fromStdString(target.string()), "JPG", int(eht::settings()->exploration.jpeg_quality)
              ))
        spdlog::error("codex: the picture could not be saved as {}", target.string());
      else
        {
        pictures_.push_back(p.picture);
        added = true;
        spdlog::info("codex: picture of {} filed as {}", p.picture.species, target.string());
        }
      // the spool file is ours - we named it, and the layer wrote nothing else under that name
      std::filesystem::remove(p.spool, ec);
      return true;
    }
  );

  if(added)
    save_pictures();
  return added;
  }

auto codex_t::write_page(database_storage_t & db) -> void
  {
  if(not pictures_loaded_)
    load_pictures();

  auto finds_read{db.load_codex_finds()};
  if(not finds_read)
    {
    spdlog::error("codex: the finds could not be read");
    return;
    }
  std::vector<bio::find_t> const & finds{*finds_read};
  uint32_t const worth{eht::settings()->exploration.bio_worth};

  // species -> its finds and its pictures, families in alphabetical order
  struct species_entry_t
    {
    std::vector<bio::find_t const *> finds;
    std::vector<picture_t const *> pictures;
    };

  std::map<std::string, std::map<std::string, species_entry_t>> families;
  for(bio::find_t const & find: finds)
    families[family_of(find.species)][find.species].finds.push_back(&find);
  for(picture_t const & picture: pictures_)
    families[family_of(picture.species)][picture.species].pictures.push_back(&picture);

  std::set<std::string> known;
  size_t analysed{};
  for(bio::find_t const & find: finds)
    {
    known.insert(find.species);
    analysed += find.sampled ? 1u : 0u;
    }

  std::ostringstream out;
  out << "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
         "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
         "<title>Codex of finds</title><style>"
      << page_style << "</style></head><body>\n";
  out << std::format(
    "<header><h1>Codex of finds</h1><div class=\"summary\">{} species of {} priced, {} finds, {} analysed "
    "here, {} pictures - written {:%Y-%m-%d %H:%M} UTC</div></header>\n",
    known.size(),
    organic_values.size(),
    finds.size(),
    analysed,
    pictures_.size(),
    std::chrono::floor<std::chrono::minutes>(std::chrono::system_clock::now())
  );

  out << "<nav>";
  for(auto const & [family, species]: families)
    out << std::format("<a href=\"#{}\">{}</a>", file_safe(family), html(family));
  out << "</nav>\n<main>\n";

  for(auto const & [family, species_map]: families)
    {
    uint32_t const range{bio::colony_range_m(family)};
    // the price list knows the lone kinds by one name for all their colours - "Anemone" for every
    // anemone - so there the finds say how many species there are, and nothing is missing once one is found
    bool const priced_as_one{bio::colony_range_m(family) == 100u and family != "Fumerola"};
    size_t const priced{std::max(
      species_map.size(),
      size_t(
        std::ranges::count_if(organic_values, [&](organic_value_t const & v) { return family_of(v.species) == family; })
      )
    )};
    out << std::format(
      "<h2 id=\"{}\">{} <small>{} of {} species found{}</small></h2>\n",
      file_safe(family),
      html(family),
      species_map.size(),
      priced,
      range != 0u ? std::format(", colony {} m", range) : std::string{}
    );

    // the valuable first - they are the ones worth reading about before the next landing
    std::vector<std::pair<std::string, species_entry_t const *>> ordered;
    for(auto const & [name, entry]: species_map)
      ordered.emplace_back(name, &entry);
    std::ranges::sort(
      ordered, std::ranges::greater{}, [](auto const & e) { return bio::species_value(e.first).value_or(0u); }
    );

    for(auto const & [name, entry]: ordered)
      {
      uint32_t const value{bio::species_value(name).value_or(0u)};
      out << std::format(
        "<section class=\"species\"><h3>{} <small class=\"{}\">{}</small></h3>\n",
        html(name),
        value >= worth ? "worth" : "poor",
        value != 0u ? credits(value) : std::string{"price unknown"}
      );

      if(not entry->finds.empty())
        {
        std::map<std::string, uint32_t> atmospheres;
        std::map<std::string, uint32_t> classes;
        std::map<std::string, uint32_t> stars;
        range_t temperature;
        range_t gravity;
        size_t done{};
        for(bio::find_t const * find: entry->finds)
          {
          ++atmospheres[find->atmosphere_type];
          ++classes[find->planet_class];
          ++stars[find->star_type.empty() ? std::string{"?"} : find->star_type];
          temperature.add(find->surface_temperature);
          gravity.add(find->surface_gravity / 9.80665);
          done += find->sampled ? 1u : 0u;
          }
        out << std::format(
          "<div class=\"facts\"><b>{} finds</b>{} &middot; {:.0f}-{:.0f} K &middot; {:.2f}-{:.2f} g<br>"
          "<b>sky</b> {}<br><b>world</b> {}<br><b>star</b> {}</div>\n",
          entry->finds.size(),
          done != 0u ? std::format(", {} analysed", done) : std::string{},
          temperature.low,
          temperature.high,
          gravity.low,
          gravity.high,
          tally(atmospheres),
          tally(classes),
          tally(stars)
        );
        }

      if(not entry->pictures.empty())
        {
        out << "<div class=\"pictures\">";
        for(picture_t const * picture: entry->pictures)
          out << std::format(
            "<figure><a href=\"{0}\"><img loading=\"lazy\" src=\"{0}\" alt=\"{1}\"></a>"
            "<figcaption>{1}<br>{2} {3}<br>{4} &middot; {5}</figcaption></figure>",
            html(picture->file),
            html(picture->variant.empty() ? picture->species : picture->variant),
            html(picture->system),
            html(
              picture->body.starts_with(picture->system) and picture->body.size() > picture->system.size()
                ? picture->body.substr(picture->system.size() + 1u)
                : picture->body
            ),
            html(picture->taken),
            html(picture->scan)
          );
        out << "</div>\n";
        }

      if(not entry->finds.empty())
        {
        out << "<details><summary>where</summary><table><tr><th>system</th><th>body</th><th>world</th>"
               "<th>sky</th><th>K</th><th>g</th><th>atm</th><th>star</th><th>ls</th><th></th></tr>\n";
        for(bio::find_t const * find: entry->finds)
          out << std::format(
            "<tr><td>{}</td><td>{}</td><td>{}</td><td>{}</td><td>{:.0f}</td><td>{:.2f}</td><td>{:.3f}</td>"
            "<td>{}</td><td>{:.0f}</td><td>{}</td></tr>\n",
            html(find->system_name),
            html(find->body_name),
            html(find->planet_class),
            html(find->atmosphere_type),
            find->surface_temperature,
            find->surface_gravity / 9.80665,
            find->surface_pressure / 101325.0,
            html(find->star_type),
            find->distance_from_arrival_ls,
            find->sampled ? "analysed" : ""
          );
        out << "</table></details>\n";
        }
      out << "</section>\n";
      }

    // what the family still holds - the next thing to look for
    std::string missing;
    for(organic_value_t const & v: organic_values)
      if(not priced_as_one and family_of(v.species) == family and not species_map.contains(std::string{v.species}))
        missing += std::format("<span>{} {}</span>", html(v.species), credits(v.value));
    if(not missing.empty())
      out << "<div class=\"missing\">not found yet: " << missing << "</div>\n";
    }

  // families never found at all, so the list of what is out there is complete
  std::string untouched;
  std::set<std::string> listed;
  for(organic_value_t const & v: organic_values)
    if(std::string const family{family_of(v.species)}; not families.contains(family) and listed.insert(family).second)
      untouched += std::format("<span>{}</span>", html(family));
  if(not untouched.empty())
    out << "<h2>never found <small>families</small></h2><div class=\"missing\">" << untouched << "</div>\n";

  out << "</main></body></html>\n";

  std::error_code ec;
  std::filesystem::create_directories(codex_dir(), ec);
  std::filesystem::path const target{page_path()};
  std::filesystem::path const partial{target.string() + ".part"};
    {
    std::ofstream file{partial, std::ios::binary | std::ios::trunc};
    file << out.str();
    if(not file)
      {
      spdlog::error("codex: could not write {}", partial.string());
      return;
      }
    }
  std::filesystem::rename(partial, target, ec);
  if(ec)
    spdlog::error("codex: could not put {} in place: {}", target.string(), ec.message());
  else
    spdlog::info("codex: {} written, {} species, {} pictures", target.string(), known.size(), pictures_.size());
  }
