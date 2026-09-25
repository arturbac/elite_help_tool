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

///\brief sledzi najnowszy journal w katalogu i przelacza sie na nowszy gdy gra go utworzy
///\detail restart gry zamyka stary journal i zaklada nowy, sledzenie samego pliku zawisa na
/// nieuzywanym juz logu. on_switch jest wolane z watku sledzacego przy kazdej zmianie pliku.
auto tail_journal_dir(
  fs::path const & dir,
  process_callback const & cb,
  std::stop_token stoken,
  journal_switch_callback const & on_switch = {}
) -> void;
auto read_file(fs::path const & path, process_callback const & cb) -> void;

