#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief a copy of what cannot be rebuilt - the journals and the codex's pictures - kept on this machine
///
/// The databases are rebuilt from the journals, and what the pictures show is described again out of
/// them, so the journals and the pictures are all there is to keep. The journals are text that repeats
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
  std::vector<std::string> errors;
  };

///\brief packs the months whose journals changed since their archive, and copies the pictures not in the backup yet
[[nodiscard]]
auto run(
  std::filesystem::path const & destination,
  std::filesystem::path const & journal_dir,
  std::filesystem::path const & codex_dir,
  int level
) -> summary_t;

///\brief what the last backup was, kept beside it
struct mark_t
  {
  std::chrono::sys_seconds at{};
  ///\brief how many pictures the codex held then
  uint64_t pictures{};
  };

[[nodiscard]]
auto read_mark(std::filesystem::path const & destination) -> mark_t;
auto write_mark(std::filesystem::path const & destination, mark_t const & mark) -> void;

///\brief the pictures of the codex and the sky album - what the backup counts the new ones of
[[nodiscard]]
auto count_pictures(std::filesystem::path const & codex_dir) -> uint64_t;

///\brief a backup is due once the days have passed since the last, or once as many new pictures came in -
/// whichever comes first; 0 turns either off
[[nodiscard]]
auto due(mark_t const & last, std::chrono::sys_seconds now, uint64_t pictures, uint32_t every_days, uint32_t every_pictures)
  -> bool;
  }  // namespace backup
