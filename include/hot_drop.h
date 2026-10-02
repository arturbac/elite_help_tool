#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief the hot drop: flying into a port in supercruise faster than the game likes, and letting Supercruise Assist
/// drop out at the port all the same
///
/// The time to the target on the HUD says how fast the ship flies: 0:07 is how the assist flies by itself, 0:06
/// the least without overspeed, and 0:05 or less is overspeed. Under the throttle at 75% the assist still drops
/// out 1 Mm from the port when the ship is not too fast there - and how fast is too fast depends on the gravity
/// around the port and on how the ship turns. Too early into overspeed, and the assist flies past the port.
///
/// Neither the journal nor Status.json says the distance or the time to the target, so both are read off the
/// label the HUD writes beside the target's marker. Every approach to a port in space is written down with the
/// readings, the place and the ship, and how it ended - the material to tell later from how far a hot drop
/// works by a body and in a ship. See doc/hot_drop.md
namespace hot_drop
  {
inline constexpr double mm_per_ls{299.792458};

///\brief the distance the HUD writes, in light seconds: 238Ls, 1.32Ls, 1.06Mm, 530km. The reader of the text
/// now and then takes a 5 for an S, a 0 for an O or a 1 for an I - mended inside the number
[[nodiscard]]
auto parse_distance_ls(std::string_view text) -> std::optional<double>;

///\brief the time to the target the HUD writes, in seconds: 0:05, 1:45, 1:02:03
[[nodiscard]]
auto parse_seconds(std::string_view text) -> std::optional<uint32_t>;

///\brief how alike two names are, 0..1 - letters and digits only, upper-cased, by the edits between them
[[nodiscard]]
auto name_likeness(std::string_view a, std::string_view b) -> double;

///\brief a line of text found on a picture, with its box in pixels
struct text_line_t
  {
  std::string text;
  int32_t left{};
  int32_t top{};
  int32_t right{};
  int32_t bottom{};
  };

///\brief what the label beside the target's marker says
struct label_t
  {
  double distance_ls{};
  ///\brief the time to the target - missing when it could not be read
  std::optional<uint32_t> seconds;
  ///\brief where the two lines were found - a second, closer reading of each one alone mends what the first
  /// reading of the whole picture got wrong; the time's box is empty when there was no line for it
  text_line_t distance_box;
  text_line_t seconds_box;
  };

///\brief the label of the target among the lines read off a picture: its name, the distance right under it and
/// the time under that - none when the name is not there
[[nodiscard]]
auto read_label(std::span<text_line_t const> lines, std::string_view target) -> std::optional<label_t>;

///\brief one reading of the label, at the moment the picture was asked for
struct reading_t
  {
  uint64_t ms{};
  double distance_ls{};
  ///\brief the time to the target, -1 when it could not be read
  int32_t seconds{-1};
  ///\brief the time as the reader of the text gave it, before it was checked against the distances
  int32_t read_seconds{-1};
  };

///\brief the time to the target the readings say: the distance by the speed the distance fell at over the last
/// seconds - none until there are readings far enough apart, or when the ship does not close in
[[nodiscard]]
auto estimate_seconds(std::span<reading_t const> earlier, uint64_t ms, double distance_ls) -> std::optional<double>;

///\brief the time read checked against the one the distances say. The reader of the text takes the leading 0 for a
/// 6 or an 8 and a 1 for a 4 - the minutes are taken as the estimate has them, the seconds as read; a time
/// still far from the estimate is replaced by it. Without an estimate the time read stays
[[nodiscard]]
auto checked_seconds(int32_t read, std::optional<double> estimate) -> int32_t;

///\brief the port flown to, and what was known of its surroundings and of the ship when the approach began
struct context_t
  {
  std::string system;
  uint64_t system_address{};
  std::string station;
  uint64_t market_id{};
  std::string station_type;
  ///\brief the body the port circles, as the system map attaches it: by the same distance from the star.
  /// Empty when it is not known - a carrier, a port never docked at, a body never scanned
  std::string body;
  ///\brief star, planet or moon
  std::string body_kind;
  double body_mass_em{};
  double body_radius_km{};
  ///\brief at the surface, in g
  double body_gravity_g{};
  ///\brief how far the port lies from the body, by the difference of their distances from the star - rough
  double port_from_body_ls{};
  std::string ship;
  uint32_t ship_id{};
  std::string ship_name;
  ///\brief the ship with its fuel and cargo, in tons - 0 when the loadout was not seen
  double ship_mass_t{};
  };

enum struct outcome_e : uint8_t
  {
  ///\brief the assist dropped out at the port
  dropped,
  ///\brief the port was passed in supercruise - the distance grew again right by it
  overshot,
  ///\brief anything else: another destination, out of supercruise elsewhere, turned away
  broken_off
  };

[[nodiscard]]
auto outcome_name(outcome_e outcome) noexcept -> std::string_view;

[[nodiscard]]
auto outcome_of(std::string_view name) noexcept -> std::optional<outcome_e>;

///\brief one approach to a port, from the first reading to its end
struct attempt_t
  {
  context_t where;
  uint64_t started_ms{};
  uint64_t ended_ms{};
  std::vector<reading_t> readings;
  outcome_e outcome{outcome_e::broken_off};
  };

///\brief what an approach was, out of its readings
struct summary_t
  {
  ///\brief the time to the target was 0:05 or less on the way - overspeed
  bool overspeed{};
  ///\brief the distance at the first reading of the last way into overspeed - the one the outcome tells of
  double overspeed_from_ls{};
  ///\brief the distance at the first reading in overspeed at all, and how many times the ship went into it - more
  /// than once when it turned to lose speed and went in again nearer
  double first_overspeed_ls{};
  uint32_t overspeed_entries{};
  ///\brief the least time to the target read on the way, -1 none
  int32_t least_seconds{-1};
  ///\brief how fast the ship flew at the last readings, Mm/s - by the distance they differ in, 0 unknown
  double end_speed_mm_s{};
  ///\brief the last distance read
  double last_ls{};
  };

///\brief the readings this near the port are the drop itself, not the flight to it - the time to the target there
/// says nothing of overspeed
inline constexpr double drop_zone_ls{0.05};

[[nodiscard]]
auto summarise(attempt_t const & attempt) -> summary_t;

///\brief follows the approaches to a port, one at a time, and hands each one over when it ended
///\detail an approach ends with SupercruiseDestinationDrop at the port, or with the distance growing right by the
/// port, or with anything else. The status file may say supercruise is over before the journal says why, so the
/// end waits a moment for the drop
class tracker_t
  {
public:
  ///\brief how long the end waits for the journal's word of the drop
  static constexpr uint64_t drop_wait_ms{4000u};
  ///\brief the distance must grow this much over the least one, in two readings in a row, to be a port passed
  static constexpr double overshoot_margin_ls{0.05};
  ///\brief and the least one must have been this near - further away it is turning away, not passing it
  static constexpr double overshoot_within_ls{0.5};
  static constexpr size_t max_readings{4000u};

  ///\brief now in supercruise towards this port, or not - port missing
  auto approach(uint64_t now_ms, std::optional<context_t> const & port) -> std::optional<attempt_t>;

  ///\brief a reading of the label of the port flown to - its time checked against the distances before
  auto reading(reading_t const & given) -> std::optional<attempt_t>;

  ///\brief the journal said the ship dropped out at a destination
  auto dropped(uint64_t now_ms, std::string_view name, uint64_t market_id) -> std::optional<attempt_t>;

  ///\brief an approach is followed now
  [[nodiscard]]
  auto active() const noexcept -> bool
    { return attempt_.has_value() and not ending_since_; }

  ///\brief the port of the approach followed, null for none
  [[nodiscard]]
  auto port() const noexcept -> context_t const *
    { return attempt_ ? &attempt_->where : nullptr; }

  ///\brief the last distance read, when any
  [[nodiscard]]
  auto last_distance() const noexcept -> std::optional<double>;

private:
  auto finish(uint64_t now_ms, outcome_e outcome) -> std::optional<attempt_t>;

  std::optional<attempt_t> attempt_;
  std::optional<uint64_t> ending_since_;
  double least_ls_{};
  uint32_t growing_{};
  };

///\brief the line of an approach in hot_drop.jsonl
[[nodiscard]]
auto attempt_line(attempt_t const & attempt) -> std::string;

///\brief an approach read back from its line, enough to advise with
struct record_t
  {
  std::string station;
  uint64_t market_id{};
  std::string ship;
  outcome_e outcome{outcome_e::broken_off};
  summary_t summary;
  };

[[nodiscard]]
auto parse_attempt_line(std::string const & line) -> std::optional<record_t>;

[[nodiscard]]
auto record_of(attempt_t const & attempt) -> record_t;

///\brief what the approaches so far say of hot drops at one port in one kind of ship
struct advice_t
  {
  ///\brief the furthest overspeed that still dropped at the port, with its least time to the target
  std::optional<double> dropped_from_ls;
  int32_t dropped_seconds{-1};
  uint32_t dropped{};
  ///\brief the nearest overspeed that passed the port, with its least time to the target
  std::optional<double> overshot_from_ls;
  int32_t overshot_seconds{-1};
  uint32_t overshot{};
  };

///\brief the port is matched by its MarketID, or by its name when either has none
[[nodiscard]]
auto advise(std::span<record_t const> records, std::string_view station, uint64_t market_id, std::string_view ship)
  -> advice_t;
  }  // namespace hot_drop
