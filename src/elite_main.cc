#include <file_io.h>
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
#include <elite_events.h>
#include <database_import_state.h>
#include <iostream>
#include <csignal>
#include <system_error>
#include <glaze/glaze.hpp>
#include <cstdlib>
#include <exception>
#include <boost/stacktrace.hpp>
#include <boost/exception/to_string.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;



///\brief jak pozno po zapowiedzi wojna ruszala - z historii wszystkich konfliktow w bazie
///
/// Gra nie zapisuje w journalu momentu, w ktorym wojna sie zaczyna; w panelu wsparcia frakcji widac
/// to od razu, w logach dopiero przy nastepnym odczycie systemu. Dlatego kazdy wiersz jest
/// przedzialem, a nie chwila - a osady wchodza w stan wojny jeszcze pozniej niz sam konflikt
void print_war_onsets(database_storage_t & db)
  {
  auto onsets{db.load_war_onsets()};
  if(not onsets)
    {
    std::println(stderr, "nie udalo sie odczytac poczatkow wojen");
    return;
    }

  std::vector<double> lags;
  lags.reserve(onsets->size());
  for(info::war_onset_t const & onset: *onsets)
    lags.push_back(
      double(std::chrono::duration_cast<std::chrono::minutes>(onset.active_first - onset.pending_last).count()) / 60.0
    );

  std::println("\n=== POCZATKI WOJEN === {} konfliktow z zapisanym przejsciem pending -> active", onsets->size());
  if(not lags.empty())
    {
    std::vector<double> sorted{lags};
    std::ranges::sort(sorted);
    std::println(
      "przedzial niepewnosci: najkrotszy {:.1f}h, mediana {:.1f}h, najdluzszy {:.1f}h"
      "  (gorne ograniczenie - zawiera tez czas, w ktorym nas tam nie bylo)",
      sorted.front(),
      sorted[sorted.size() / 2u],
      sorted.back()
    );
    }

  std::println(
    "{:<24}{:<9}{:<40}{:>6}  {:<24}{:<16}{:<16}{:>8}",
    "system",
    "typ",
    "strony",
    "dni",
    "wygrala",
    "zapowiedziana",
    "zauwazona",
    "okno"
  );
  for(size_t ix{}; ix < onsets->size() and ix < 25u; ++ix)
    {
    info::war_onset_t const & onset{(*onsets)[ix]};
    std::println(
      "{:<24}{:<9}{:<40}{:>6}  {:<24}{:<16}{:<16}{:>7.1f}h",
      onset.system_name.substr(0, 23),
      onset.war_type.substr(0, 8),
      std::format("{} / {}", onset.faction1, onset.faction2).substr(0, 39),
      std::format("{}:{}", onset.won_days1, onset.won_days2),
      // pusty status znaczy, ze wojna sie zamknela - dopiero wtedy wynik jest ostateczny
      (not onset.status.empty()  ? std::string{onset.status == "pending" ? "zapowiedziana" : "trwa"}
       : onset.won_days1 > onset.won_days2 ? onset.faction1
       : onset.won_days2 > onset.won_days1 ? onset.faction2
                                           : std::string{"remis"})
        .substr(0, 23),
      std::format("{:%d.%m %H:%M}", onset.pending_last),
      std::format("{:%d.%m %H:%M}", onset.active_first),
      lags[ix]
    );
    }
  }

///\brief praca w plusach zestawiona z tym, co dala - doba BGS po dobie
///
/// Przelicznik plusow na punkt procentowy liczony jest osobno dla kazdego systemu i nigdzie nie
/// jest usredniany, bo gra dzieli wplyw misji przez wielkosc systemu: te same dziesiec plusow daje
/// w systemie czterdziestomilionowym ulamek tego, co w czterdziestotysiecznym
void print_bgs_effort(database_storage_t & db, uint32_t within_days)
  {
  auto effort{db.load_bgs_effort(within_days, 0u)};
  if(not effort)
    {
    std::println(stderr, "nie udalo sie odczytac pracy BGS");
    return;
    }

  std::println("\n=== PRACA BGS === {} pozycji z ostatnich {} dni", effort->size(), within_days);
  std::println(
    "{:<12}{:<24}{:>10}  {:<24}{:>5}{:>7}{:>7}{:>16}{:>9}",
    "zamknieta",
    "system",
    "populacja",
    "frakcja",
    "msn",
    "w gore",
    "w dol",
    "wplyw przed/po",
    "plus/pp"
  );

  for(info::bgs_effort_t const & row: *effort)
    {
    std::string const closed{
      row.closed_by == std::chrono::sys_seconds{} ? std::string{"trwa"} : std::format("{:%d.%m %H:%M}", row.closed_by)
    };

    std::string moved{"-"};
    std::string rate{"-"};
    if(row.influence_before and row.influence_after)
      {
      double const delta{*row.influence_after - *row.influence_before};
      moved = std::format("{:.1f}->{:.1f}", *row.influence_before, *row.influence_after);

      // Przy ruchu rzedu dziesiatych czesci punktu przelicznik mowi juz tylko o zaokragleniu -
      // i o tym, co tej doby zrobili inni gracze, bo wplyw jest suma zerowa
      if(delta >= 0.3 and row.pushed_up > 0)
        rate = std::format("{:.1f}", double(row.pushed_up) / delta);
      }

    std::println(
      "{:<12}{:<24}{:>10}  {:<24}{:>5}{:>7}{:>7}{:>16}{:>9}",
      closed,
      row.system_name.substr(0, 23),
      row.population,
      row.faction.substr(0, 23),
      row.missions,
      row.pushed_up,
      row.pushed_down,
      moved,
      rate
    );
    }
  }

///\brief wypisuje zaobserwowane fale przeliczen - osobno wplywy, osobno wojny
///
/// Zadna z tych liczb nie jest prognoza. Tick przesuwa sie co kilka dni, w weekend potrafi nie
/// przyjsc przez prawie dwie doby, a po aktualizacji gry gubi sie zupelnie - wiec wypisujemy
/// wylacznie to, co zostalo zobaczone
void print_tick_history(database_storage_t & db, uint32_t within_days)
  {
  for(info::tick_kind_e const kind: {info::tick_kind_e::influence, info::tick_kind_e::war})
    {
    auto facts{db.load_recent_ticks(kind, within_days)};
    if(not facts)
      {
      std::println(stderr, "nie udalo sie odczytac historii tickow");
      continue;
      }

    std::println(
      "\n=== {} === {} fal z ostatnich {} dni",
      kind == info::tick_kind_e::influence ? "WPLYWY (oddaj misje przed poczatkiem)"
                                           : "WOJNY (bondy sprzedawaj po koncu)",
      facts->size(),
      within_days
    );
    if(auto stats{db.load_tick_stats(kind, within_days)}; stats)
      std::println(
        "przerwa typowo {:.1f}h, najdluzej {:.1f}h | okno pomiaru typowo {} min"
        " | fal z wiecej niz jednym systemem: {} z {} (najszersza propagacja {} min)",
        double(stats->typical_gap.count()) / 60.0,
        double(stats->longest_gap.count()) / 60.0,
        stats->typical_window.count(),
        stats->multi_system_waves,
        stats->waves,
        stats->widest_spread.count()
      );

    std::println(
      "{:<24}{:<24}{:>5}{:>10}{:>9}", "poczatek fali (UTC)", "koniec fali (UTC)", "sys", "odczytow", "do nast."
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

void signal_handler(int signal)
  {
  std::cerr << "\nSignal handler called for signal: " << signal << "\n";
  std::cerr << boost::stacktrace::stacktrace();
  std::abort();
  }

[[nodiscard]]
auto main(int argc, char ** argv) -> int
  {
  spdlog::set_pattern("[%^%l%$] %v");
  // Rejestracja handlera dla std::terminate
  std::set_terminate(terminate_handler);

  // Rejestracja handlera dla sygnałów (SIGABRT, SIGTERM, itp.)
  std::signal(SIGABRT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // spdlog::set_level(spdlog::level::debug);

  // FID stoi w zdarzeniu Commander zaraz na poczatku pliku, wiec nie trzeba czytac calosci
  auto commander_of = [](fs::path const & journal) -> events::commander_t
  {
    std::ifstream file{journal};
    std::string line;
    for(int read{}; read < 20 and std::getline(file, line); ++read)
      {
      if(not line.contains(R"("event":"Commander")"))
        continue;
      events::commander_t who{};
      if(auto res{glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = false}>(who, line)};
         not res)
        return who;
      }
    return {};
  };

  po::options_description desc("Opcje");
  desc.add_options()("help,h", "Wyświetl pomoc")(
    "dir,d", po::value<std::string>()->default_value("."), "journal folder"
  )("commander,c",
    po::value<std::string>()->default_value(""),
    "FID konta do ktorego nalezy baza; puste = konto z najnowszego journala, 'all' = bez rozroznienia"
  )("ticks",
    po::value<uint32_t>()->implicit_value(30),
    "wypisz zaobserwowane fale przeliczen z istniejacej bazy zamiast importowac, za tyle ostatnich dni"
  )("bgs",
    po::value<uint32_t>()->implicit_value(14),
    "wypisz prace w plusach zestawiona z ruchem wplywow, doba po dobie, za tyle ostatnich dni"
  )("wars", "wypisz jak pozno po zapowiedzi wojny naprawde ruszaly");

  po::variables_map vm;
  try
    {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    }
  catch(std::exception const & e)
    {
    std::println(stderr, "Błąd parametrów: {}", e.what());
    return 1;
    }

  if(vm.count("help"))
    {
    std::cout << desc << "\n";
    return 0;
    }

  if(vm.count("ticks") or vm.count("bgs") or vm.count("wars"))
    {
    database_import_state_t::state_t state{"ehtdb.sqlite"};
    if(not state.db_.open(storage_mode_e::live))
      return EXIT_FAILURE;
    if(vm.count("ticks"))
      print_tick_history(state.db_, vm["ticks"].as<uint32_t>());
    if(vm.count("bgs"))
      print_bgs_effort(state.db_, vm["bgs"].as<uint32_t>());
    if(vm.count("wars"))
      print_war_onsets(state.db_);
    return 0;
    }

  auto const path = fs::path{vm["dir"].as<std::string>()};

  // import buduje obie odtwarzalne bazy od zera i wstawia zwyklym INSERT, wiec pozostawienie
  // poprzedniej zawartosci konczy sie konfliktem klucza albo zdublowanymi wierszami.
  // live.sqlite zostaje nietkniety - jego zawartosci nie da sie odtworzyc z journali
  for(char const * rebuildable: {"ehtdb.sqlite", "galaxy.sqlite"})
    {
    if(not fs::exists(rebuildable))
      continue;

    // galaxy.sqlite bywa dowiazaniem do pliku dzielonego z drugim kontem - skasowanie samego
    // dowiazania zerwaloby to po cichu, wiec kasujemy zawartosc, a dowiazanie zostaje.
    // sqlite otwiera z O_CREAT, wiec zaklada plik z powrotem po drugiej stronie dowiazania
    std::error_code ec;
    fs::path const target{fs::weakly_canonical(rebuildable, ec)};
    fs::remove(ec ? fs::path{rebuildable} : target);
    }
  database_import_state_t dbimport{path.string()};
  database_import_state_t::state_t state{"ehtdb.sqlite"};
  if(not state.db_.open(storage_mode_e::bulk_import))
    return EXIT_FAILURE;
  dbimport.state = &state;
  std::vector<fs::path> journals{find_all_journals(path)};

  // katalog bywa mieszany - prefix kopiowany na drugie konto zabiera ze soba cudze journale.
  // swiat bierzemy ze wszystkich, ale kariere tylko od wlasciciela tej bazy
  if(auto const & chosen{vm["commander"].as<std::string>()}; chosen == "all")
    std::println("import bez rozrozniania kont");
  else if(not chosen.empty())
    state.owner_fid = chosen;
  else if(not journals.empty())
    {
    events::commander_t const who{commander_of(journals.back())};
    state.owner_fid = who.FID;
    if(not who.FID.empty())
      if(auto res{state.db_.store_owner(info::db_owner_t{.fid = who.FID, .name = who.Name})}; not res)
        std::println(stderr, "nie udalo sie zapisac wlasciciela bazy");
    }

  if(not state.owner_fid.empty())
    std::println("baza nalezy do konta {}", state.owner_fid);
  else
    std::println("nie udalo sie ustalic konta - kariera zostanie wzieta ze wszystkich journali");

  for(fs::path const & p: journals)
    {
    std::println("Importing file: {}", p.string());
    read_file(p, std::bind_front(&generic_state_t::discovery, &dbimport));
    }

  return 0;
  }

