#pragma once

#include <eht_settings.h>
#include <overlay_protocol.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

///\brief the recorder of eht_vision: pictures of the screen, each with what the game's files said at that moment
///
/// The layer keeps a small copy of the middle screen in a shared file (overlay::sample_t); the recorder keeps a
/// picture of it a second, skipping those that are the same as the last one, and writes beside the pictures what
/// Status.json and the journal said - the labels a model learns from, which nobody ever has to type in.
/// Everything goes into one directory a UTC day:
///   <dataset>/YYYY-MM-DD/<taken_ms>.png  the picture
///   <dataset>/YYYY-MM-DD/frames.jsonl    a line a picture: where it lies on the screen, Status.json as it was
///   <dataset>/YYYY-MM-DD/status.jsonl    every new content of Status.json, with the moment it was seen
///   <dataset>/YYYY-MM-DD/events.jsonl    every event of the journal, with the moment it was seen
///   <dataset>/YYYY-MM-DD/<asked_ms>_<why>.jpg  the whole middle screen in full resolution, now and then and
///                                      after the state changed - the tool asks the layer for it
///   <dataset>/YYYY-MM-DD/shots.jsonl     a line each, Status.json as it was
namespace vision
  {
///\brief the sample's pixels as RGB, with the head the layer wrote
struct sample_read_t
  {
  overlay::sample_header_t header;
  std::vector<uint8_t> rgb;
  };

///\brief the head of the shared file says a sample is whole, new and of a size the file holds
///\detail before and after are the head read before and after the pixels - the layer writes seq odd while it
/// writes, so the pixels are good only when both are the same even number
[[nodiscard]]
auto sample_valid(
  overlay::sample_header_t const & before, overlay::sample_header_t const & after, uint64_t seen
) noexcept -> bool;

///\brief RGBA turned into RGB
[[nodiscard]]
auto rgb_of(std::span<uint8_t const> rgba) -> std::vector<uint8_t>;

///\brief the sample the layer wrote last, when it is newer than the one seen and whole
[[nodiscard]]
auto read_sample(std::filesystem::path const & path, uint64_t seen) -> std::optional<sample_read_t>;

inline constexpr uint32_t thumb_width{32u};
inline constexpr uint32_t thumb_height{18u};
using thumbnail_t = std::array<uint8_t, size_t{thumb_width} * thumb_height>;

///\brief the brightness of the picture shrunk to 32 x 18, each cell the mean of the pixels it covers
[[nodiscard]]
auto thumbnail(std::span<uint8_t const> rgb, uint32_t width, uint32_t height) -> thumbnail_t;

///\brief the mean difference of the brightness of two thumbnails, 0-255
[[nodiscard]]
auto difference(thumbnail_t const & a, thumbnail_t const & b) noexcept -> float;

///\brief decides which pictures are kept: at most one every every_ms, and only one that changed - but at least
/// one every keep_every_ms, and always one of another part of the screen
class keeper_t
  {
public:
  struct verdict_t
    {
    bool keep;
    ///\brief from the last one kept; a negative one when there is none to compare with
    float difference;
    };

  [[nodiscard]]
  auto judge(
    thumbnail_t const & thumb,
    overlay::sample_header_t const & header,
    uint32_t every_ms,
    float min_difference,
    uint32_t keep_every_ms
  ) const -> verdict_t;

  auto kept(thumbnail_t const & thumb, overlay::sample_header_t const & header) -> void;

private:
  std::optional<thumbnail_t> thumb_;
  overlay::sample_header_t header_{};
  };

///\brief the UTC day of a moment in milliseconds, YYYY-MM-DD - the name of its directory
[[nodiscard]]
auto day_name(uint64_t ms) -> std::string;

///\brief a day's directory and how much it holds
struct day_size_t
  {
  std::string name;
  uint64_t bytes;
  };

///\brief the days to delete, the oldest first, until the rest is not above the limit - never today
[[nodiscard]]
auto days_to_drop(std::vector<day_size_t> days, uint64_t limit_bytes, std::string_view today)
  -> std::vector<std::string>;

///\brief the day directories under the dataset, with their sizes - only names that are days
[[nodiscard]]
auto measure_days(std::filesystem::path const & dataset) -> std::vector<day_size_t>;

///\brief the line of a picture in frames.jsonl
struct frame_record_t
  {
  std::string file;
  overlay::sample_header_t header;
  ///\brief the brightness difference from the last picture kept, negative for the first one
  float difference;
  ///\brief who plays, from the journal, and the name of the layer's socket - each account has its own
  std::string commander;
  std::string socket;
  ///\brief when Status.json was seen as it is, 0 when it was not yet
  uint64_t status_ms;
  ///\brief its whole content, empty when it was not yet read
  std::string status;
  };

[[nodiscard]]
auto frame_line(frame_record_t const & record) -> std::string;

///\brief the line of a new content of Status.json in status.jsonl
[[nodiscard]]
auto status_line(uint64_t ms, std::string_view status, bool flags_changed) -> std::string;

///\brief the line of an event in events.jsonl - the game's timestamp has whole seconds, ms says when it was seen
[[nodiscard]]
auto event_line(uint64_t ms, std::string_view timestamp, std::string_view event) -> std::string;

///\brief what Status.json says that decides whether a picture's label is to be trusted
struct status_flags_t
  {
  uint64_t Flags{};
  uint64_t Flags2{};
  uint32_t GuiFocus{};

  auto operator==(status_flags_t const &) const -> bool = default;
  };

///\brief Status.json read when it is whole JSON - the game rewrites it in place, and now and then it is caught
/// half written or empty
[[nodiscard]]
auto parse_status(std::string_view text) -> std::optional<status_flags_t>;

///\brief an event of the journal: its name and its game timestamp
struct journal_event_t
  {
  std::string timestamp;
  std::string event;
  std::string Name;
  };

[[nodiscard]]
auto parse_event(std::string_view line) -> std::optional<journal_event_t>;

///\brief follows the newest Journal.*.log, line by line as the game writes them
///\detail the file there already at the start is followed from its end - its past is not what was on the screen
/// - but read through for the commander's name. A newer file is read from its beginning
class journal_tail_t
  {
public:
  explicit journal_tail_t(std::filesystem::path dir);

  ///\brief the whole lines written since the last call
  [[nodiscard]]
  auto poll() -> std::vector<std::string>;

  [[nodiscard]]
  auto commander() const noexcept -> std::string const &
    { return commander_; }

private:
  auto open_newest() -> void;
  auto note(std::string_view line) -> void;

  std::filesystem::path dir_;
  std::filesystem::path file_;
  uint64_t offset_{};
  std::string partial_;
  std::string commander_;
  std::chrono::steady_clock::time_point looked_{};
  bool first_{true};
  };

///\brief writes a PNG of RGB pixels; false when it could not
[[nodiscard]]
auto write_png(std::filesystem::path const & path, std::span<uint8_t const> rgb, uint32_t width, uint32_t height)
  -> bool;

///\brief a whole-screen picture the tool asked the layer for: <prefix><ms>_<why>.ppm
struct shot_name_t
  {
  uint64_t asked_ms;
  std::string reason;
  };

///\brief the moment and the reason out of a picture's file name, when it is one of this account's whole pictures
[[nodiscard]]
auto parse_shot_name(std::string_view file_name, std::string_view prefix) -> std::optional<shot_name_t>;

///\brief an RGB picture read from a file
struct picture_t
  {
  uint32_t width{};
  uint32_t height{};
  std::vector<uint8_t> rgb;
  };

///\brief a binary PPM as the layer writes it - P6, 8 bits
[[nodiscard]]
auto read_ppm(std::filesystem::path const & path) -> std::optional<picture_t>;

///\brief writes a JPEG of RGB pixels; false when it could not
[[nodiscard]]
auto write_jpeg(
  std::filesystem::path const & path, std::span<uint8_t const> rgb, uint32_t width, uint32_t height, uint32_t quality
) -> bool;

///\brief the line of a whole-screen picture in shots.jsonl
struct shot_record_t
  {
  std::string file;
  shot_name_t name;
  uint32_t width;
  uint32_t height;
  std::string commander;
  std::string socket;
  uint64_t status_ms;
  std::string status;
  };

[[nodiscard]]
auto shot_line(shot_record_t const & record) -> std::string;

///\brief the recorder itself, run until stopped
class recorder_t
  {
public:
  ///\param journal_dir where the journals and Status.json are
  ///\param sample_path the layer's shared file of the sample
  recorder_t(std::filesystem::path journal_dir, std::filesystem::path sample_path);

  auto run(std::stop_token stop) -> void;

  ///\brief one round: Status.json, the journal and the sample looked at once
  auto step(eht::vision_settings_t const & cfg, uint64_t now_ms) -> void;

private:
  auto append(std::string_view day, std::string_view file, std::string_view line) -> void;
  auto look_at_status(uint64_t now_ms) -> void;
  auto look_at_journal(uint64_t now_ms) -> void;
  auto look_at_sample(eht::vision_settings_t const & cfg, uint64_t now_ms) -> void;
  auto look_at_shots(eht::vision_settings_t const & cfg) -> void;
  auto counted(std::string const & day, std::filesystem::path const & file) -> void;
  auto make_room(eht::vision_settings_t const & cfg, std::string const & today) -> bool;

  std::filesystem::path journal_dir_;
  std::filesystem::path sample_path_;
  ///\brief where the layer writes the whole-screen pictures, and how this account's names begin
  std::filesystem::path spool_;
  std::string shot_prefix_;
  std::filesystem::path dataset_;
  std::string socket_;
  journal_tail_t journal_;
  keeper_t keeper_;
  uint64_t seen_seq_{};
  std::filesystem::file_time_type status_time_{};
  std::string status_;
  uint64_t status_ms_{};
  std::optional<status_flags_t> flags_;
  std::vector<day_size_t> days_;
  bool measured_{};
  bool full_{};
  uint64_t frames_{};
  uint64_t shots_{};
  uint64_t bytes_{};
  };
  }  // namespace vision
