#include <eht_settings.h>
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


auto find_all_journals(fs::path const & dir, std::error_code & ec) -> std::vector<fs::path>
  {
  ec.clear();
  // not there yet is no error - the game may not have made its directory
  if(not fs::is_directory(dir, ec))
    {
    if(ec == std::errc::no_such_file_or_directory)
      ec.clear();
    return {};
    }

  // stepped with the error code: a range over the iterator steps with the throwing operator++
  std::vector<fs::path> journals;
  for(fs::directory_iterator it{dir, ec}, end{}; not ec and it != end; it.increment(ec))
    if(std::error_code type_ec; it->is_regular_file(type_ec) and it->path().filename().string().contains("Journal"))
      journals.push_back(it->path());

  // a lexicographic sort of the file names (the ISO 8601 in the name guarantees it works)
  std::ranges::sort(journals);
  return journals;
  }

auto find_all_journals(fs::path const & dir) -> std::vector<fs::path>
  {
  std::error_code ec;
  auto journals{find_all_journals(dir, ec)};
  if(ec)
    std::println(stderr, "error: the journals in {} could not all be listed: {}", dir.string(), ec.message());
  return journals;
  }

auto find_latest_journal(fs::path const & dir, std::error_code & ec) -> std::optional<fs::path>
  {
  auto journals{find_all_journals(dir, ec)};
  if(journals.empty())
    return std::nullopt;
  return journals.back();
  }
namespace
  {
[[nodiscard]]
auto tail_poll_interval() -> std::chrono::milliseconds
  { return std::chrono::milliseconds{eht::settings()->journal.tail_poll_ms}; }
[[nodiscard]]
auto journal_check_interval() -> std::chrono::seconds
  { return std::chrono::seconds{eht::settings()->journal.new_file_check_s}; }

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
    std::this_thread::sleep_for(tail_poll_interval());

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
  // looked at every few seconds, so a directory that cannot be listed is said once, and once when it can again
  bool listing_failed{};
  auto const latest_journal = [&dir, &listing_failed]() -> std::optional<fs::path>
  {
    std::error_code ec;
    auto latest{find_latest_journal(dir, ec)};
    if(ec and not listing_failed)
      std::println(stderr, "error: the journals in {} cannot be listed: {}", dir.string(), ec.message());
    else if(not ec and listing_failed)
      std::println(stderr, "the journals in {} can be listed again", dir.string());
    listing_failed = bool(ec);
    return latest;
  };

  while(not stoken.stop_requested())
    {
    auto latest{latest_journal()};
    if(not latest)
      {
      // the game's directory can still be empty
      std::this_thread::sleep_for(journal_check_interval());
      continue;
      }

    fs::path const current{std::move(*latest)};
    if(on_switch)
      on_switch(current);

    auto last_check{std::chrono::steady_clock::now()};

    // restarting the game starts a new file, the old one stops growing
    auto const newer_journal_available = [&latest_journal, &current, &last_check]() -> bool
    {
      auto const now{std::chrono::steady_clock::now()};
      if(now - last_check < journal_check_interval())
        return false;
      last_check = now;

      auto newest{latest_journal()};
      return newest and *newest != current;
    };

    if(not tail_until(current, cb, stoken, newer_journal_available, on_caught_up))
      std::this_thread::sleep_for(journal_check_interval());
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
