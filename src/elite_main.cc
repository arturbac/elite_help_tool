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

namespace fs = std::filesystem;
namespace po = boost::program_options;

struct config_t
  {
  fs::path directory;
  };

[[nodiscard]]
auto main(int argc, char ** argv) -> int
  {
  // spdlog::set_level(spdlog::level::debug);

  po::options_description desc("Opcje");
  desc
    .add_options()("help,h", "Wyświetl pomoc")("dir,d", po::value<std::string>()->default_value("."),
                                               "journal folder")
    ;

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

  if(fs::exists("ehtdb.sqlite"))
    fs::remove("ehtdb.sqlite");
  database_import_state_t dbimport;
  database_import_state_t::state_t state{"ehtdb.sqlite"};
  if(not state.db_.open())
    return EXIT_FAILURE;
  dbimport.state = &state;
  std::vector<fs::path> journals{find_all_journals(path)};
  for(fs::path const & p: journals)
    {
    spdlog::info("Importing file: {}", p.string());
    read_file(p, std::bind_front(&generic_state_t::discovery, &dbimport));
    }

  return 0;
  }

