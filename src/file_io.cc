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


auto find_all_journals(fs::path const & dir) -> std::vector<fs::path>
  {
  if(!fs::exists(dir) || !fs::is_directory(dir))
    return {};

  auto journals
    = fs::directory_iterator{dir}
      | std::views::filter([](auto const & entry)
                           { return entry.is_regular_file() && entry.path().filename().string().contains("Journal"); })
      | std::views::transform([](auto const & entry) { return entry.path(); })
      | std::ranges::to<std::vector<fs::path>>();

  if(journals.empty())
    return journals;

  // Sortowanie leksykograficzne nazw plików (ISO 8601 w nazwie to gwarantuje)
  std::ranges::sort(journals);
  return journals;
  }

auto find_latest_journal(fs::path const & dir) -> std::optional<fs::path>
{
  auto journals{find_all_journals(dir)};
  if(journals.empty())
    return std::nullopt;
  return journals.back();
}
auto tail_file(fs::path const & path, process_callback const & cb, std::stop_token stoken) -> void
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
  while(not stoken.stop_requested())
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
auto read_file(fs::path const & path, process_callback const & cb) -> void
  {
  std::ifstream file(path, std::ios::in);
  if(!file.is_open())
    {
    std::println(stderr, "Błąd: Nie można otworzyć pliku {}", path.string());
    return;
    }

  std::string line;
  // Najpierw przeczytaj całą obecną zawartość
  while(std::getline(file, line))
    cb(line);
  }
