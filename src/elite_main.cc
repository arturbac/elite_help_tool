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

namespace fs = std::filesystem;
namespace po = boost::program_options;

struct config_t
  {
  fs::path directory;
  };

[[nodiscard]]
auto main(int argc, char ** argv) -> int
  {
  po::options_description desc("Opcje");
  desc
    .add_options()("help,h", "Wyświetl pomoc")("dir,d", po::value<std::string>()->default_value("."), "journal folder")(
      "file,f", po::value<std::string>()->default_value("."), "file for explicit load"
    );

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

  spdlog::set_level(spdlog::level::debug);
  auto const path = fs::path{vm["dir"].as<std::string>()};
  std::optional<fs::path> latest;

  if(vm.count("file"))
    latest = fs::path{vm["file"].as<std::string>()};
  else
    latest = find_latest_journal(path);
  std::stop_source stop_source;
  if(latest)
    {
    std::println("Listning to file: {}", latest->string());
    state_t state{};
    discovery_state_t monitor;
    monitor.state = &state;
    tail_file(*latest, std::bind_front(&generic_state_t::discovery, &monitor), stop_source.get_token());
    }
  else
    {
    std::println("Nie znaleziono pliku Journal w {}", path.string());
    }

  return 0;
  }

