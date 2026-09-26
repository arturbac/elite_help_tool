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

  // a lexicographic sort of the file names (the ISO 8601 in the name guarantees it works)
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

///\brief reads the file to its end and goes on following the lines appended to it
///\param give_up checked once the end of the file is reached; following stops when it returns true
///\returns false when the file could not be opened
auto tail_until(
  fs::path const & path,
  process_callback const & cb,
  std::stop_token stoken,
  std::function<bool()> const & give_up,
  caught_up_callback const & on_caught_up
) -> bool
  {
  // the "shared" mode on POSIX systems is the standard fstream.
  // on Windows one can use specific API flags, but std::ifstream usually suffices for reading logs.
  std::ifstream file(path, std::ios::in);
  if(!file.is_open())
    {
    std::println(stderr, "error: cannot open file {}", path.string());
    return false;
    }

  std::string line;
  // read everything that is already there first
  while(std::getline(file, line))
    // std::println("{}", line);
    cb(line);
  file.clear();  // clear the EOF flag so that reading can go on

  // from here on the lines arrive as the game writes them; everything above was the past
  if(on_caught_up)
    on_caught_up();

  // the loop that watches for changes
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
      // read the tail (Shutdown, for instance) before handing the file back
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
  static_cast<void>(tail_until(path, cb, stoken, {}, {}));
  }

auto tail_journal_dir(
  fs::path const & dir,
  process_callback const & cb,
  std::stop_token stoken,
  journal_switch_callback const & on_switch,
  caught_up_callback const & on_caught_up
) -> void
  {
  while(not stoken.stop_requested())
    {
    auto latest{find_latest_journal(dir)};
    if(not latest)
      {
      // the game's directory can still be empty
      std::this_thread::sleep_for(journal_check_interval);
      continue;
      }

    fs::path const current{std::move(*latest)};
    if(on_switch)
      on_switch(current);

    auto last_check{std::chrono::steady_clock::now()};

    // restarting the game starts a new file, the old one stops growing
    auto const newer_journal_available = [&dir, &current, &last_check]() -> bool
    {
      auto const now{std::chrono::steady_clock::now()};
      if(now - last_check < journal_check_interval)
        return false;
      last_check = now;

      auto newest{find_latest_journal(dir)};
      return newest and *newest != current;
    };

    if(not tail_until(current, cb, stoken, newer_journal_available, on_caught_up))
      std::this_thread::sleep_for(journal_check_interval);
    }
  }
auto read_file(fs::path const & path, process_callback const & cb) -> void
  {
  std::ifstream file(path, std::ios::in);
  if(!file.is_open())
    {
    std::println(stderr, "error: cannot open file {}", path.string());
    return;
    }

  std::string line;
  // read everything that is already there first
  while(std::getline(file, line))
    cb(line);
  }
