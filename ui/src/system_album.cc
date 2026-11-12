#include <system_album.h>

#include <data/station.h>
#include <databse_storage.h>
#include <diagram_svg.h>
#include <eht_settings.h>
#include <picture_records.h>
#include <star_system.h>
#include <system_diagram.h>
#include <system_index.h>

#include <simple_enum/simple_enum.hpp>
#include <spdlog/spdlog.h>

#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <format>
#include <fstream>
#include <map>
#include <ranges>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
  {
///\brief goes up when the pages change their shape, so all of them are made again
constexpr uint32_t page_version{2u};
constexpr std::string_view mark{"<!-- eht-album "};
constexpr double standard_gravity{9.80665};
constexpr double seconds_in_a_day{86400.0};
constexpr double degrees_in_a_radian{57.29577951308232};

struct shot_t
  {
  std::string file;
  std::string title;
  std::string who;
  };

struct sky_shot_t
  {
  std::string file;
  std::string title;
  std::string who;
  bool first{};
  };

///\brief what the pages show besides the database, of all the accounts
struct gathered_t
  {
  std::map<std::pair<uint64_t, uint32_t>, std::vector<shot_t>> samples;
  std::map<std::string, std::vector<sky_shot_t>> sky;
  std::map<std::pair<uint64_t, uint32_t>, std::vector<pictures::signed_find_t const *>> finds;
  std::vector<pictures::signed_find_t> all_finds;
  std::vector<uint64_t> sampled_systems;
  std::map<std::string, std::string> faces;
  };

[[nodiscard]]
auto fnv(uint64_t hash, std::string_view text) noexcept -> uint64_t
  {
  for(char const c: text)
    hash = (hash ^ static_cast<unsigned char>(c)) * 0x100000001b3ull;
  return hash;
  }

[[nodiscard]]
auto first_line(std::filesystem::path const & path) -> std::string
  {
  std::ifstream in{path};
  std::string line;
  std::getline(in, line);
  return line;
  }

auto add_faces(gathered_t & gathered, std::filesystem::path const & own, std::filesystem::path const & dir) -> void
  {
  std::error_code ec;
  for(std::filesystem::directory_entry const & entry: std::filesystem::directory_iterator{dir / "faces", ec})
    if(entry.path().extension() == ".png")
      gathered.faces.try_emplace(
        entry.path().stem().string(), "../" + pictures::seen_from(own, dir, "faces/" + entry.path().filename().string())
      );
  }

auto add_pictures(
  gathered_t & gathered, std::filesystem::path const & own, std::filesystem::path const & dir, std::string const & who
) -> void
  {
  for(pictures::codex_record_t & record: pictures::read_codex_list(dir))
    gathered.samples[{record.system_address, record.body_id}].push_back(
      shot_t{
        .file = "../" + pictures::seen_from(own, dir, record.file),
        .title = record.variant.empty() ? record.species : record.variant,
        .who = who
      }
    );
  for(pictures::sky_record_t & record: pictures::read_sky_list(dir))
    {
    std::string title{
      record.body.size() > record.system.size() and record.body.starts_with(record.system)
        ? record.body.substr(record.system.size() + 1u)
        : std::string{"arrival star"}
    };
    gathered.sky[record.system].push_back(
      sky_shot_t{
        .file = "../" + pictures::seen_from(own, dir, record.file),
        .title = std::move(title),
        .who = who,
        .first = record.first
      }
    );
    }
  add_faces(gathered, own, dir);
  }

[[nodiscard]]
auto gather(std::filesystem::path const & own) -> gathered_t
  {
  auto const cfg{eht::settings()};
  gathered_t gathered;
  std::string const own_name{cfg->exploration.shared_codex.empty() ? std::string{} : cfg->exploration.codex_name};
  add_pictures(gathered, own, own, own_name);
  auto const own_finds{pictures::read_finds(own)};
  pictures::merge_finds(gathered.all_finds, own_finds, own_name);
  for(eht::shared_codex_t const & other: cfg->exploration.shared_codex)
    {
    add_pictures(gathered, own, other.dir, other.name);
    auto const finds{pictures::read_finds(other.dir)};
    pictures::merge_finds(gathered.all_finds, finds, other.name);
    }
  for(pictures::signed_find_t const & find: gathered.all_finds)
    gathered.finds[{find.find.system_address, find.find.body_id}].push_back(&find);
  return gathered;
  }

///\brief a row of a body's card
auto add_row(std::string & table, std::string_view key, std::string_view value) -> void
  { table += std::format("<tr><td>{}</td><td>{}</td></tr>", key, system_index::escaped(value)); }

[[nodiscard]]
auto body_card(star_system_t const & system, body_t const & body, gathered_t const & gathered) -> std::string
  {
  using system_index::escaped;
  std::string title;
  std::string table;
  add_row(table, "from arrival", std::format("{:.0f} ls", body.distance_from_arrival_ls));
  std::string bio;
  std::string air;
  std::string badges;
  if(not body.was_discovered)
    badges = "first discovery";
  auto const badge = [&badges](std::string_view text) { badges += std::format("{}{}", badges.empty() ? "" : ", ", text); };
  auto const add_motion = [&](std::optional<double> rotation, bool locked)
  {
    if(body.orbital_period > 0.0)
      add_row(table, "orbit", std::format("{:.1f} days", body.orbital_period / seconds_in_a_day));
    if(rotation and *rotation != 0.0)
      add_row(table, "rotation", std::format("{:.1f} days{}", std::abs(*rotation) / seconds_in_a_day, locked ? ", locked" : ""));
  };
  if(auto const * star{std::get_if<star_details_t>(&body.details)}; star != nullptr)
    {
    title = std::format("{}{} {}", star->star_type, star->sub_class, star->luminosity);
    add_row(table, "mass", std::format("{:.2f} sol", star->stellar_mass));
    add_row(table, "temperature", std::format("{:.0f} K", star->surface_temperature));
    add_row(table, "age", std::format("{} My", star->age_my));
    add_motion(star->rotation_period, false);
    }
  else
    {
    auto const & planet{std::get<planet_details_t>(body.details)};
    title = planet.planet_class;
    add_row(table, "gravity", std::format("{:.2f} g", planet.surface_gravity / standard_gravity));
    add_row(table, "temperature", std::format("{:.0f} K", planet.surface_temperature));
    if(not planet.atmosphere.empty())
      add_row(table, "atmosphere", planet.atmosphere);
    if(not planet.volcanism.empty())
      add_row(table, "volcanism", planet.volcanism);
    if(planet.landable)
      add_row(table, "landable", "yes");
    if(planet.mapped)
      add_row(table, "mapped", "yes");
    add_motion(planet.rotation_period, planet.tidal_lock);
    if(planet.axial_tilt)
      add_row(table, "axial tilt", std::format("{:.0f}\u00b0", std::abs(*planet.axial_tilt) * degrees_in_a_radian));
    for(events::signal_t const & signal: planet.signals_)
      if(signal.Type_Localised == "Geological" or signal.Type_Localised == "Human")
        add_row(table, signal.Type_Localised == "Human" ? "human signals" : "geological signals", std::format("{}", signal.Count));
    if(not planet.was_mapped)
      badge("unmapped before");
    if(planet.landable and not planet.was_footfalled)
      badge("no footfall before");
    for(events::atmosphere_element_t const & element: planet.atmosphere_composition)
      air += std::format("{}{} {:.0f}%", air.empty() ? "" : ", ", simple_enum::enum_name(element.Name), element.Percent);
    std::string rings;
    for(ring_t const & ring: system.rings)
      if(ring.parent_body_id == body.body_id and ring.name.ends_with("Ring"))
        {
        std::string_view kind{ring.ring_class};
        if(size_t const at{kind.rfind('_')}; at != std::string_view::npos)
          kind.remove_prefix(at + 1u);
        rings += std::format("{}{}", rings.empty() ? "" : ", ", kind);
        }
    if(not rings.empty())
      add_row(table, "rings", rings);
    }
  add_row(table, "value", std::format("{} cr", body.value));

  std::map<std::string, std::string> species;
  auto const key{std::pair{system.system_address, body.body_id}};
  if(auto const it{gathered.finds.find(key)}; it != gathered.finds.end())
    for(pictures::signed_find_t const * find: it->second)
      species.try_emplace(find->find.species, find->who);
  std::string thumbs;
  if(auto const it{gathered.samples.find(key)}; it != gathered.samples.end())
    for(shot_t const & shot: it->second)
      thumbs += std::format(
        "<a href=\"{0}\"><img loading=\"lazy\" src=\"{0}\" alt=\"{1}\" title=\"{1}\"></a>", escaped(shot.file), escaped(shot.title)
      );
  if(auto const * planet{std::get_if<planet_details_t>(&body.details)}; planet != nullptr)
    for(events::genus_t const & genus: planet->genuses_)
      if(std::ranges::none_of(species, [&](auto const & found) { return found.first.starts_with(genus.Genus_Localised); }))
        species.try_emplace(genus.Genus_Localised, "signal");
  if(not species.empty() or not thumbs.empty())
    {
    bio = "<div class=\"bio\">";
    for(auto const & [name, who]: species)
      bio += std::format("{} <span class=\"n\">{}</span><br>", escaped(name), escaped(who));
    if(not thumbs.empty())
      bio += std::format("<div class=\"thumbs\">{}</div>", thumbs);
    bio += "</div>";
    }
  return std::format(
    "<div class=\"card\" id=\"b{}\"><b>{}</b><div class=\"sub\">{}</div>{}<table>{}</table>{}{}</div>\n",
    body.body_id,
    escaped(body.name.empty() ? system.name : body.name),
    escaped(title),
    badges.empty() ? std::string{} : std::format("<div class=\"badges\">{}</div>", escaped(badges)),
    table,
    air.empty() ? std::string{} : std::format("<div class=\"air\">{}</div>", escaped(air)),
    bio
  );
  }

[[nodiscard]]
auto system_page(
  star_system_t const & system,
  std::span<info::station_t const> stations,
  gathered_t const & gathered,
  std::string const & region,
  std::string_view signature
) -> std::string
  {
  using system_index::escaped;
  auto const face_of = [&](std::string const &, std::string const & body, uint32_t) -> std::string
  {
    auto const it{gathered.faces.find(pictures::file_safe(body))};
    return it == gathered.faces.end() ? std::string{} : it->second;
  };
  auto const diagram{build_system_diagram(system, stations, {}, std::nullopt, {}, face_of, true)};

  std::vector<body_t const *> ordered;
  for(body_t const & body: system.bodies)
    if(auto const * planet{std::get_if<planet_details_t>(&body.details)}; planet == nullptr or not planet->planet_class.empty())
      ordered.push_back(&body);
  std::ranges::stable_sort(
    ordered,
    [](body_t const * a, body_t const * b)
    {
      if(a->name.empty() != b->name.empty())
        return a->name.empty();
      return natural_less(a->name, b->name);
    }
  );

  std::string page{std::format(
    "{}{} -->\n<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\"><title>{}</title><style>{}</style></head>"
    "<body>\n<div class=\"sub\"><a href=\"../sky.html\">Sky album</a> / {} / {}</div>\n<h1>{}</h1>\n"
    "<div class=\"sub\">{} bodies, coordinates {:.0f} / {:.0f} / {:.0f}</div>\n",
    mark,
    signature,
    escaped(system.name),
    system_index::page_style(),
    escaped(region),
    escaped(system_index::sector_of(system.name)),
    escaped(system.name),
    ordered.size(),
    system.system_location[0],
    system.system_location[1],
    system.system_location[2]
  )};
  if(diagram)
    page += std::format("<h2>System</h2><div class=\"map\">{}</div>\n", overlay::diagram_svg(*diagram));
  if(auto const it{gathered.sky.find(system.name)}; it != gathered.sky.end())
    {
    page += "<h2>Seen from the ship</h2><div class=\"sky\">\n";
    for(sky_shot_t const & shot: it->second)
      page += std::format(
        "<figure><a href=\"{0}\"><img loading=\"lazy\" src=\"{0}\" alt=\"{1}\"></a><figcaption>{1}{2}{3}</figcaption></figure>\n",
        escaped(shot.file),
        escaped(shot.title),
        shot.who.empty() ? std::string{} : " - " + escaped(shot.who),
        shot.first ? " <span class=\"first\">first discovery</span>" : ""
      );
    page += "</div>\n";
    }
  page += "<h2>Bodies</h2><div class=\"grid\">\n";
  for(body_t const * body: ordered)
    page += body_card(system, *body, gathered);
  page += "</div>\n</body></html>\n";
  return page;
  }

///\brief what a page is made from, as much of it as can be told without loading the system
[[nodiscard]]
auto signature_of(
  database_storage_t::album_system_t const & known, gathered_t const & gathered, std::string const & region
) -> std::string
  {
  uint64_t hash{0xcbf29ce484222325ull};
  hash = fnv(hash, std::format("{}|{}|{}|{}", page_version, known.name, known.bodies.size(), region));
  for(database_storage_t::album_body_t const & body: known.bodies)
    {
    hash = fnv(hash, body.name);
    std::string const full{body.name.empty() ? known.name : known.name + " " + body.name};
    if(auto const it{gathered.faces.find(pictures::file_safe(full))}; it != gathered.faces.end())
      hash = fnv(hash, it->second);
    if(auto const it{gathered.samples.find({known.address, body.body_id})}; it != gathered.samples.end())
      for(shot_t const & shot: it->second)
        hash = fnv(hash, shot.file);
    if(auto const it{gathered.finds.find({known.address, body.body_id})}; it != gathered.finds.end())
      for(pictures::signed_find_t const * find: it->second)
        hash = fnv(hash, find->find.species + find->who);
    }
  if(auto const it{gathered.sky.find(known.name)}; it != gathered.sky.end())
    for(sky_shot_t const & shot: it->second)
      hash = fnv(hash, shot.file);
  return std::format("{:016x}", hash);
  }

[[nodiscard]]
auto counts_of(database_storage_t::album_system_t const & known) -> std::string
  {
  system_index::body_counts_t counts;
  for(database_storage_t::album_body_t const & body: known.bodies)
    if(body.star != 0)
      ++counts["S"];
    else if(std::string_view const letter{system_index::body_letter(body.planet_class)}; not letter.empty())
      ++counts[std::string{letter}];
  return system_index::counts_text(counts);
  }
  }  // namespace

system_album_t::system_album_t(std::string db_path) :
    db_path_{std::move(db_path)},
    worker_{[this](std::stop_token stop) { run(std::move(stop)); }}
  {
  }

system_album_t::~system_album_t()
  {
  worker_.request_stop();
  wake_.notify_all();
  }

auto system_album_t::request() -> void
  {
    {
    std::lock_guard const lock{mutex_};
    requested_ = true;
    }
  wake_.notify_all();
  }

auto system_album_t::run(std::stop_token stop) -> void
  {
  // on Linux this reaches the calling thread alone - the game comes first
  setpriority(PRIO_PROCESS, 0, 15);
  while(not stop.stop_requested())
    {
      {
      std::unique_lock lock{mutex_};
      if(not wake_.wait(lock, stop, [this] { return requested_; }))
        return;
      requested_ = false;
      }
    try
      {
      pass(stop);
      }
    catch(std::exception const & failure)
      {
      spdlog::error("album: a pass over the systems failed: {}", failure.what());
      }
    catch(...)
      {
      spdlog::error("album: a pass over the systems failed");
      }
    }
  }

auto system_album_t::pass(std::stop_token const & stop) -> void
  {
  auto const started{std::chrono::steady_clock::now()};
  auto const cfg{eht::settings()};
  std::filesystem::path const own{std::filesystem::absolute(cfg->exploration.codex_dir)};
  std::error_code ec;
  std::filesystem::create_directories(own / "systems", ec);

  database_storage_t db{db_path_};
  if(auto const opened{db.open()}; not opened)
    {
    spdlog::error("album: the database could not be opened");
    return;
    }
  auto known{db.load_album_systems()};
  if(not known)
    {
    spdlog::error("album: the systems could not be read");
    return;
    }

  gathered_t const gathered{gather(own)};

  std::vector<system_index::row_t> rows;
  rows.reserve(known->size());
  size_t written{};
  for(database_storage_t::album_system_t const & system: *known)
    {
    if(stop.stop_requested())
      return;
    std::string const region{system_index::region_of(system.address)};
    std::string const signature{signature_of(system, gathered, region)};
    std::string const href{system_index::page_name(system.name)};
    bool picture{gathered.sky.contains(system.name)};
    // the star the jump arrives at: the nearest to where it drops the ship
    database_storage_t::album_body_t const * arrival{};
    for(database_storage_t::album_body_t const & body: system.bodies)
      if(body.star != 0 and (arrival == nullptr or body.distance_ls < arrival->distance_ls))
        arrival = &body;
    bool life{};
    bool first{};
    for(database_storage_t::album_body_t const & body: system.bodies)
      {
      std::pair const key{system.address, body.body_id};
      first = first or (body.was_discovered == 0 and (body.star != 0 or not body.planet_class.empty()));
      picture = picture or gathered.samples.contains(key);
      life = life or gathered.finds.contains(key) or gathered.samples.contains(key);
      }
    rows.push_back(
      system_index::row_t{
        .address = system.address,
        .name = system.name,
        .counts = counts_of(system),
        .href = href,
        .picture = picture,
        .life = life,
        .first = first,
        .star_type = arrival != nullptr ? arrival->star_type : std::string{}
      }
    );

    if(first_line(own / href) == std::format("{}{} -->", mark, signature))
      continue;
    auto loaded{db.load_system(system.address)};
    if(not loaded or not loaded->has_value())
      {
      spdlog::warn("album: {} could not be read", system.name);
      continue;
      }
    auto stations{db.load_stations(system.address)};
    std::vector<info::station_t> const none;
    system_index::write_if_changed(
      own / href, system_page(**loaded, stations ? *stations : none, gathered, region, signature)
    );
    ++written;
    // the game needs the disk and the processor more than a page does
    std::this_thread::sleep_for(std::chrono::milliseconds{2});
    }

  // the rows of this account go out as they are; what only the others know links to their pages
  system_index::write_rows(own, rows);
  std::unordered_set<uint64_t> listed;
  for(system_index::row_t const & row: rows)
    listed.insert(row.address);
  for(eht::shared_codex_t const & other: cfg->exploration.shared_codex)
    for(system_index::row_t & row: system_index::read_rows(other.dir))
      if(listed.insert(row.address).second)
        {
        row.href = pictures::seen_from(own, other.dir, row.href);
        rows.push_back(std::move(row));
        }
  std::ranges::sort(rows, [](system_index::row_t const & a, system_index::row_t const & b) { return natural_less(a.name, b.name); });
  system_index::write_if_changed(own / "sky.html", system_index::index_page(rows));

  if(written != 0u)
    spdlog::info(
      "album: {} of {} system pages written in {} s",
      written,
      known->size(),
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count()
    );
  }
