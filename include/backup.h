#pragma once

#include <simple_enum/expected.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

///\brief a copy of what cannot be rebuilt - the journals, the codex's pictures, live.sqlite, and the settings of the
/// game and of the tool - kept on this machine
///
/// `ehtdb.sqlite` and `galaxy.sqlite` are rebuilt from the journals, and what the pictures show is
/// described again out of them, so the journals and the pictures are what a periodic backup packs. But
/// `live.sqlite` (markets, carrier cargo) holds readings the journals never carry verbatim - a market
/// overwrites the last one the moment you dock somewhere else - so it is copied whole instead, hot,
/// using SQLite's own backup API rather than a plain file copy. The journals are text that repeats
/// itself line after line and pack some seventy times smaller, one tar.zst a month: a month is packed
/// again only when a journal of it changed. The pictures are JPGs already and are copied as they are, and
/// nothing is ever deleted from the backup
namespace backup
  {
///\brief "~/.backups/eht" with the home directory in place of the tilde
[[nodiscard]]
auto expand_home(std::string_view path) -> std::filesystem::path;

///\brief a tar of the files, each under its own name without directories, compressed with zstd
///\detail written aside and put in place whole, so an interrupted run never leaves half an archive under the name.
/// The window stays within what zstd reads by default, so a plain `tar -xaf` unpacks it
[[nodiscard]]
auto write_tar_zst(std::filesystem::path const & archive, std::span<std::filesystem::path const> files, int level)
  -> bool;

struct summary_t
  {
  ///\brief the months packed now, as 2026-09
  std::vector<std::string> months;
  size_t pictures_copied{};
  bool live_db_copied{};
  std::vector<std::string> errors;
  };

///\brief a hot copy of a live sqlite database into "live.sqlite" under destination, safe to take while
/// another connection has it open (SQLite's own backup API, not a plain file copy)
[[nodiscard]]
auto backup_live_db(std::filesystem::path const & destination, std::filesystem::path const & live_db_path) -> bool;

///\brief packs the months whose journals changed since their archive, copies the pictures not in the
/// backup yet, and refreshes the copy of live_db_path
[[nodiscard]]
auto run(
  std::filesystem::path const & destination,
  std::filesystem::path const & journal_dir,
  std::filesystem::path const & codex_dir,
  std::filesystem::path const & live_db_path,
  int level
) -> summary_t;

///\brief the settings nothing rebuilds - the game's own and the tool's - and where each lies; an empty path is
/// skipped
struct settings_sources_t
  {
  ///\brief the game's Options directory: Bindings, Graphics, Player, Audio
  std::filesystem::path options_dir;
  ///\brief the game's own directory, Products/<product> - of it only what kept_from_game names
  std::filesystem::path game_dir;
  ///\brief eht_settings.json
  std::filesystem::path tool_settings;
  };

struct settings_summary_t
  {
  size_t copied{};
  std::vector<std::string> errors;
  };

///\brief the files of the game's own directory a verification of the game's files would delete or put back as
/// shipped: AppConfigLocal.xml, GraphicsConfiguration.xml, and the mods' .ini files. Never the binaries - a mod's
/// DLL comes again from where it was built or downloaded
[[nodiscard]]
auto kept_from_game(std::filesystem::path const & file_name) -> bool;

///\brief a log of a game session gone by, set aside by edworld when the next session began: edworld.<UTC>.log, or
/// edworld_eht.<UTC>.log from the build that works with EHT. Never edworld.log or edworld_eht.log - the running
/// session writes it
[[nodiscard]]
auto closed_mod_log(std::filesystem::path const & file_name) -> bool;

struct mod_logs_summary_t
  {
  size_t moved{};
  std::vector<std::string> errors;
  };

///\brief each closed log of the game's own directory compressed (zstd) into "mod-logs" under destination and, once
/// the copy reads back the same, deleted from the game's directory - a verification of the game's files would
/// delete it anyway, and a week of them would be lost with it
[[nodiscard]]
auto move_mod_logs(std::filesystem::path const & destination, std::filesystem::path const & game_dir, int level)
  -> mod_logs_summary_t;

///\brief the game's own directory out of the netLog's: Products/<product>/Logs; empty when it is not one
[[nodiscard]]
auto game_dir_of(std::filesystem::path const & netlog_dir) -> std::filesystem::path;

///\brief copies the settings into "settings" under destination - "options", "game" and eht_settings.json - each
/// file when missing or newer. The copy it replaces is kept beside it under the name with its own time appended,
/// so a setting broken later never overwrites the last good one, and nothing is ever deleted
[[nodiscard]]
auto copy_settings(std::filesystem::path const & destination, settings_sources_t const & sources) -> settings_summary_t;

///\brief what the last backup was, kept beside it
struct mark_t
  {
  std::chrono::sys_seconds at{};
  ///\brief how many pictures the codex held then
  uint64_t pictures{};
  };

///\brief the mark of the last backup - an empty one when there was none yet; an error when the file is
/// there but cannot be read, which the caller says, since a backup then follows at every look
[[nodiscard]]
auto read_mark(std::filesystem::path const & destination) -> cxx23::expected<mark_t, std::error_code>;
[[nodiscard]]
auto write_mark(std::filesystem::path const & destination, mark_t const & mark) -> cxx23::expected<void, std::error_code>;

///\brief the pictures of the codex and the sky album - what the backup counts the new ones of; none when
/// the directory is not there yet
[[nodiscard]]
auto count_pictures(std::filesystem::path const & codex_dir) -> cxx23::expected<uint64_t, std::error_code>;

///\brief a backup is due once the days have passed since the last, or once as many new pictures came in -
/// whichever comes first; 0 turns either off
[[nodiscard]]
auto due(mark_t const & last, std::chrono::sys_seconds now, uint64_t pictures, uint32_t every_days, uint32_t every_pictures)
  -> bool;
  }  // namespace backup
