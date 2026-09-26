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

///\brief follows the newest journal in the directory and switches to a newer one when the game creates it
///\detail restarting the game closes the old journal and starts a new one; following the file alone hangs on a
/// log that is no longer used. on_switch is called from the following thread on every change of file.
auto tail_journal_dir(
  fs::path const & dir,
  process_callback const & cb,
  std::stop_token stoken,
  journal_switch_callback const & on_switch = {}
) -> void;
auto read_file(fs::path const & path, process_callback const & cb) -> void;

