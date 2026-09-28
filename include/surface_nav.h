#pragma once

#include <biology.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

///\brief the way to a point on a body's surface, from where Status.json says we stand
///
/// The game leads to stations and settlements, but a tip-off or a guide gives a place as two numbers, and
/// the game has no way to take them. The target is typed in by hand; everything else - where we are, which
/// way we face, how big the body is - the game writes several times a second
namespace nav
  {
///\brief a place to go to, on the body it lies on
struct target_t
  {
  ///\brief the body's full name, as Status.json gives it
  std::string body;
  bio::surface_point_t point;
  ///\brief what the commander called it, if anything
  std::string label;
  };

///\brief two numbers read out of whatever the text around them is
///\detail "12.34, -56.78", "12,34 -56,78" (decimal commas), "Lat: 12.34 Lon: -56.78", "12.34 N 56.78 W",
/// "12.34°S, 56.78°E". The latitude comes first unless the letters say otherwise; a longitude past 180 is
/// taken as the same meridian counted the other way round
[[nodiscard]]
auto parse_point(std::string_view text) -> std::optional<bio::surface_point_t>;

///\brief where the target lies from here
struct guidance_t
  {
  double distance_m{};
  ///\brief degrees clockwise from north, the way the game counts Heading
  double bearing_deg{};
  ///\brief how far to turn, -180..180, positive to the right - absent when the heading is not known
  std::optional<double> turn_deg;
  };

[[nodiscard]]
auto guide(bio::surface_point_t here, std::optional<double> heading_deg, bio::surface_point_t target, double radius_m)
  -> guidance_t;

///\brief "850 m", "3.42 km", "128 km" - as precise as the distance deserves
[[nodiscard]]
auto format_distance(double metres) -> std::string;

///\brief "ahead", "42° right", "behind" - a turn said the way it is read at a glance
[[nodiscard]]
auto format_turn(double turn_deg) -> std::string;

///\brief "40 s", "3 min", "1 h 20 min"
[[nodiscard]]
auto format_duration(std::chrono::seconds duration) -> std::string;

///\brief how fast we move over the ground, from the last few seconds of positions
///\detail the positions come a few times a second while moving and not at all while standing, so the
/// speed is the way covered over a short window rather than from one step to the next, which a single
/// late write of the file would turn into a sprint or a halt
class ground_speed_t
  {
public:
  using clock = std::chrono::steady_clock;

  auto push(clock::time_point at, bio::surface_point_t point, double radius_m) -> void;

  ///\brief metres a second, once the window holds enough time to say
  [[nodiscard]]
  auto metres_per_second(clock::time_point now) const -> std::optional<double>;

  auto clear() -> void { samples_.clear(); }

private:
  struct sample_t
    {
    clock::time_point at;
    bio::surface_point_t point;
    };

  std::deque<sample_t> samples_;
  double radius_m_{};
  };

///\brief something the codex logged at a place on a body - a species, a geological feature, a ship's wreck
struct codex_point_t
  {
  std::string name;
  std::string category;
  bio::surface_point_t point;
  ///\brief the journal's timestamp, as written
  std::string seen;
  ///\brief the first time anyone of the accounts logged it at all
  bool first{};
  };

///\brief the codex entries of every body, read out of the journals
///\detail CodexEntry names the body by its number only, so the body's name comes from the approach and the
/// touchdown on it, which name both. The first update reads every journal, and each one after that only
/// what was added since - the archive runs to gigabytes, and a new line is all that changes
class codex_index_t
  {
public:
  auto update(std::filesystem::path const & journal_dir) -> void;

  ///\brief the entries on the body of this name, the oldest first
  [[nodiscard]]
  auto on_body(std::string const & body_name) const -> std::vector<codex_point_t>;

private:
  using body_key_t = std::pair<uint64_t, uint32_t>;

  ///\brief how much of each journal has been read, whole lines only
  std::map<std::filesystem::path, std::uintmax_t> read_;
  std::map<std::string, body_key_t> bodies_;
  std::map<body_key_t, std::vector<codex_point_t>> points_;

  auto read_line(std::string const & line) -> void;
  };
  }  // namespace nav
