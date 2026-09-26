#include <overlay_feed.h>
#include <qformat.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <functional>
#include <cctype>
#include <format>
#include <map>
#include <set>
#include <ranges>

namespace
  {
constexpr uint32_t colour_heading{0x9ad1ffu};
constexpr uint32_t colour_plain{0xddddddu};
constexpr uint32_t colour_alert{0xd9a34au};
///\brief undiscovered by anyone - that is the case worth stopping for
constexpr uint32_t colour_first{0x3cb371u};
constexpr uint32_t colour_expiring{0xd9534fu};

///\brief the blocks go out when the tool falls silent - better no text than text from an hour ago
constexpr uint32_t block_ttl_ms{10000u};
///\brief an unchanged picture has to be repeated anyway, or it expires on a player standing still
constexpr std::chrono::seconds heartbeat{3};

///\brief below this threshold going down to a body does not pay for the time it takes
constexpr uint32_t minimum_body_value{300000u};
///\brief the side band has its limits, and a long list will not be read in flight anyway
constexpr size_t listed_bodies{5u};
constexpr size_t listed_factions{5u};
constexpr size_t listed_missions{6u};
constexpr size_t listed_cargo{4u};
constexpr size_t listed_commodities{3u};
///\brief below this much time in hand a mission is a problem, not a plan
constexpr std::chrono::hours expiry_warning{3};
///\brief smaller departures from the galactic average are not worth showing
constexpr double interesting_deviation{0.25};
///\brief a percentage without an amount lies - 93% off a commodity worth 20 Cr saves nothing that matters
constexpr uint32_t interesting_margin{500u};
///\brief influence updates once a day; asking more often serves nothing
constexpr std::chrono::seconds faction_refresh{60};
///\brief a market can appear at any moment, as soon as the player opens it
constexpr std::chrono::seconds market_refresh{5};
///\brief missions arrive rarely, and the query goes across both databases
constexpr std::chrono::seconds supply_refresh{10};
constexpr size_t listed_sources{2u};
///\brief the band is not a mission log - beyond this the list stops being read at a glance
constexpr size_t listed_settlement_work{4u};
constexpr unsigned listed_trades{3u};

///\brief a port in space carries the most goods, a settlement the least - that is the order of worth
[[nodiscard]]
auto station_rank(std::string_view station_type) -> int
  {
  using namespace std::string_view_literals;
  constexpr std::array space{
    "Coriolis"sv,
    "Orbis"sv,
    "Ocellus"sv,
    "Dodec"sv,
    "AsteroidBase"sv,
    "MegaShip"sv,
    "Outpost"sv,
    "SpaceConstructionDepot"sv
  };
  constexpr std::array planetary{
    "CraterPort"sv, "CraterOutpost"sv, "SurfaceStation"sv, "PlanetaryConstructionDepot"sv, "DockablePlanetStation"sv
  };

  if(std::ranges::contains(space, station_type))
    return 0;
  if(std::ranges::contains(planetary, station_type))
    return 1;
  if(station_type == "OnFootSettlement")
    return 2;
  return 3;
  }

///\brief the same colours as in the reputation window - red federation, blue empire, green alliance
[[nodiscard]]
auto allegiance_colour(info::allegiance_e allegiance) -> uint32_t
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation: return 0xd9534fu;
    case empire:     return 0x4a90d9u;
    case alliance:   return 0x3cb371u;
    default:         return colour_plain;
    }
  }

///\brief the superpower's emblem, for the allegiances that have one
///\detail an independent faction answers to nobody, so it gets no emblem and the space stays empty -
/// a placeholder there would say something the game does not
[[nodiscard]]
auto allegiance_emblem(info::allegiance_e allegiance) -> overlay::emblem_e
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation: return overlay::emblem_e::federation;
    case empire:     return overlay::emblem_e::empire;
    case alliance:   return overlay::emblem_e::alliance;
    default:         return overlay::emblem_e::none;
    }
  }

///\brief separates factions that share an allegiance, without losing what the colour says
///\detail allegiance decides the hue, so four independents come out as one grey mass and their lines
/// cannot be followed. The hue stays - it is the part that says Federation or Empire - and only the
/// brightness steps down, far enough to tell two lines apart and not so far that the text stops
/// reading against the dark band
[[nodiscard]]
auto shade(uint32_t rgb, size_t step) -> uint32_t
  {
  constexpr std::array steps{1.0, 0.82, 0.68, 0.58, 0.50};
  double const factor{steps[std::min(step, steps.size() - 1u)]};

  auto const channel = [factor](uint32_t value) -> uint32_t
  { return static_cast<uint32_t>(std::lround(static_cast<double>(value) * factor)) & 0xffu; };

  return (channel((rgb >> 16u) & 0xffu) << 16u) | (channel((rgb >> 8u) & 0xffu) << 8u) | channel(rgb & 0xffu);
  }

[[nodiscard]]
auto same_content(overlay::frame_t const & left, overlay::frame_t const & right) -> bool
  {
  if(left.blocks.size() != right.blocks.size())
    return false;

  for(size_t block{}; block != left.blocks.size(); ++block)
    {
    if(
      left.blocks[block].corner != right.blocks[block].corner
      or left.blocks[block].lines.size() != right.blocks[block].lines.size()
    )
      return false;

    for(size_t line{}; line != left.blocks[block].lines.size(); ++line)
      if(left.blocks[block].lines[line].text != right.blocks[block].lines[line].text)
        return false;

    // the chart carries no text, so without comparing it a tick could pass without the picture
    // ever being resent - the lines would keep saying the same while the curves had moved
    auto const & lc{left.blocks[block].charts};
    auto const & rc{right.blocks[block].charts};
    if(lc.size() != rc.size())
      return false;

    for(size_t chart{}; chart != lc.size(); ++chart)
      {
      if(lc[chart].caption != rc[chart].caption or lc[chart].series.size() != rc[chart].series.size())
        return false;

      for(size_t series{}; series != lc[chart].series.size(); ++series)
        {
        overlay::series_t const & ls{lc[chart].series[series]};
        overlay::series_t const & rs{rc[chart].series[series]};
        if(ls.name != rs.name or ls.points.size() != rs.points.size())
          return false;
        }
      }
    }
  return true;
  }

///\brief the game repeats the system's name inside a body's name - in a side band that is pure waste of room
[[nodiscard]]
auto short_body_name(std::string const & system_name, std::string const & body_name) -> std::string
  {
  if(body_name.size() > system_name.size() + 1u and body_name.starts_with(system_name))
    return body_name.substr(system_name.size() + 1u);
  return body_name;
  }

///\brief worth going down to only what we have not mapped yet and what pays something
[[nodiscard]]
auto worth_mapping(body_t const & body) -> bool
  {
  auto const * const planet{std::get_if<planet_details_t>(&body.details)};
  return planet != nullptr and not planet->mapped and body.value >= minimum_body_value;
  }

[[nodiscard]]
auto describe_exploration(star_system_t const & system) -> std::vector<overlay::line_t>
  {
  std::vector<body_t const *> candidates;
  for(body_t const & body: system.bodies)
    if(worth_mapping(body))
      candidates.push_back(&body);

  if(candidates.empty())
    return {};

  std::ranges::sort(candidates, std::ranges::greater{}, [](body_t const * body) { return body->value; });

  uint64_t total{};
  for(body_t const * body: candidates)
    total += body->value;

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = std::format("worth mapping: {} bodies, {} Cr", candidates.size(), format_credits_value(uint32_t(total))),
      .color = colour_heading
    }
  );

  for(body_t const * body: candidates | std::views::take(listed_bodies))
    {
    auto const * const planet{std::get_if<planet_details_t>(&body->details)};
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}  {} Cr  {:.0f} ls{}",
          short_body_name(system.name, body->name),
          format_credits_value(body->value),
          body->distance_from_arrival_ls,
          planet != nullptr and planet->landable ? "  landable" : ""
        ),
        // a first discovery is a bonus that cannot be had again later
        .color = body->was_discovered ? colour_plain : colour_first
      }
    );
    }

  if(candidates.size() > listed_bodies)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", candidates.size() - listed_bodies), .color = colour_plain}
    );

  return lines;
  }

///\brief the internal name is readable but ugly - a capital letter is enough where there is no translation
[[nodiscard]]
auto readable_name(events::cargo_item_t const & item) -> std::string
  {
  if(not item.Name_Localised.empty())
    return item.Name_Localised;

  std::string name{item.Name};
  if(not name.empty())
    name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
  return name;
  }

///\brief cargo left on board blocks calling the ship down to a settlement's pad
[[nodiscard]]
auto describe_cargo(events::cargo_file_t const & cargo) -> std::vector<overlay::line_t>
  {
  if(cargo.timestamp == std::chrono::sys_seconds{})
    return {};

  std::vector<overlay::line_t> lines;

  if(cargo.Count == 0u)
    {
    lines.push_back(overlay::line_t{.text = "cargo: empty", .color = colour_plain});
    return lines;
    }

  lines.push_back(overlay::line_t{.text = std::format("cargo: {} t", cargo.Count), .color = colour_alert});

  auto sorted{cargo.Inventory};
  std::ranges::sort(sorted, std::ranges::greater{}, &events::cargo_item_t::Count);

  for(events::cargo_item_t const & item: sorted | std::views::take(listed_cargo))
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {}  {} t{}",
          readable_name(item),
          item.Count,
          item.Stolen != 0u ? std::format("  {} stolen", item.Stolen) : ""
        ),
        .color = item.Stolen != 0u ? colour_expiring : colour_plain
      }
    );

  if(sorted.size() > listed_cargo)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", sorted.size() - listed_cargo), .color = colour_plain}
    );

  return lines;
  }

///\brief less than an hour left is a different kind of news than a few days left
[[nodiscard]]
auto format_remaining(std::chrono::seconds left) -> std::string
  {
  if(left <= std::chrono::seconds::zero())
    return "expired";

  auto const days{std::chrono::duration_cast<std::chrono::days>(left)};
  auto const hours{std::chrono::duration_cast<std::chrono::hours>(left - days)};
  if(days.count() != 0)
    return std::format("{}d {}h", days.count(), hours.count());

  auto const minutes{std::chrono::duration_cast<std::chrono::minutes>(left - hours)};
  if(hours.count() != 0)
    return std::format("{}h {}m", hours.count(), minutes.count());

  return std::format("{}m", minutes.count());
  }

///\brief a redirected mission is done and only waits to be handed in - a different category from the rest
[[nodiscard]]
auto describe_missions(std::vector<info::mission_t> const & missions) -> std::vector<overlay::line_t>
  {
  std::vector<info::mission_t const *> open;
  for(info::mission_t const & mission: missions)
    if(mission.status == info::mission_status_e::accepted or mission.status == info::mission_status_e::redirected)
      open.push_back(&mission);

  if(open.empty())
    return {};

  std::ranges::sort(open, {}, [](info::mission_t const * mission) { return mission->expiry; });

  auto const ready{std::ranges::count_if(
    open, [](info::mission_t const * mission) { return mission->status == info::mission_status_e::redirected; }
  )};

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;
  lines.push_back(
    overlay::line_t{
      .text = ready == 0 ? std::format("missions: {} open", open.size())
                         : std::format("missions: {} open, {} to hand in", open.size(), ready),
      .color = colour_heading
    }
  );

  for(info::mission_t const * mission: open | std::views::take(listed_missions))
    {
    auto const left{std::chrono::duration_cast<std::chrono::seconds>(mission->expiry - now)};
    bool const done{mission->status == info::mission_status_e::redirected};

    auto const where{
      done ? (mission->redirected_station.empty() ? mission->redirected_system : mission->redirected_station)
           : (mission->destination_station.empty() ? mission->destination_system : mission->destination_station)
    };

    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{}{}  {}{}{}",
          done ? "> " : "  ",
          mission->faction,
          where.empty() ? std::string{"-"} : where,
          "  ",
          format_remaining(left)
        ),
        // green is ready to hand in, red is about to be lost
        .color = left < expiry_warning ? colour_expiring : (done ? colour_first : colour_plain)
      }
    );
    }

  if(open.size() > listed_missions)
    lines.push_back(
      overlay::line_t{.text = std::format("... and {} more", open.size() - listed_missions), .color = colour_plain}
    );

  return lines;
  }

[[nodiscard]]
auto describe_system(star_system_t const & system, bool with_controlling) -> std::vector<overlay::line_t>
  {
  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = system.name, .color = colour_heading});

  // with influence at hand the controlling faction is marked there with a star, so repeating it serves nothing
  if(with_controlling and not system.controlling_faction.empty())
    lines.push_back(overlay::line_t{.text = system.controlling_faction, .color = colour_plain});

  if(not system.economy.empty() or not system.government.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "{} / {}", system.economy.empty() ? "?" : system.economy, system.government.empty() ? "?" : system.government
        ),
        .color = colour_plain
      }
    );

  if(not system.security.empty())
    lines.push_back(overlay::line_t{.text = std::format("security: {}", system.security), .color = colour_plain});

  // a system with the FSS unfinished is a reason to stay, not to fly on
  if(not system.fss_complete and not system.bodies.empty())
    lines.push_back(
      overlay::line_t{
        .text = std::format("FSS incomplete, {} bodies known", system.bodies.size()), .color = colour_alert
      }
    );

  return lines;
  }

///\brief how far back the influence chart reaches
///\detail ten days left the days far apart in the band's width; twenty fit the same rectangle and
/// carry more of the story - a faction's climb usually takes longer than a week to show as a climb
constexpr std::chrono::days chart_window{20};
///\brief the height of the plot itself, without the caption and the legend
constexpr uint32_t chart_height{130u};
///\brief the shapes handed out in the order of the legend
///\detail three independent factions share one colour, so without distinct shapes their lines
/// could not be told apart
constexpr std::array chart_markers{
  overlay::marker_e::circle,
  overlay::marker_e::diamond,
  overlay::marker_e::triangle,
  overlay::marker_e::square,
  overlay::marker_e::cross
};

///\brief a faction as the chart needs it - who it is and how it is drawn, nothing more
struct charted_t
  {
  int64_t oid{-1};
  std::string name;
  uint32_t color{};
  overlay::marker_e marker{};
  };

///\brief the ten day influence chart, with the scale already applied
///
/// The layer receives nothing but numbers in 0..1 - the logarithm, the decades and the window all
/// happen here. A logarithmic scale is what makes the chart useful at all: a faction sitting at 2%
/// and one at 45% move by comparable fractions of themselves, and on a linear axis the small one

/// lies flat against the bottom.
[[nodiscard]]
auto build_influence_chart(
  std::span<info::faction_influence_t const> history, std::span<charted_t const> shown, std::chrono::sys_seconds now
) -> std::vector<overlay::chart_t>
  {
  using std::chrono::sys_seconds;

  sys_seconds const from{now - chart_window};
  double const span{static_cast<double>((now - from).count())};
  if(span <= 0.0)
    return {};

  struct sample_t
    {
    double x{};
    double percent{};
    };

  std::vector<std::vector<sample_t>> gathered(shown.size());
  double lowest{std::numeric_limits<double>::max()};
  double highest{};

  for(size_t ix{}; ix != shown.size(); ++ix)
    {
    // influence is stored only when it changes, so a faction quiet for ten days has no row inside
    // the window at all - the last value before it is what the line has to start from
    std::optional<double> seed;
    std::vector<sample_t> & points{gathered[ix]};

    for(info::faction_influence_t const & entry: history)
      {
      if(entry.faction_oid != shown[ix].oid)
        continue;

      double const percent{entry.influence * 100.0};
      if(entry.timestamp <= from)
        seed = percent;
      else if(entry.timestamp <= now)
        points.push_back(
          sample_t{.x = static_cast<double>((entry.timestamp - from).count()) / span, .percent = percent}
        );
      }

    if(seed)
      points.insert(points.begin(), sample_t{.x = 0.0, .percent = *seed});

    // the value holds until the next tick, so the line runs on to the right edge instead of
    // stopping wherever the last change happened to fall
    if(not points.empty() and points.back().x < 1.0)
      points.push_back(sample_t{.x = 1.0, .percent = points.back().percent});

    for(sample_t const & point: points)
      if(point.percent > 0.0)
        {
        lowest = std::min(lowest, point.percent);
        highest = std::max(highest, point.percent);
        }
    }

  if(highest <= 0.0 or std::ranges::all_of(gathered, [](auto const & p) { return p.size() < 2u; }))
    return {};

  // Whole decades give a readable grid, and the floor is one per cent because that is the floor in
  // the game: a faction present in a system holds at least a point of it, and below that it is not
  // weakened but gone - it retreats and stops being present at all. A decade under that would be an
  // empty quarter of the chart squeezing the part where the fight actually happens
  double const low{std::max(1.0, std::pow(10.0, std::floor(std::log10(lowest))))};
  double const high{std::max(low * 10.0, std::pow(10.0, std::ceil(std::log10(highest))))};
  double const log_low{std::log10(low)};
  double const log_span{std::log10(high) - log_low};

  auto const map_y = [&](double percent) -> float
  { return static_cast<float>((std::log10(std::clamp(percent, low, high)) - log_low) / log_span); };

  overlay::chart_t chart{
    .caption = std::format("influence, {} days, log scale", chart_window.count()), .height = chart_height
  };

  for(double decade{low}; decade <= high * 1.0001; decade *= 10.0)
    chart.grid.push_back(
      overlay::grid_line_t{
        .y = map_y(decade), .label = decade >= 1.0 ? std::format("{:.0f}%", decade) : std::format("{:.1f}%", decade)
      }
    );

  for(size_t ix{}; ix != shown.size(); ++ix)
    {
    if(gathered[ix].size() < 2u)
      continue;

    overlay::series_t series{
      .name = shown[ix].name, .color = shown[ix].color,
      .marker = shown[ix].marker
    };
    series.points.reserve(gathered[ix].size());
    for(sample_t const & point: gathered[ix])
      series.points.push_back(overlay::point_t{.x = static_cast<float>(point.x), .y = map_y(point.percent)});

    chart.series.push_back(std::move(series));
    }

  if(chart.series.empty())
    return {};

  return {std::move(chart)};
  }
  }  // namespace

overlay_feed_t::overlay_feed_t(std::string socket_path, std::string db_path) :
    server_{std::make_unique<overlay::server_t>(std::move(socket_path))},
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("overlay feed: failed to open {}", db_path);

  if(server_->listening())
    spdlog::info("overlay feed listening on {}", server_->path());
  else
    // the commonest cause is a path longer than 107 characters - sockaddr_un has nowhere to put it
    spdlog::warn(
      "overlay feed could not listen on {} ({} chars), in-game overlay will stay empty",
      server_->path(),
      server_->path().size()
    );
  }

[[nodiscard]]
auto overlay_feed_t::listening() const noexcept -> bool
  { return server_->listening(); }

[[nodiscard]]
auto overlay_feed_t::clients() const noexcept -> unsigned
  { return server_->clients(); }

auto overlay_feed_t::refresh_factions(current_state_t const & state) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  bool const same_system{state.current_system_address_ == factions_system_};

  // influence moves once a day, so asking the database every frame would be a waste
  if(same_system and now - factions_loaded_ < faction_refresh)
    return;

  factions_system_ = state.current_system_address_;
  factions_loaded_ = now;
  faction_lines_.clear();
  conflict_lines_.clear();
  faction_charts_.clear();

  if(factions_system_ == 0u)
    return;

  // one line fits in the side band, so what is left is the hour alone and whether the wave has reached
  // here - the spread across systems and the statistics are visible in the system window
  auto const wall_clock{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto const tick_line = [&](info::tick_kind_e kind, std::string_view caption) -> overlay::line_t
  {
    tick_view_t const view{describe_tick(db_, factions_system_, kind, wall_clock)};
    return overlay::line_t{
      .text = std::format("{} {}{}", caption, view.here, view.awaiting ? "  (wave started, not here yet)" : ""),
      .color = view.awaiting ? colour_alert : colour_plain
    };
  };

  faction_lines_.push_back(tick_line(info::tick_kind_e::influence, "BGS tick"));

  if(auto conflicts{db_.load_conflicts(factions_system_)}; conflicts)
    {
    // the database holds the whole history of rows, and the screen is to show each pair's present state
    std::map<std::pair<std::string, std::string>, info::conflict_t const *> latest_conflict;
    for(info::conflict_t const & conflict: *conflicts)
      {
      auto & slot{latest_conflict[{conflict.faction1, conflict.faction2}]};
      if(slot == nullptr or slot->timestamp < conflict.timestamp)
        slot = &conflict;
      }

    for(auto const & [pair, entry]: latest_conflict)
      {
      info::conflict_t const & conflict{*entry};
      // the finished ones and the merely announced ones change nothing about what to do now
      if(conflict.status != "active")
        continue;

      conflict_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "{}: {} {} - {} {}",
            conflict.war_type,
            conflict.faction1,
            conflict.won_days1,
            conflict.won_days2,
            conflict.faction2
          ),
          .color = colour_alert
        }
      );

      if(not conflict.stake1.empty() or not conflict.stake2.empty())
        conflict_lines_.push_back(
          overlay::line_t{
            .text = std::format(
              "  stake: {} / {}",
              conflict.stake1.empty() ? "-" : conflict.stake1,
              conflict.stake2.empty() ? "-" : conflict.stake2
            ),
            .color = colour_plain
          }
        );
      }
    }

  // the war clock is shown only while something is running - it is what bonds are sold by
  if(not conflict_lines_.empty())
    {
    // a conflict is settled at the fourth day won, so the end can be counted down without knowing when
    // the recalculation falls - and that is exactly when bonds are worth the most
    if(auto countdown{db_.load_war_countdown(factions_system_)}; countdown)
      for(info::war_countdown_t const & war: *countdown)
        {
        if(not war.active)
          continue;

        bool const decides_now{war.ticks_left == 0u};
        conflict_lines_.insert(
          conflict_lines_.begin(),
          overlay::line_t{
            .text = decides_now ? std::format("{}: decided at the next tick - have bonds ready", war.war_type)
                                : std::format("{}: {} more war ticks", war.war_type, war.ticks_left),
            .color = decides_now ? colour_alert : colour_plain
          }
        );
        }

    conflict_lines_.insert(conflict_lines_.begin(), tick_line(info::tick_kind_e::war, "war tick"));
    }

  auto history{db_.load_influence_history(factions_system_)};
  if(not history)
    {
    spdlog::error("overlay feed: failed to load influence for {}", factions_system_);
    return;
    }

  // a faction thrown out of the system stops appearing in the readings, but its last influence row stays
  // behind - which is why the list is narrowed to the ones seen at the newest reading
  std::set<int64_t> present;
  if(auto refs{db_.load_present_factions(factions_system_)}; refs)
    for(info::faction_ref_t const & ref: *refs)
      present.insert(ref.faction_oid);

  // each faction's last row is its present state in the system
  std::map<int64_t, info::faction_influence_t const *> latest;
  for(info::faction_influence_t const & entry: *history)
    {
    // an empty set means we have no trace of presence for this system yet - then everything is shown
    if(not present.empty() and not present.contains(entry.faction_oid))
      continue;
    latest[entry.faction_oid] = &entry;
    }

  struct presence_t
    {
    int64_t oid;
    overlay::trend_e trend{overlay::trend_e::unknown};
    std::string name;
    info::allegiance_e allegiance;
    std::string active;
    double influence;
    };

  std::vector<presence_t> presence;
  presence.reserve(latest.size());
  for(auto const & [oid, entry]: latest)
    {
    presence_t item{
      .oid = oid,
      .name = {},
      .allegiance = info::allegiance_e::unknown,
      .active = not entry->active_states.empty() ? entry->active_states
                : entry->faction_state == "None" ? std::string{}
                                                 : entry->faction_state,
      .influence = entry->influence
    };

    if(
      auto it{std::ranges::find(state.known_factions, oid, &info::faction_info_t::oid)};
      it != state.known_factions.end()
    )
      {
      item.name = it->name;
      item.allegiance = it->allegiance;
      }

    if(not item.name.empty())
      presence.emplace_back(std::move(item));
    }

  // Which way each faction went at the last recalculation, rather than at its last change - those
  // are different questions. A faction that did not move writes no row, so silence after a wave we
  // have actually seen means it held its ground; silence because we have not been here since the
  // wave means we simply do not know, and the two must not look the same.
  if(auto waves{db_.load_recent_ticks(info::tick_kind_e::influence, 30u)}; waves and not waves->empty())
    {
    std::chrono::sys_seconds const wave{waves->front().start_begin};
    bool const seen_since{
      std::ranges::any_of(*history, [wave](info::faction_influence_t const & e) { return e.timestamp >= wave; })
    };

    if(seen_since)
      for(presence_t & item: presence)
        {
        std::optional<double> before;
        std::optional<double> after;
        for(info::faction_influence_t const & entry: *history)
          if(entry.faction_oid == item.oid)
            {
            if(entry.timestamp < wave)
              before = entry.influence;
            else
              after = entry.influence;
            }

        if(not before)
          continue;
        if(not after)
          {
          item.trend = overlay::trend_e::flat;
          continue;
          }

        item.trend = *after > *before  ? overlay::trend_e::up
                     : *after < *before ? overlay::trend_e::down
                                        : overlay::trend_e::flat;
        }
    }

  std::ranges::sort(presence, std::ranges::greater{}, &presence_t::influence);

  // the line and the chart series are given the same colour and the same shape in one place, so
  // there is no way for them to drift apart
  std::vector<charted_t> charted;
  std::map<uint32_t, size_t> shades_used;
  size_t marker_ix{};
  for(presence_t const & item: presence | std::views::take(listed_factions))
    {
    uint32_t const base{allegiance_colour(item.allegiance)};
    uint32_t const colour{shade(base, shades_used[base]++)};
    overlay::marker_e const marker{chart_markers[marker_ix % chart_markers.size()]};
    ++marker_ix;

    charted.push_back(charted_t{.oid = item.oid, .name = item.name, .color = colour, .marker = marker});

    faction_lines_.push_back(
      overlay::line_t{
        // the star marks the controlling faction, because that one decides the system's face
        .text = std::format(
          "{}{}  {:.1f}%",
          item.name == state.system.controlling_faction ? "* " : "  ",
          item.name,
          item.influence * 100.0
        ),
        .color = colour,
        // the same shape the faction's line wears on the chart below, so the two read as one
        .marker = marker,
        .emblem = allegiance_emblem(item.allegiance),
        // the mark goes between the value and the states, because it speaks about the value
        .trend = item.trend,
        .suffix = item.active
      }
    );
    }

  faction_charts_ = build_influence_chart(*history, charted, wall_clock);
  }

auto overlay_feed_t::refresh_market(uint64_t market_id, uint32_t cargo_capacity) -> void
  {
  // the place alone will not do as a key: we are at a settlement from the moment we enter, but the goods
  // are known only once the player opens the market, so asking once on a change of place always found nothing
  auto const now{std::chrono::steady_clock::now()};
  if(market_id == market_id_ and now - market_loaded_ < market_refresh)
    return;

  market_id_ = market_id;
  market_loaded_ = now;
  market_lines_.clear();
  station_name_.clear();
  station_faction_.clear();
  station_type_.clear();

  if(market_id == 0u)
    return;

  auto station{db_.load_station(market_id)};
  std::string const name{station and *station ? (*station)->name : std::format("market {}", market_id)};
  // the owner of the place decides whose influence grows from missions handed in here - without it a
  // port's name on its own says little when planning work for a faction
  std::string const owner{station and *station ? (*station)->controlling_faction : std::string{}};

  // kept for the settlement block, which asks the same reading a different question
  if(station and *station)
    {
    station_name_ = (*station)->name;
    station_faction_ = owner;
    station_type_ = (*station)->station_type;
    }

  auto entries{db_.load_market_entries(market_id)};
  if(not entries or entries->empty())
    {
    // the Market event appears only once the commodities screen is opened - silence here would look like
    // there being no opportunity, when it means nothing but that we had nothing to record
    market_lines_.push_back(overlay::line_t{.text = std::format("{}: no market data", name), .color = colour_alert});
    if(not owner.empty())
      market_lines_.push_back(overlay::line_t{.text = std::format("  {}", owner), .color = colour_first});
    market_lines_.push_back(overlay::line_t{.text = "  open the commodity market to record it", .color = colour_plain});
    return;
    }

  // the departure from the galactic average is the only number saying whether a price is a bargain
  auto const sell_gain{
    [](info::market_entry_t const & entry) -> double
    {
      if(entry.mean_price == 0u or entry.demand == 0u)
        return 0.0;
      return (double(entry.sell_price) - double(entry.mean_price)) / double(entry.mean_price);
    }
  };
  auto const buy_gain{
    [](info::market_entry_t const & entry) -> double
    {
      if(entry.mean_price == 0u or entry.stock == 0u or entry.buy_price == 0u)
        return 0.0;
      return (double(entry.mean_price) - double(entry.buy_price)) / double(entry.mean_price);
    }
  };

  // a market read before the flags were added has zeros everywhere - then the flags cannot be trusted
  bool const flags_known{
    std::ranges::any_of(*entries, [](info::market_entry_t const & entry) { return entry.producer or entry.consumer; })
  };

  std::vector<info::market_entry_t const *> sells;
  std::vector<info::market_entry_t const *> buys;
  for(info::market_entry_t const & entry: *entries)
    {
    // the game gives a price for commodities the station does not trade in as well - without these flags
    // Platinum at Scott View looked like an opportunity, though it was neither sold nor bought there.
    // markets stored before the flags were added have them at zero, so for those we fall back on stock and demand
    bool const buys_it{flags_known ? entry.consumer : entry.demand > 0u};
    bool const sells_it{flags_known ? entry.producer : entry.stock > 0u};

    if(
      buys_it and sell_gain(entry) >= interesting_deviation and entry.sell_price > entry.mean_price + interesting_margin
    )
      sells.push_back(&entry);
    if(
      sells_it and buy_gain(entry) >= interesting_deviation and entry.buy_price + interesting_margin < entry.mean_price
    )
      buys.push_back(&entry);
    }

  // the hold has a fixed capacity, so what decides the choice is the amount per tonne, not the percentage
  std::ranges::sort(
    sells, std::ranges::greater{}, [](info::market_entry_t const * e) { return e->sell_price - e->mean_price; }
  );
  std::ranges::sort(
    buys, std::ranges::greater{}, [](info::market_entry_t const * e) { return e->mean_price - e->buy_price; }
  );

  size_t on_sale{};
  size_t wanted{};
  for(info::market_entry_t const & entry: *entries)
    {
    on_sale += entry.stock > 0u ? 1u : 0u;
    wanted += entry.demand > 0u ? 1u : 0u;
    }

  // a port is either a supplier or a buyer - these two numbers say which at first glance
  market_lines_.push_back(
    overlay::line_t{.text = std::format("{}: {} on sale, {} wanted", name, on_sale, wanted), .color = colour_heading}
  );
  if(not owner.empty())
    market_lines_.push_back(overlay::line_t{.text = std::format("  {}", owner), .color = colour_first});

  // a trader will not bring in a raw material nobody sells - a station may pay splendidly for it and it
  // stays a dead end all the same, so it goes at the end and under a heading of its own
  auto const mined{[](info::market_entry_t const * entry) { return info::is_mining_only(entry->name); }};
  auto const tradeable{std::ranges::partition(sells, std::not_fn(mined))};
  std::vector<info::market_entry_t const *> const dug_up(tradeable.begin(), tradeable.end());
  sells.erase(tradeable.begin(), tradeable.end());

  if(not sells.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "pays above average:", .color = colour_plain});
    for(info::market_entry_t const * entry: sells | std::views::take(listed_commodities))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} over avg",
            entry->name,
            format_credits_value(entry->sell_price),
            format_credits_value(entry->sell_price - entry->mean_price)
          ),
          .color = colour_first
        }
      );
    }

  if(not dug_up.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "pays well, but mining only:", .color = colour_plain});
    for(info::market_entry_t const * entry: dug_up | std::views::take(2u))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} over avg",
            entry->name,
            format_credits_value(entry->sell_price),
            format_credits_value(entry->sell_price - entry->mean_price)
          ),
          .color = colour_plain
        }
      );
    }

  if(not buys.empty())
    {
    market_lines_.push_back(overlay::line_t{.text = "sells below average:", .color = colour_plain});
    for(info::market_entry_t const * entry: buys | std::views::take(listed_commodities))
      market_lines_.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} Cr  {} under avg",
            entry->name,
            format_credits_value(entry->buy_price),
            format_credits_value(entry->mean_price - entry->buy_price)
          ),
          .color = colour_plain
        }
      );
    }

  // the galactic average says whether a price is good, but the earnings come from the difference between markets
  auto const add_trades{
    [this, cargo_capacity](bool bring_here, char const * heading)
    {
      auto trades{db_.load_trade_options(market_id_, listed_trades, bring_here)};
      if(not trades or trades->empty())
        return;

      market_lines_.push_back(overlay::line_t{.text = heading, .color = colour_plain});

      for(info::trade_option_t const & trade: *trades)
        {
        auto const margin{trade.sell_price - trade.buy_price};
        // one run is as many tonnes as the hold takes, provided the goods and the demand suffice
        auto const tonnes{std::min({cargo_capacity != 0u ? cargo_capacity : trade.stock, trade.stock, trade.demand})};
        auto const run{uint64_t{margin} * tonnes};

        market_lines_.push_back(
          overlay::line_t{
            .text = std::format("  {}  {} Cr/t", trade.commodity, format_credits_value(margin)), .color = colour_first
          }
        );
        market_lines_.push_back(
          overlay::line_t{
            .text = std::format(
              "    {} t = {} Cr  {} {}{}{}",
              format_credits_value(tonnes),
              format_credits_value(static_cast<uint32_t>(std::min<uint64_t>(run, UINT32_MAX))),
              bring_here ? "from" : "to",
              trade.station,
              trade.system.empty() ? "" : ", ",
              trade.system
            ),
            .color = colour_plain
          }
        );
        }
    }
  };

  add_trades(true, "bring here, best known:");
  add_trades(false, "take from here, best known:");
  }

auto overlay_feed_t::refresh_supply() -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(now - supply_loaded_ < supply_refresh)
    return;

  supply_loaded_ = now;
  needs_.clear();
  options_.clear();
  producers_.clear();

  if(auto needs{db_.load_cargo_needs()}; needs)
    needs_ = std::move(*needs);

  if(needs_.empty())
    return;

  if(auto options{db_.load_supply_options()}; options)
    options_ = std::move(*options);

  if(auto producers{db_.load_producers()}; producers)
    producers_ = std::move(*producers);
  }

///\brief what the open missions can be advanced with without flying anywhere
///
/// Two kinds of work meet at a settlement and the game words them differently. One names the place:
/// a theft, a download, an assassination at a settlement given by name. The other names a faction
/// and no place at all - exterminating that faction's members counts at any settlement it holds,
/// this one included, and nothing in the mission says so. Standing on the pad both are the same
/// question, which is why they are answered together and above everything else on this side.
auto overlay_feed_t::build_settlement_lines(current_state_t const & state) const -> std::vector<overlay::line_t>
  {
  if(station_name_.empty())
    return {};

  // the faction match holds only where one can walk up to them; in a starport the same mission
  // would be a suggestion to do something the game does not allow
  bool const on_foot{station_type_ == "OnFootSettlement"};

  std::vector<info::mission_t const *> hand_in;
  std::vector<info::mission_t const *> here;
  std::vector<info::mission_t const *> fits;

  for(info::mission_t const & mission: state.active_missions)
    {
    bool const done{mission.status == info::mission_status_e::redirected};
    if(mission.status != info::mission_status_e::accepted and not done)
      continue;

    // A redirected mission keeps the settlement it was done at in its destination, and that place is
    // now finished business - only the handing in is left. Matching it by destination would send the
    // player back to a job already done.
    if(done)
      {
      if(mission.redirected_settlement == station_name_ or mission.redirected_station == station_name_)
        hand_in.push_back(&mission);
      }
    else if(mission.destination_settlement == station_name_ or mission.destination_station == station_name_)
      here.push_back(&mission);
    else if(
      on_foot and not station_faction_.empty() and mission.target_faction == station_faction_
      and mission.destination_settlement.empty()
    )
      fits.push_back(&mission);
    }

  if(hand_in.empty() and here.empty() and fits.empty())
    return {};

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = station_name_, .color = colour_heading});
  if(not station_faction_.empty())
    lines.push_back(overlay::line_t{.text = std::format("  {}", station_faction_), .color = colour_plain});

  auto const add = [&](std::vector<info::mission_t const *> const & group, char const * caption, uint32_t colour)
  {
    if(group.empty())
      return;

    lines.push_back(overlay::line_t{.text = caption, .color = colour_heading});
    for(info::mission_t const * mission: group | std::views::take(listed_settlement_work))
      {
      auto const left{std::chrono::duration_cast<std::chrono::seconds>(mission->expiry - now)};
      auto const count{mission->mission_count()};

      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}{}  {}",
            info::transform_mission_name(mission->type),
            count > 1u ? std::format("  x{}", count) : std::string{},
            format_remaining(left)
          ),
          .color = left < expiry_warning ? colour_expiring : colour
        }
      );
      }

    if(group.size() > listed_settlement_work)
      lines.push_back(
        overlay::line_t{
          .text = std::format("  ... and {} more", group.size() - listed_settlement_work), .color = colour_plain
        }
      );
  };

  add(hand_in, "hand in here:", colour_first);
  add(here, "do here:", colour_plain);
  add(fits, "any settlement of this faction:", colour_plain);

  return lines;
  }

auto overlay_feed_t::build_supply_lines(events::cargo_file_t const & cargo) const -> std::vector<overlay::line_t>
  {
  if(needs_.empty())
    return {};

  // the names in the hold are internal, in missions readable - we compare by letters and digits alone
  auto const key{[](std::string_view text)
                 {
                   std::string out;
                   for(char const c: text)
                     if(std::isalnum(static_cast<unsigned char>(c)))
                       out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                   return out;
                 }};

  std::map<std::string, uint32_t> aboard;
  for(events::cargo_item_t const & item: cargo.Inventory)
    {
    auto const internal{key(item.Name)};
    aboard[internal] += item.Count;

    // both names can come down to the same key, and then the quantity would be counted twice
    if(auto const localised{key(item.Name_Localised)}; not localised.empty() and localised != internal)
      aboard[localised] += item.Count;
    }

  auto const held{
    [&](std::string const & commodity) -> uint32_t
    {
      auto const it{aboard.find(key(commodity))};
      return it != aboard.end() ? it->second : 0u;
    }
  };

  std::vector<overlay::line_t> lines;
  lines.push_back(overlay::line_t{.text = "mission cargo:", .color = colour_heading});

  std::set<std::string> missing;
  for(info::cargo_need_t const & need: needs_)
    {
    auto const have{held(need.commodity)};
    bool const known{std::ranges::any_of(
      options_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
    )};

    // a market may trade in it and happen to be empty - that is quite different news from having no source
    auto const seller{std::ranges::find_if(
      producers_, [&need](info::supply_option_t const & option) { return option.commodity == need.commodity; }
    )};
    bool const sold_somewhere{seller != producers_.end()};

    if(have < need.count)
      missing.insert(need.commodity);

    // the number in brackets is what the hold holds - it is what makes a missing item stand out
    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {} x{} ({}){}",
          need.commodity,
          need.count,
          have,
          have >= need.count or known ? std::string{}
          : sold_somewhere            ? std::format("   {} sells it, out of stock", seller->station)
                                      : std::string{"   no source known"}
        ),
        .color = have >= need.count ? colour_first : (known ? colour_plain : colour_alert)
      }
    );
    }

  if(missing.empty())
    {
    lines.push_back(overlay::line_t{.text = "all aboard", .color = colour_first});
    return lines;
    }

  // one place for several missing goods saves a whole run; only after that does the kind of port count
  struct place_t
    {
    std::string station;
    std::string system;
    int rank{};
    std::vector<info::supply_option_t const *> items;
    };

  std::map<uint64_t, place_t> places;
  for(info::supply_option_t const & option: options_)
    {
    if(not missing.contains(option.commodity))
      continue;

    place_t & place{places[option.market_id]};
    if(place.items.empty())
      {
      place.station = option.station.empty() ? std::format("market {}", option.market_id) : option.station;
      place.system = option.system;
      place.rank = station_rank(option.station_type);
      }
    place.items.push_back(&option);
    }

  std::vector<place_t const *> ranked;
  ranked.reserve(places.size());
  for(auto const & [market_id, place]: places)
    ranked.push_back(&place);

  std::ranges::sort(
    ranked,
    [](place_t const * left, place_t const * right)
    {
      if(left->items.size() != right->items.size())
        return left->items.size() > right->items.size();
      return left->rank < right->rank;
    }
  );

  for(place_t const * place: ranked | std::views::take(listed_sources))
    {
    lines.push_back(
      overlay::line_t{
        .text = std::format("{}{}{}", place->station, place->system.empty() ? "" : "  ", place->system),
        .color = colour_first
      }
    );

    for(info::supply_option_t const * item: place->items | std::views::take(listed_commodities))
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}  {} in stock  {} Cr",
            item->commodity,
            format_credits_value(item->stock),
            format_credits_value(item->buy_price)
          ),
          .color = colour_plain
        }
      );
    }

  return lines;
  }

auto overlay_feed_t::build_route_lines(current_state_t const & state, plotted_route_t const & plotted) const
  -> std::vector<overlay::line_t>
  {
  // a route can run to forty jumps and the side band holds a dozen or so lines - the further ones change
  // nothing about what is being done right now
  constexpr size_t listed_hops{10};

  ///\brief at which stars one can refuel
  ///\detail KGBFOAM; the class sometimes comes with a subtype, so what counts is the first letter
  auto const scoopable = [](std::string_view star_class) -> bool
  {
    return not star_class.empty() and std::string_view{"KGBFOAM"}.find(star_class.front()) != std::string_view::npos;
  };

  std::vector<overlay::line_t> lines;

  if(not plotted.waypoints.empty())
    {
    // numerator and denominator both count waypoints - mixing them with jumps gave "8 of 7"
    auto const left{plotted.waypoints.size() - std::min(plotted.reached, plotted.waypoints.size())};
    lines.push_back(
      overlay::line_t{
        .text = std::format("plotted route: {} of {} stops left", left, plotted.waypoints.size()),
        .color = colour_heading
      }
    );

    for(size_t ix{plotted.reached}; ix < plotted.waypoints.size() and ix < plotted.reached + listed_hops; ++ix)
      {
      info::neutron_waypoint_t const & waypoint{plotted.waypoints[ix]};
      lines.push_back(
        overlay::line_t{
          // a neutron star does not refuel, and the file gives no class for the others
          .text = std::format(
            "  {}. {}  {}  {:.0f} ly",
            ix - plotted.reached + 1u,
            waypoint.system,
            waypoint.neutron ? "N no fuel" : "?",
            waypoint.distance
          ),
          .color = ix == plotted.reached ? colour_first : colour_plain
        }
      );
      }
    }

  auto pending{state.route_ | std::views::filter([](info::route_item_t const & item) { return not item.visited; })};
  if(std::ranges::distance(pending) != 0)
    {
    lines.push_back(
      overlay::line_t{
        .text = std::format("game route: {} jumps", std::ranges::distance(pending)), .color = colour_heading
      }
    );

    size_t shown{};
    for(info::route_item_t const & item: pending | std::views::take(listed_hops))
      {
      bool const fuel{scoopable(item.star_class)};
      lines.push_back(
        overlay::line_t{
          .text = std::format(
            "  {}. {}  {}{}  {:.0f} ly",
            ++shown,
            item.system,
            item.star_class.empty() ? "?" : item.star_class,
            fuel ? " fuel" : "",
            item.distance
          ),
          .color = fuel ? colour_first : colour_plain
        }
      );
      }
    }

  return lines;
  }

auto overlay_feed_t::build_logistics_lines() const -> std::vector<overlay::line_t>
  {
  auto & db{const_cast<database_storage_t &>(db_)};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<overlay::line_t> lines;

  // The last port decides where an escape pod sends you - settlements and carriers do not count, and
  // after a few stops it is easy to lose track of which of them was a port
  if(auto port{db.load_last_port()}; port and *port)
    lines.push_back(
      overlay::line_t{
        .text = std::format("last port: {}, {} ({})", (*port)->name, (*port)->system, (*port)->station_type),
        .color = colour_plain
      }
    );

  auto pending{db.load_transfers_in_flight(now)};
  if(not pending or pending->empty())
    return lines;

  lines.push_back(overlay::line_t{.text = "ships in transit:", .color = colour_heading});
  for(info::ship_transfer_t const & transfer: *pending)
    {
    auto const left{std::chrono::duration_cast<std::chrono::minutes>(transfer.arrives - now)};
    auto const station{db.load_station(transfer.to_market_id)};

    lines.push_back(
      overlay::line_t{
        .text = std::format(
          "  {} -> {}  in {}h {:02}min",
          transfer.ship_type,
          station and *station ? (*station)->name : std::format("market {}", transfer.to_market_id),
          left.count() / 60,
          left.count() % 60
        ),
        // the last quarter of an hour is when it pays to be there
        .color = left < std::chrono::minutes{15} ? colour_first : colour_plain
      }
    );
    }

  return lines;
  }

auto overlay_feed_t::publish(current_state_t const & state, plotted_route_t const & plotted) -> void
  {
  if(not server_->listening())
    return;

  refresh_factions(state);
  refresh_market(state.settlement_market_id_, state.ship_loadout.CargoCapacity);
  refresh_supply();

  overlay::frame_t frame{};

  if(not state.system.name.empty())
    {
    auto lines{describe_system(state.system, faction_lines_.empty())};
    lines.insert(lines.end(), faction_lines_.begin(), faction_lines_.end());
    lines.insert(lines.end(), conflict_lines_.begin(), conflict_lines_.end());
    frame.blocks.push_back(
      overlay::block_t{
        // the chart goes under the text, because the names and the current values are what one
        // reads at a glance; the shape of the last ten days is what one studies when there is time
        .corner = overlay::corner_e::top_left,
        .ttl_ms = block_ttl_ms,
        .lines = std::move(lines),
        .charts = faction_charts_
      }
    );
    }

  if(auto exploration{describe_exploration(state.system)}; not exploration.empty())
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_right, .ttl_ms = block_ttl_ms, .lines = std::move(exploration)
      }
    );

  // what can be done here comes first: it is the only thing on this side that asks nothing of the
  // player but to turn around, and the trading below it will still be there afterwards
  if(auto settlement{build_settlement_lines(state)}; not settlement.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms, .lines = std::move(settlement)}
    );

  if(auto supply{build_supply_lines(state.cargo)}; not supply.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms, .lines = std::move(supply)}
    );

  // the market underneath, because it is longer and less urgent than what the missions still lack
  if(not market_lines_.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::top_right, .ttl_ms = block_ttl_ms, .lines = market_lines_}
    );

  if(auto logistics{build_logistics_lines()}; not logistics.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms, .lines = std::move(logistics)}
    );

  // the route on the left, above the hold - in flight it is the thing one looks at
  if(auto route{build_route_lines(state, plotted)}; not route.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms, .lines = std::move(route)}
    );

  if(auto cargo{describe_cargo(state.cargo)}; not cargo.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms, .lines = std::move(cargo)}
    );

  if(auto missions{describe_missions(state.active_missions)}; not missions.empty())
    frame.blocks.push_back(
      overlay::block_t{.corner = overlay::corner_e::bottom_left, .ttl_ms = block_ttl_ms, .lines = std::move(missions)}
    );

  if(not state.next_target.Name.empty() and state.next_target.Name != state.system.name)
    frame.blocks.push_back(
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = block_ttl_ms,
        .lines = {overlay::line_t{
          .text = std::format(
            "next: {} ({})",
            state.next_target.Name,
            state.next_target.StarClass.empty() ? "?" : state.next_target.StarClass
          ),
          .color = colour_plain
        }}
      }
    );

  // the game gets a frame when it has changed or when the keep-alive time has passed - not on every journal event
  auto const now{std::chrono::steady_clock::now()};
  if(same_content(frame, last_) and now - last_sent_ < heartbeat)
    return;

  frame.seq = ++sequence_;
  server_->publish(frame);
  last_ = std::move(frame);
  last_sent_ = now;
  }
