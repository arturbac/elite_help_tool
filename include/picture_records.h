#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
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
  }  // namespace pictures
