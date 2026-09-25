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
namespace
  {
constexpr auto tail_poll_interval{std::chrono::milliseconds(50)};
constexpr auto journal_check_interval{std::chrono::seconds(2)};

///\brief czyta plik do konca i dalej sledzi dopisywane linie
///\param give_up sprawdzane po dojsciu do konca pliku, przerywa sledzenie gdy zwroci true
///\returns false gdy pliku nie udalo sie otworzyc
auto tail_until(
  fs::path const & path, process_callback const & cb, std::stop_token stoken, std::function<bool()> const & give_up
) -> bool
  {
  // Tryb "współdzielony" w systemach POSIX to standardowy fstream.
  // Na Windows można użyć specyficznych flag API, ale std::ifstream zazwyczaj wystarcza do odczytu logów.
  std::ifstream file(path, std::ios::in);
  if(!file.is_open())
    {
    std::println(stderr, "Błąd: Nie można otworzyć pliku {}", path.string());
    return false;
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
      continue;
      }

    file.clear();
    std::this_thread::sleep_for(tail_poll_interval);

    if(give_up and give_up())
      {
      // doczytujemy koncowke (np. Shutdown) zanim oddamy plik
      while(std::getline(file, line))
        cb(line);
      break;
      }
    }
  return true;
  }
  }  // namespace

auto tail_file(fs::path const & path, process_callback const & cb, std::stop_token stoken) -> void
  {
  static_cast<void>(tail_until(path, cb, stoken, {}));
  }

auto tail_journal_dir(
  fs::path const & dir, process_callback const & cb, std::stop_token stoken, journal_switch_callback const & on_switch
) -> void
  {
  while(not stoken.stop_requested())
    {
    auto latest{find_latest_journal(dir)};
    if(not latest)
      {
      // katalog gry moze byc jeszcze pusty
      std::this_thread::sleep_for(journal_check_interval);
      continue;
      }

    fs::path const current{std::move(*latest)};
    if(on_switch)
      on_switch(current);

    auto last_check{std::chrono::steady_clock::now()};

    // restart gry zaklada nowy plik, stary przestaje rosnac
    auto const newer_journal_available = [&dir, &current, &last_check]() -> bool
    {
      auto const now{std::chrono::steady_clock::now()};
      if(now - last_check < journal_check_interval)
        return false;
      last_check = now;

      auto newest{find_latest_journal(dir)};
      return newest and *newest != current;
    };

    if(not tail_until(current, cb, stoken, newer_journal_available))
      std::this_thread::sleep_for(journal_check_interval);
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
