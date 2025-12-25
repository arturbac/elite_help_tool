#include <file_io.h>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>
#include <ranges>
#include <fstream>
#include <thread>
#include <chrono>
#include <print>


[[nodiscard]]
auto find_latest_journal(fs::path const & dir) -> std::optional<fs::path>
  {
  if(!fs::exists(dir) || !fs::is_directory(dir))
    return std::nullopt;

  auto journals
    = fs::directory_iterator{dir}
      | std::views::filter([](auto const & entry)
                           { return entry.is_regular_file() && entry.path().filename().string().contains("Journal"); })
      | std::views::transform([](auto const & entry) { return entry.path(); })
      | std::ranges::to<std::vector<fs::path>>();

  if(journals.empty())
    return std::nullopt;

  // Sortowanie leksykograficzne nazw plików (ISO 8601 w nazwie to gwarantuje)
  std::ranges::sort(journals);
  return journals.back();
  }

auto tail_file(fs::path const & path, process_callback const & cb) -> void
  {
  // Tryb "współdzielony" w systemach POSIX to standardowy fstream.
  // Na Windows można użyć specyficznych flag API, ale std::ifstream zazwyczaj wystarcza do odczytu logów.
  std::ifstream file(path, std::ios::in);
  if(!file.is_open())
    {
    std::println(stderr, "Błąd: Nie można otworzyć pliku {}", path.string());
    return;
    }

  std::string line;
  // Najpierw przeczytaj całą obecną zawartość
  while(std::getline(file, line))
    // std::println("{}", line);
    cb(line);
  file.clear();  // Czyścimy flagę EOF, aby móc czytać dalej

  // Pętla monitorująca zmiany
  while(true)
    {
    if(std::getline(file, line))
      {
      // std::println("{}", line);
      cb(line);
      }
    else if(file.eof())
      {
      file.clear();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }
    }
  }

