#pragma once
// #define SPDLOG_USE_STD_FORMAT
#include <filesystem>
#include <functional>
#include <string_view>
#include <stop_token>
namespace fs = std::filesystem;

using process_callback = std::function<void(std::string_view)>;

[[nodiscard]]
auto find_all_journals(fs::path const & dir) -> std::vector<fs::path>;
[[nodiscard]]
auto find_latest_journal(fs::path const & dir) -> std::optional<fs::path>;
auto tail_file(fs::path const & path, process_callback const & cb, std::stop_token stoken) -> void;

using journal_switch_callback = std::function<void(fs::path const &)>;

///\brief called the moment the reader reaches the end of what was already written
///\detail everything before that point is the past being replayed - the ship has since moved, the
/// target has been let go, the game may have been closed. Everything after it is happening now.
/// Nothing else in the stream tells the two apart, and the difference decides what may be believed
using caught_up_callback = std::function<void()>;

///\brief follows the newest journal in the directory and switches to a newer one when the game creates it
///\detail restarting the game closes the old journal and starts a new one; following the file alone hangs on a
/// log that is no longer used. on_switch is called from the following thread on every change of file.
auto tail_journal_dir(
  fs::path const & dir,
  process_callback const & cb,
  std::stop_token stoken,
  journal_switch_callback const & on_switch = {},
  caught_up_callback const & on_caught_up = {}
) -> void;
auto read_file(fs::path const & path, process_callback const & cb) -> void;

