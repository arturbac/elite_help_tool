#include <eddn_publisher.h>
#include <eht_settings.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <sstream>

namespace eddn
  {
using namespace std::string_view_literals;

namespace
  {
using object_t = json_t::object_t;
using array_t = json_t::array_t;

///\brief a system's events wait at most this many for the verdict - a session begun inside a system never
/// sees its arrival star scanned, and its events are not to pile up without end
constexpr size_t max_waiting{300u};

///\brief an event older than this is the past, whatever the replay says - EDMC draws the same line
constexpr std::chrono::minutes max_age{60};

///\brief the carrier's bar file is written with the journal line; one from another moment is another stock
constexpr std::chrono::seconds max_file_discrepancy{5};

///\brief a value of the generic type - its braces pick another constructor than the one meant
template<typename value_t>
[[nodiscard]]
auto make(value_t value) -> json_t
  {
  json_t result;
  result.data = std::move(value);
  return result;
  }

[[nodiscard]]
auto object_of(json_t & value) -> object_t *
  { return value.get_if<object_t>(); }

[[nodiscard]]
auto object_of(json_t const & value) -> object_t const *
  { return value.get_if<object_t>(); }

[[nodiscard]]
auto find(json_t const & entry, std::string_view key) -> json_t const *
  {
  object_t const * const object{object_of(entry)};
  if(object == nullptr)
    return nullptr;
  auto const it{object->find(key)};
  return it == object->end() ? nullptr : &it->second;
  }

[[nodiscard]]
auto has(json_t const & entry, std::string_view key) -> bool
  { return find(entry, key) != nullptr; }

[[nodiscard]]
auto text(json_t const & entry, std::string_view key) -> std::string
  {
  json_t const * const value{find(entry, key)};
  if(value == nullptr)
    return {};
  std::string const * const s{value->get_if<std::string>()};
  return s == nullptr ? std::string{} : *s;
  }

///\brief a number whichever way glaze stored it
[[nodiscard]]
auto number(json_t const & entry, std::string_view key) -> std::optional<double>
  {
  json_t const * const value{find(entry, key)};
  if(value == nullptr)
    return std::nullopt;
  if(auto const * u{value->get_if<uint64_t>()}; u != nullptr)
    return double(*u);
  if(auto const * i{value->get_if<int64_t>()}; i != nullptr)
    return double(*i);
  if(auto const * d{value->get_if<double>()}; d != nullptr)
    return *d;
  return std::nullopt;
  }

[[nodiscard]]
auto address(json_t const & entry, std::string_view key = "SystemAddress") -> uint64_t
  {
  json_t const * const value{find(entry, key)};
  if(value == nullptr)
    return 0u;
  if(auto const * u{value->get_if<uint64_t>()}; u != nullptr)
    return *u;
  if(auto const * i{value->get_if<int64_t>()}; i != nullptr)
    return uint64_t(*i);
  if(auto const * d{value->get_if<double>()}; d != nullptr)
    return uint64_t(*d);
  return 0u;
  }

[[nodiscard]]
auto flag(json_t const & entry, std::string_view key) -> std::optional<bool>
  {
  json_t const * const value{find(entry, key)};
  if(value == nullptr)
    return std::nullopt;
  if(auto const * b{value->get_if<bool>()}; b != nullptr)
    return *b;
  return std::nullopt;
  }

auto erase(json_t & entry, std::string_view key) -> void
  {
  if(object_t * const object{object_of(entry)}; object != nullptr)
    if(auto const it{object->find(key)}; it != object->end())
      object->erase(it);
  }

auto put(json_t & entry, std::string_view key, json_t value) -> void
  {
  if(object_t * const object{object_of(entry)}; object != nullptr)
    (*object)[std::string{key}] = std::move(value);
  }

[[nodiscard]]
auto timestamp_of(json_t const & entry) -> std::optional<std::chrono::sys_seconds>
  {
  std::string const stamp{text(entry, "timestamp")};
  if(stamp.empty())
    return std::nullopt;
  std::istringstream in{stamp};
  std::chrono::sys_seconds result;
  in >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", result);
  if(in.fail())
    return std::nullopt;
  return result;
  }

[[nodiscard]]
auto listed(std::vector<std::string> const & list, std::string_view fid) -> bool
  { return not fid.empty() and std::ranges::find(list, fid) != list.end(); }

///\brief the schema each exploration event goes under - the ones not here are not sent at all
[[nodiscard]]
auto exploration_schema(std::string_view event) -> std::string_view
  {
  if(event == "FSDJump" or event == "Scan" or event == "SAASignalsFound")
    return "journal/1";
  if(event == "FSSDiscoveryScan")
    return "fssdiscoveryscan/1";
  if(event == "FSSAllBodiesFound")
    return "fssallbodiesfound/1";
  if(event == "FSSBodySignals")
    return "fssbodysignals/1";
  if(event == "ScanBaryCentre")
    return "scanbarycentre/1";
  if(event == "CodexEntry")
    return "codexentry/1";
  return {};
  }
  }  // namespace

auto filter_localised(json_t & value) -> void
  {
  if(object_t * const object{object_of(value)}; object != nullptr)
    {
    std::erase_if(*object, [](auto const & item) { return item.first.ends_with("_Localised"); });
    for(auto & [key, child]: *object)
      filter_localised(child);
    }
  else if(array_t * const array{value.get_if<array_t>()}; array != nullptr)
    for(json_t & child: *array)
      filter_localised(child);
  }

publisher_t::publisher_t(std::filesystem::path journal_dir, std::filesystem::path held_path, emit_t emit) :
    journal_dir_{std::move(journal_dir)},
    held_path_{std::move(held_path)},
    emit_{std::move(emit)}
  {
  std::ifstream in{held_path_};
  for(std::string line; std::getline(in, line);)
    if(held_t item; not line.empty() and not glz::read_json(item, line))
      held_.push_back(std::move(item));
  if(not held_.empty())
    spdlog::info("eddn: {} messages held until their systems' data is sold", held_.size());
  }

auto publisher_t::save_held() const -> void
  {
  std::filesystem::path const partial{held_path_.string() + ".partial"};
  {
  std::ofstream out{partial, std::ios::trunc};
  for(held_t const & item: held_)
    if(auto line{glz::write_json(item)}; line)
      out << *line << '\n';
  if(not out)
    {
    spdlog::error("eddn: {} could not be written", partial.string());
    return;
    }
  }
  std::error_code ec;
  std::filesystem::rename(partial, held_path_, ec);
  if(ec)
    spdlog::error("eddn: {} could not be put in place: {}", held_path_.string(), ec.message());
  }

auto publisher_t::feed(std::string_view line, bool live) -> void
  {
  json_t entry;
  if(auto const err{glz::read_json(entry, line)}; err or object_of(entry) == nullptr)
    return;
  std::string const event{text(entry, "event")};
  if(event.empty())
    return;

  learn(event, entry);

  if(backfill_)
    {
    if(not exploration_schema(event).empty())
      explore(event, std::move(entry), live);
    }
  else if(event == "FCMaterials")
    bartender(entry, live);
  else if(event == "SellExplorationData" or event == "MultiSellExplorationData")
    release(entry, live);
  else if(not exploration_schema(event).empty())
    explore(event, std::move(entry), live);
  }

auto publisher_t::learn(std::string_view event, json_t const & entry) -> void
  {
  if(event == "Fileheader" or event == "LoadGame")
    {
    // LoadGame repeats them, and where it does it is the later word
    if(std::string const version{text(entry, "gameversion")}; not version.empty())
      game_version_ = version;
    if(std::string const build{text(entry, "build")}; not build.empty())
      game_build_ = build;
    }
  if(event == "LoadGame")
    {
    horizons_ = flag(entry, "Horizons");
    odyssey_ = flag(entry, "Odyssey").value_or(false);
    if(std::string const name{text(entry, "Commander")}; not name.empty())
      commander_name_ = name;
    if(std::string const fid{text(entry, "FID")}; not fid.empty())
      commander_fid_ = fid;
    }
  else if(event == "Commander")
    {
    commander_name_ = text(entry, "Name");
    commander_fid_ = text(entry, "FID");
    }
  else if(event == "JoinACrew")
    crew_ = true;
  else if(event == "QuitACrew")
    crew_ = false;
  else if(event == "Location" or event == "FSDJump" or event == "CarrierJump")
    {
    uint64_t const here{address(entry)};
    if(here != system_address_)
      {
      verdict_ = verdict_e::unknown;
      waiting_.clear();
      seen_.clear();
      }
    system_address_ = here;
    system_name_ = text(entry, "StarSystem");
    if(json_t const * const pos{find(entry, "StarPos")}; pos != nullptr)
      star_pos_ = *pos;
    else
      star_pos_.reset();
    // somebody lives here - the one kind of system that is never sent, whoever discovered it
    if(number(entry, "Population").value_or(0.0) > 0.0 or has(entry, "SystemFaction"))
      verdict_ = verdict_e::skip;
    }
  }

auto publisher_t::explore(std::string_view event, json_t entry, bool live) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->eddn.enabled or crew_ or not listed(cfg->eddn.exploration_commanders, commander_fid_))
    return;
  if(backfill_)
    {
    if(commander_fid_ != only_fid_ or std::ranges::find(wanted_, system_name_) == wanted_.end())
      return;
    }
  else if(auto const at{timestamp_of(entry)};
          not live or not at or std::chrono::system_clock::now() - *at > max_age)
    return;

  // the arrival star says whether anybody was here before
  bool decided{};
  if(
    event == "Scan" and verdict_ == verdict_e::unknown and address(entry) == system_address_
    and has(entry, "StarType") and number(entry, "DistanceFromArrivalLS").value_or(1.0) == 0.0
  )
    {
    verdict_ = flag(entry, "WasDiscovered").value_or(true) ? verdict_e::skip : verdict_e::send;
    decided = true;
    }
  if(verdict_ == verdict_e::skip)
    {
    waiting_.clear();
    return;
    }

  std::string const schema{exploration_schema(event)};
  // an event of another system is a late one - its place is not ours to add
  if(address(entry) != 0u and address(entry) != system_address_)
    return;

  if(schema == "journal/1")
    {
    for(std::string_view key:
        {"ActiveFine", "CockpitBreach", "BoostUsed", "FuelLevel", "FuelUsed", "JumpDist", "Latitude", "Longitude", "Wanted"})
      erase(entry, key);
    if(object_t * const object{object_of(entry)}; object != nullptr)
      if(auto const it{object->find("Factions")}; it != object->end())
        if(array_t * const factions{it->second.get_if<array_t>()}; factions != nullptr)
          for(json_t & faction: *factions)
            for(std::string_view key: {"HappiestSystem", "HomeSystem", "MyReputation", "SquadronFaction"})
              erase(faction, key);
    if(address(entry) == 0u)
      return;
    }
  else if(schema == "fssdiscoveryscan/1")
    erase(entry, "Progress");
  else if(schema == "codexentry/1")
    {
    erase(entry, "IsNewEntry");
    erase(entry, "NewTraitsDiscovered");
    for(std::string_view key: {"System", "Name", "Region", "Category", "SubCategory"})
      if(text(entry, key).empty())
        return;
    }

  if(not augment(entry))
    return;
  filter_localised(entry);

  // the events that waited for the verdict go first, in the order they came
  if(decided and verdict_ == verdict_e::send)
    {
    for(auto & [waiting_schema, waiting_entry]: waiting_)
      hold(waiting_schema, std::move(waiting_entry));
    waiting_.clear();
    }

  if(verdict_ == verdict_e::send)
    hold(schema, std::move(entry));
  else if(waiting_.size() < max_waiting)
    waiting_.emplace_back(schema, std::move(entry));
  }

auto publisher_t::augment(json_t & entry) const -> bool
  {
  if(not has(entry, "StarSystem") and not has(entry, "SystemName") and not has(entry, "System"))
    {
    if(system_name_.empty())
      return false;
    put(entry, "StarSystem", make(system_name_));
    }
  if(not has(entry, "SystemAddress"))
    {
    if(system_address_ == 0u)
      return false;
    put(entry, "SystemAddress", make(system_address_));
    }
  if(not has(entry, "StarPos"))
    {
    if(not star_pos_)
      return false;
    put(entry, "StarPos", *star_pos_);
    }
  return true;
  }

auto publisher_t::bartender(json_t const & entry, bool live) -> void
  {
  auto const cfg{eht::settings()};
  if(not live or not cfg->eddn.enabled or crew_ or not listed(cfg->eddn.bartender_commanders, commander_fid_))
    return;
  auto const at{timestamp_of(entry)};
  if(not at or std::chrono::system_clock::now() - *at > max_age)
    return;

  // the journal line only says the bar was looked at - the stock is in the file beside it
  json_t stock;
  std::string buffer;
  if(auto const err{glz::read_file_json(stock, (journal_dir_ / "FCMaterials.json").string(), buffer)};
     err or object_of(stock) == nullptr)
    {
    spdlog::warn("eddn: FCMaterials.json could not be read");
    return;
    }
  auto const written{timestamp_of(stock)};
  if(not written or (*written > *at ? *written - *at : *at - *written) > max_file_discrepancy)
    {
    spdlog::warn("eddn: FCMaterials.json is from another moment than the journal line");
    return;
    }
  json_t const * const items{find(stock, "Items")};
  if(items == nullptr)
    return;

  uint64_t const market{address(stock, "MarketID")};
  std::string const items_text{items->dump().value_or(std::string{})};
  if(market == last_market_id_ and items_text == last_items_)
    return;
  last_market_id_ = market;
  last_items_ = items_text;

  filter_localised(stock);
  if(auto message{envelope("fcmaterials_journal/1", std::move(stock))}; message)
    emit_(std::move(*message));
  }

auto publisher_t::hold(std::string_view schema, json_t message) -> void
  {
  // the same event written again is the same message - once is enough
  if(auto const text_of{message.dump()}; not text_of or not seen_.insert(*text_of).second)
    return;
  auto made{envelope(schema, std::move(message))};
  if(not made)
    return;
  if(backfill_)
    {
    emit_(std::move(*made));
    return;
    }
  held_t item{.fid = commander_fid_, .system = system_name_, .schema = std::move(made->schema), .envelope = std::move(made->envelope)};
  // appended as it comes, so a crash loses nothing that was already scanned
  if(std::ofstream out{held_path_, std::ios::app}; out)
    if(auto line{glz::write_json(item)}; line)
      out << *line << '\n';
  held_.push_back(std::move(item));
  }

auto publisher_t::release(json_t const & sale, bool live) -> void
  {
  auto const cfg{eht::settings()};
  // nothing held is no reason to stop - the systems may still be in the journals
  if(not live or not cfg->eddn.enabled)
    return;

  // a single sale names its systems, a multiple one lists them with their bodies
  std::vector<std::string> sold;
  if(json_t const * const systems{find(sale, "Systems")}; systems != nullptr)
    if(array_t const * const list{systems->get_if<array_t>()}; list != nullptr)
      for(json_t const & name: *list)
        if(std::string const * const s{name.get_if<std::string>()}; s != nullptr)
          sold.push_back(*s);
  if(json_t const * const discovered{find(sale, "Discovered")}; discovered != nullptr)
    if(array_t const * const list{discovered->get_if<array_t>()}; list != nullptr)
      for(json_t const & item: *list)
        if(std::string const name{text(item, "SystemName")}; not name.empty())
          sold.push_back(name);

  if(not listed(cfg->eddn.exploration_commanders, commander_fid_))
    return;

  size_t released{};
  std::vector<std::string> found;
  std::erase_if(
    held_,
    [&](held_t & item)
    {
      if(item.fid != commander_fid_ or std::ranges::find(sold, item.system) == sold.end())
        return false;
      if(std::ranges::find(found, item.system) == found.end())
        found.push_back(item.system);
      emit_(message_t{.schema = std::move(item.schema), .envelope = std::move(item.envelope)});
      ++released;
      return true;
    }
  );
  if(released != 0u)
    {
    spdlog::info("eddn: {} held messages of {} sold systems released", released, found.size());
    save_held();
    }

  std::vector<std::string> missing;
  for(std::string const & name: sold)
    if(std::ranges::find(found, name) == found.end() and std::ranges::find(missing, name) == missing.end())
      missing.push_back(name);
  if(not missing.empty())
    backfill(missing);
  }

auto publisher_t::backfill(std::vector<std::string> const & systems) -> void
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir_, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals);
  // a month or two of sessions - scans older than that were sold long ago, or never will be
  constexpr size_t reach{600u};
  if(journals.size() > reach)
    journals.erase(journals.begin(), journals.end() - std::ptrdiff_t{reach});

  size_t sent{};
  publisher_t replay{journal_dir_, {}, [&](message_t && message) { emit_(std::move(message)); ++sent; }};
  replay.backfill_ = true;
  replay.wanted_ = systems;
  replay.only_fid_ = commander_fid_;

  // only what teaches the state or names a wanted system is read whole - the rest of the archive is skipped
  constexpr std::array state_events{
    R"("event":"Fileheader")"sv, R"("event":"LoadGame")"sv, R"("event":"Commander")"sv, R"("event":"Location")"sv,
    R"("event":"FSDJump")"sv, R"("event":"CarrierJump")"sv, R"("event":"JoinACrew")"sv, R"("event":"QuitACrew")"sv
  };
  for(std::filesystem::path const & path: journals)
    {
    std::ifstream in{path, std::ios::binary};
    for(std::string line; std::getline(in, line);)
      {
      bool const wanted{
        std::ranges::any_of(systems, [&](std::string const & name) { return line.contains(name); })
        or std::ranges::any_of(state_events, [&](std::string_view key) { return line.contains(key); })
      };
      if(wanted)
        replay.feed(line, false);
      }
    }
  spdlog::info("eddn: {} messages of {} sold systems found in the journals and sent", sent, systems.size());
  }

auto publisher_t::envelope(std::string_view schema, json_t message) const -> std::optional<message_t>
  {
  auto const cfg{eht::settings()};
  if(horizons_)
    put(message, "horizons", make(*horizons_));
  put(message, "odyssey", make(odyssey_));

  std::string version_lower{game_version_};
  std::ranges::transform(version_lower, version_lower.begin(), [](char c) { return char(std::tolower(c)); });
  bool const test{cfg->eddn.test or version_lower.contains("alpha") or version_lower.contains("beta")};

  json_t whole{make(object_t{})};
  put(whole, "$schemaRef", make(std::format("https://eddn.edcd.io/schemas/{}{}", schema, test ? "/test" : "")));
  json_t header{make(object_t{})};
  put(header, "uploaderID", make(commander_name_));
  put(header, "softwareName", make(std::string{software_name}));
  put(header, "softwareVersion", make(std::string{software_version}));
  put(header, "gameversion", make(game_version_));
  put(header, "gamebuild", make(game_build_));
  put(whole, "header", std::move(header));
  put(whole, "message", std::move(message));

  auto text_out{whole.dump()};
  if(not text_out)
    {
    spdlog::error("eddn: a {} message could not be written", schema);
    return std::nullopt;
    }
  return message_t{.schema = std::format("{}{}", schema, test ? "/test" : ""), .envelope = std::move(*text_out)};
  }
  }  // namespace eddn
