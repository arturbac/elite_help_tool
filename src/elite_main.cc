#include <eht_settings.h>
#include <data/bgs.h>
#include <data/progress.h>
#include <file_io.h>
#include <biology.h>
#include <territory.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <ranges>
#include <fstream>
#include <thread>
#include <chrono>
#include <print>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <generic_state.h>
#include <database_import_state.h>
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <system_error>
#include <json_io.h>
#include <cstdlib>
#include <exception>
#include <boost/stacktrace.hpp>
#include <boost/exception/to_string.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;



///\brief how late after the announcement a war started - from the history of every conflict in the database
///
/// The game does not write the moment a war begins into the journal; the faction support panel shows it
/// at once, the logs only at the next reading of the system. That is why every row is a span rather than
/// a moment - and settlements enter the war state later still than the conflict itself
void print_war_onsets(database_storage_t & db)
  {
  auto onsets{db.load_war_onsets()};
  if(not onsets)
    {
    std::println(stderr, "could not read war onsets");
    return;
    }

  std::vector<double> lags;
  lags.reserve(onsets->size());
  for(info::war_onset_t const & onset: *onsets)
    lags.push_back(
      double(std::chrono::duration_cast<std::chrono::minutes>(onset.active_first - onset.pending_last).count()) / 60.0
    );

  std::println("\n=== WAR ONSETS === {} conflicts with a recorded pending -> active transition", onsets->size());
  if(not lags.empty())
    {
    std::vector<double> sorted{lags};
    std::ranges::sort(sorted);
    std::println(
      "uncertainty window: shortest {:.1f}h, median {:.1f}h, longest {:.1f}h"
      "  (an upper bound - it also covers the time we were not there)",
      sorted.front(),
      sorted[sorted.size() / 2u],
      sorted.back()
    );
    }

  std::println(
    "{:<24}{:<9}{:>7}  {:<26}{:^15}{:<26}{:<19}{:<19}",
    "system",
    "type",
    "window",
    "faction A",
    "state",
    "faction B",
    "announced UTC",
    "seen running UTC"
  );
  for(size_t ix{}; ix < onsets->size() and ix < 25u; ++ix)
    {
    info::war_onset_t const & onset{(*onsets)[ix]};
    // an empty status means the war has closed - only then is the result final
    std::string const state{
      onset.status == "pending"
        ? std::string{"announced"}
        : std::format("{} : {}{}", onset.won_days1, onset.won_days2, onset.status.empty() ? "" : " running")
    };

    std::println(
      "{:<24}{:<9}{:>6.1f}h  {:<26}{:^15}{:<26}{:<19}{:<19}",
      onset.system_name.substr(0, 23),
      onset.war_type.substr(0, 8),
      lags[ix],
      onset.faction1.substr(0, 25),
      state,
      onset.faction2.substr(0, 25),
      std::format("{:%d.%m %H:%M}", onset.pending_last),
      std::format("{:%d.%m %H:%M}", onset.active_first)
    );
    }
  }

///\brief effort in pluses set against what it gave - BGS day by BGS day
///
/// The pluses per percentage point are counted separately for every system and averaged nowhere,
/// because the game divides mission influence by the size of the system: the same ten pluses give,
/// in a forty-million system, a fraction of what they give in a forty-thousand one
void print_bgs_effort(database_storage_t & db, uint32_t within_days)
  {
  auto effort{db.load_bgs_effort(within_days, 0u)};
  if(not effort)
    {
    std::println(stderr, "could not read BGS effort");
    return;
    }

  std::println("\n=== BGS EFFORT === {} rows from the last {} days", effort->size(), within_days);
  std::println(
    "{:<16}{:<24}{:>9}  {:<24}{:<11}{:>5}{:>7}{:>7}{:>8}{:>18}{:>11}",
    "closed UTC",
    "system",
    "population",
    "faction",
    "state",
    "msn",
    "up",
    "down",
    "share",
    "influence",
    "sys pl/pt"
  );

  for(info::bgs_effort_t const & row: *effort)
    {
    std::string const closed{
      row.closed_by == std::chrono::sys_seconds{} ? std::string{"running"} : std::format("{:%d.%m %H:%M}", row.closed_by)
    };

    std::string moved{"-"};
    if(row.influence_before and row.influence_after)
      moved = std::format("{:.1f}->{:.1f}", *row.influence_before, *row.influence_after);

    // what part of all the upward work put into this system that day went to this faction
    std::string share{"-"};
    if(row.pushed_up > 0 and row.system_pushed_up > 0)
      share = std::format("{:.0f}%", 100.0 * double(row.pushed_up) / double(row.system_pushed_up));

    // The cost of a point is a property of the system, not of the faction - the percentages add up to a
    // hundred, so factions pushed on the same day share one gain between them. At movements of tenths of
    // a point it says nothing but the rounding and what other players did anyway
    std::string rate{"-"};
    if(row.system_gain and *row.system_gain >= 0.3 and row.system_pushed_up > 0)
      rate = std::format("{:.1f}", double(row.system_pushed_up) / *row.system_gain);

    std::println(
      "{:<16}{:<24}{:>9}  {:<24}{:<11}{:>5}{:>7}{:>7}{:>8}{:>18}{:>11}",
      closed,
      row.system_name.substr(0, 23),
      info::format_population(row.population),
      row.faction.substr(0, 23),
      row.faction_state.substr(0, 10),
      row.missions,
      row.pushed_up,
      row.pushed_down,
      share,
      moved,
      rate
    );
    }
  }

///\brief what cartographic data paid against what EHT reckoned, sale by sale - one system a sale is an exact price
void print_cartography(fs::path const & journal_dir, std::string_view commander)
  {
  auto const sales{bio::cartography_sales(journal_dir, commander)};
  std::println("\n=== CARTOGRAPHY SALES === {}", sales.size());
  std::println(
    "{:<17}{:>6}{:>7}{:>12}{:>12}{:>10}{:>12}{:>8}  {}",
    "sold UTC",
    "sys",
    "bodies",
    "estimate",
    "base",
    "bonus",
    "total",
    "base/est",
    "system"
  );
  for(bio::cartography_sale_t const & sale: sales)
    std::println(
      "{:<17%d.%m.%Y %H:%M}{:>6}{:>7}{:>12}{:>12}{:>10}{:>12}{:>8}  {}",
      sale.when,
      sale.systems.size(),
      sale.bodies,
      sale.estimate,
      sale.base_value,
      sale.bonus,
      sale.total,
      sale.estimate != 0u ? std::format("{:.2f}", double(sale.base_value) / double(sale.estimate)) : std::string{"-"},
      sale.systems.size() == 1u ? sale.systems.front() : std::format("{} systems", sale.systems.size())
    );
  }

///\brief the systems one's own factions are in, each as last read - the Territory tab in the terminal
void print_territory(database_storage_t & db)
  {
  auto const own{eht::settings()->bgs.own_factions};
  if(own.empty())
    {
    std::println(stderr, "no factions of your own: set bgs.own_factions in {}", eht::settings_file_name);
    return;
    }
  auto systems{db.load_territory(own)};
  auto wave{db.newest_influence_wave()};
  if(not systems or not wave)
    {
    std::println(stderr, "could not read the territory");
    return;
    }

  std::println("\n=== TERRITORY === {} systems", systems->size());
  for(territory::standing_t const & standing: territory::standings(*systems, own))
    {
    std::string text{std::format("{:<24} controls {} of {}", standing.faction, standing.controls, standing.present)};
    if(standing.thinnest_lead)
      text += std::format(
        ", thinnest lead {:.1f} over {} in {}",
        *standing.thinnest_lead,
        standing.thinnest_rival,
        standing.thinnest_system
      );
    if(standing.closest_gap)
      text += std::format(
        ", closest to control: {} {:.1f} behind {}",
        standing.closest_system,
        *standing.closest_gap,
        standing.closest_controller
      );
    std::println("{}", text);
    }

  for(territory::system_t const & system: *systems)
    {
    auto const seen{territory::tick_seen(system, *wave)};
    std::println(
      "\n{} ({}), {}{}",
      system.name,
      info::format_population(system.population),
      seen == territory::tick_seen_e::known       ? std::string{"the tick seen"}
      : seen == territory::tick_seen_e::unchanged ? std::string{"unchanged since the tick"}
      : system.seen ? std::format("not seen since the tick, read {:%d.%m %H:%M}", *system.seen)
                    : std::string{"never read"},
      system.pushed_up != 0 or system.pushed_down != 0
        ? std::format(", pushed +{}/-{} since", system.pushed_up, system.pushed_down)
        : std::string{}
    );
    for(territory::faction_t const & faction: system.factions)
      std::println(
        "  {}{:<36}{:>6.1f}%  {:<6} {}",
        faction.name == system.controlling ? "* " : "  ",
        faction.name,
        faction.influence,
        faction.moved ? std::format("{:+.1f}", *faction.moved) : std::string{"?"},
        faction.active
      );
    for(std::string const & note: territory::notes(system, own, eht::settings()->bgs.retreat_below))
      std::println("  ! {}", note);
    }
  }

///\brief prints the observed recalculation waves - influence apart, wars apart
///
/// None of these numbers is a forecast. The tick drifts every few days, over a weekend it can stay away
/// for almost two days, and after a game update it gets lost altogether - so we print nothing but what
/// has been seen
void print_tick_history(database_storage_t & db, uint32_t within_days)
  {
  for(info::tick_kind_e const kind: {info::tick_kind_e::influence, info::tick_kind_e::war})
    {
    auto facts{db.load_recent_ticks(kind, within_days)};
    if(not facts)
      {
      std::println(stderr, "could not read tick history");
      continue;
      }

    std::println(
      "\n=== {} === {} waves from the last {} days",
      kind == info::tick_kind_e::influence ? "INFLUENCE (hand missions in before the start)"
                                           : "WARS (sell bonds after the end)",
      facts->size(),
      within_days
    );
    if(auto stats{db.load_tick_stats(kind, within_days)}; stats)
      std::println(
        "gap typically {:.1f}h, longest {:.1f}h | measurement window typically {} min"
        " | waves seen in more than one system: {} of {} (widest spread {} min)",
        double(stats->typical_gap.count()) / 60.0,
        double(stats->longest_gap.count()) / 60.0,
        stats->typical_window.count(),
        stats->multi_system_waves,
        stats->waves,
        stats->widest_spread.count()
      );

    std::println(
      "{:<24}{:<24}{:>5}{:>10}{:>9}", "wave start (UTC)", "wave end (UTC)", "sys", "reads", "to next"
    );

    std::optional<std::chrono::sys_seconds> previous;
    for(info::tick_fact_t const & f: *facts)
      {
      std::string gap{"-"};
      if(previous)
        {
        auto const hours{std::chrono::duration_cast<std::chrono::minutes>(*previous - f.start_end).count() / 60.0};
        gap = std::format("{:.1f}h", hours);
        }
      previous = f.start_end;

      std::println(
        "{:<24}{:<24}{:>5}{:>10}{:>9}",
        std::format("{:%m-%d %H:%M} - {:%H:%M}", f.start_begin, f.start_end),
        std::format("{:%m-%d %H:%M} - {:%H:%M}", f.end_begin, f.end_end),
        f.systems,
        f.samples,
        gap
      );
      }
    }
  }

struct config_t
  {
  fs::path directory;
  };

void terminate_handler()
  {
  std::cerr << "\nTerminate handler called.\n";
  std::cerr << boost::stacktrace::stacktrace();
  std::abort();
  }

///\brief only what may be called inside a signal handler: write(2), the stack dumped by the signal-safe call,
/// and the signal raised again with its default action - the process ends as the signal meant it to
void signal_handler(int signal)
  {
  constexpr char said[]{"\nSignal handler called, the stack follows\n"};
  [[maybe_unused]]
  auto const written{::write(STDERR_FILENO, said, sizeof(said) - 1u)};
  boost::stacktrace::safe_dump_to(STDERR_FILENO);
  std::signal(signal, SIG_DFL);
  std::raise(signal);
  }

[[nodiscard]]
auto main(int argc, char ** argv) -> int
  {
  spdlog::set_pattern("[%^%l%$] %v");
  // register the handler for std::terminate
  std::set_terminate(terminate_handler);

  // register the handler for signals (SIGABRT, SIGTERM and so on)
  std::signal(SIGABRT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // spdlog::set_level(spdlog::level::debug);

  // the thresholds behind reading the tick are the tool's settings - taken from the file when there is
  // one here, but a rebuild in a scratch directory does not leave a fresh one behind
  if(fs::exists(eht::settings_file_name))
    eht::load_settings(eht::settings_file_name);

  // the FID sits in the Commander event right at the start of the file, so there is no need to read it all
  auto commander_of = [](fs::path const & journal) -> events::commander_t
  {
    std::ifstream file{journal};
    std::string line;
    for(int read{}; read < 20 and std::getline(file, line); ++read)
      {
      if(not line.contains(R"("event":"Commander")"))
        continue;
      events::commander_t who{};
      if(auto res{eht::json::read_lenient(who, line)}; not res)
        return who;
      }
    return {};
  };

  po::options_description desc("Options");
  desc.add_options()("help,h", "show help")(
    "dir,d", po::value<std::string>()->default_value("."), "journal folder"
  )("commander,c",
    po::value<std::string>()->default_value(""),
    "FID of the account this database belongs to; empty = account from the newest journal, 'all' = no distinction"
  )("ticks",
    po::value<uint32_t>()->implicit_value(30),
    "print the observed recalculation waves from the existing database instead of importing, for this many days"
  )("bgs",
    po::value<uint32_t>()->implicit_value(14),
    "print the effort in pluses against the influence it moved, day by day, for this many days"
  )("wars", "print how late after the announcement the wars actually started")(
    "territory", "print the systems of the factions in bgs.own_factions, each as last read"
  )("cartography", "print every sale of cartographic data from the journals, what it paid against EHT's estimate");

  po::variables_map vm;
  try
    {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    }
  catch(std::exception const & e)
    {
    std::println(stderr, "bad arguments: {}", e.what());
    return 1;
    }

  if(vm.count("help"))
    {
    std::cout << desc << "\n";
    return 0;
    }

  // straight from the journals, the database untouched; --commander narrows it to one account, empty takes all
  if(vm.count("cartography"))
    {
    print_cartography(fs::path{vm["dir"].as<std::string>()}, vm["commander"].as<std::string>());
    return 0;
    }

  if(vm.count("ticks") or vm.count("bgs") or vm.count("wars") or vm.count("territory"))
    {
    database_import_state_t::state_t state{"ehtdb.sqlite"};
    if(auto const res{state.db_.open(storage_mode_e::live)}; not res)
      {
      if(res.error() == std::errc::not_supported)
        std::println(
          "this database was written by a newer EHT than this build understands - refusing to touch it"
        );
      return EXIT_FAILURE;
      }
    if(vm.count("ticks"))
      print_tick_history(state.db_, vm["ticks"].as<uint32_t>());
    if(vm.count("bgs"))
      print_bgs_effort(state.db_, vm["bgs"].as<uint32_t>());
    if(vm.count("wars"))
      print_war_onsets(state.db_);
    if(vm.count("territory"))
      print_territory(state.db_);
    return 0;
    }

  auto const path = fs::path{vm["dir"].as<std::string>()};

  // the import builds both rebuildable databases from scratch and writes with a plain INSERT, so leaving
  // the previous contents in place ends in a key conflict or in doubled rows.
  // live.sqlite is left untouched - its contents cannot be rebuilt from journals
  for(char const * rebuildable: {"ehtdb.sqlite", "galaxy.sqlite"})
    {
    if(not fs::exists(rebuildable))
      continue;

    // galaxy.sqlite is sometimes a link to a file shared with the second account - removing the link
    // itself would break that silently, so we remove the contents and the link stays.
    // sqlite opens with O_CREAT, so it creates the file again on the other side of the link
    std::error_code ec;
    fs::path const target{fs::weakly_canonical(rebuildable, ec)};
    fs::remove(ec ? fs::path{rebuildable} : target);
    }
  database_import_state_t dbimport{path.string()};
  database_import_state_t::state_t state{"ehtdb.sqlite"};
  if(auto const res{state.db_.open(storage_mode_e::bulk_import)}; not res)
    {
    if(res.error() == std::errc::not_supported)
      std::println(
        "live.sqlite was written by a newer EHT than this build understands - it cannot be rebuilt, "
        "run the newer EHT"
      );
    return EXIT_FAILURE;
    }
  dbimport.state = &state;
  std::vector<fs::path> journals{find_all_journals(path)};

  // the directory is sometimes mixed - a prefix copied to a second account brings somebody else's
  // journals with it. the world we take from all of them, the career only from this database's owner
  if(auto const & chosen{vm["commander"].as<std::string>()}; chosen == "all")
    std::println("importing without distinguishing accounts");
  else if(not chosen.empty())
    state.owner_fid = chosen;
  else if(not journals.empty())
    {
    events::commander_t const who{commander_of(journals.back())};
    state.owner_fid = who.FID;
    if(not who.FID.empty())
      if(auto res{state.db_.store_owner(info::db_owner_t{.fid = who.FID, .name = who.Name})}; not res)
        std::println(stderr, "could not store the database owner");
    }

  if(not state.owner_fid.empty())
    std::println("database belongs to account {}", state.owner_fid);
  else
    std::println("could not determine the account - career will be taken from every journal");

  for(fs::path const & p: journals)
    {
    std::println("Importing file: {}", p.string());
    read_file(p, std::bind_front(&generic_state_t::discovery, &dbimport));
    }

  return 0;
  }

