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
    "FID konta do ktorego nalezy baza; puste = konto z najnowszego journala, 'all' = bez rozroznienia");

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

