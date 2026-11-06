#pragma once

#include <biology.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief what the pictures of the codex and the sky album show, and how that is found again after a loss
///
/// The pictures cannot be rebuilt, but what they show can: every file is named after the journal's
/// timestamp of the moment it was taken for - the sample, the jump, the scanner - and the journals hold
/// everything else. So the lists beside the pictures are a convenience, not the only record: a list lost
/// or broken, or a picture not in it, is described again out of the journals
namespace pictures
  {
///\brief a name fit for a file - letters, digits and dashes, nothing a shell or a browser would stumble on
[[nodiscard]]
auto file_safe(std::string_view text) -> std::string;

///\brief the start of a picture's file name - the journal's moment, then what it shows: 20260928-223512_Name
[[nodiscard]]
auto stem(std::chrono::sys_seconds at, std::string_view name) -> std::string;

///\brief the line under a star's picture
[[nodiscard]]
auto star_detail(
  std::string_view star_type,
  uint32_t subclass,
  std::string_view luminosity,
  double solar_masses,
  double temperature_k,
  double radius_m
) -> std::string;

///\brief the line under a planet's picture
[[nodiscard]]
auto planet_detail(std::string_view planet_class, std::string_view atmosphere, double gravity_ms2, double temperature_k)
  -> std::string;

///\brief a picture of a sample, as the codex lists it
struct codex_record_t
  {
  std::string file;
  std::string taken;
  std::string system;
  std::string body;
  uint64_t system_address{};
  uint32_t body_id{};
  std::string genus;
  std::string species;
  std::string variant;
  std::string scan;
  };

///\brief a picture of a star or a planet, as the sky album lists it
struct sky_record_t
  {
  std::string file;
  std::string taken;
  std::string kind;
  std::string system;
  std::string body;
  std::string detail;
  bool first{};
  };

///\brief the pictures given, relative to the codex directory, described out of the journals
///\detail a picture whose moment no journal has keeps what its name says, so it is listed all the same
[[nodiscard]]
auto rebuild_codex(std::vector<std::string> const & files, std::filesystem::path const & journal_dir)
  -> std::vector<codex_record_t>;

[[nodiscard]]
auto rebuild_sky(std::vector<std::string> const & files, std::filesystem::path const & journal_dir)
  -> std::vector<sky_record_t>;

///\brief the pictures under a directory of the codex, relative to the codex directory, in the order of their names
[[nodiscard]]
auto pictures_under(std::filesystem::path const & codex_dir, std::string_view sub) -> std::vector<std::string>;

///\brief the list of samples' pictures in a codex directory - none when it is missing or cannot be read
[[nodiscard]]
auto read_codex_list(std::filesystem::path const & codex_dir) -> std::vector<codex_record_t>;

///\brief the sky album's list in a codex directory - none when it is missing or cannot be read
[[nodiscard]]
auto read_sky_list(std::filesystem::path const & codex_dir) -> std::vector<sky_record_t>;

///\brief the finds an account left in its codex directory for the others - none when it has not yet
[[nodiscard]]
auto read_finds(std::filesystem::path const & codex_dir) -> std::vector<bio::find_t>;

///\brief leaves the finds in the codex directory for the other accounts, whose own databases are out of reach
///\detail a list that did not change is not touched, so accounts watching each other's lists do not keep each
/// other writing
auto write_finds(std::filesystem::path const & codex_dir, std::span<bio::find_t const> finds) -> bool;

///\brief a file of another codex directory as a page in this one reaches it
[[nodiscard]]
auto seen_from(std::filesystem::path const & own_dir, std::filesystem::path const & other_dir, std::string_view file)
  -> std::string;

///\brief a find with the accounts that made it
struct signed_find_t
  {
  bio::find_t find;
  std::string who;
  };

///\brief adds an account's finds, in the order the codex lists them
///\detail two accounts on the same body and species are one find, signed by both, and analysed if either did
auto merge_finds(std::vector<signed_find_t> & into, std::span<bio::find_t const> from, std::string_view who) -> void;

///\brief when a codex directory's lists last changed - what tells a page there is something new to show
[[nodiscard]]
auto lists_stamp(std::filesystem::path const & codex_dir) -> std::filesystem::file_time_type;
  }  // namespace pictures
