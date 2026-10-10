#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

///\brief the evidence of the settlement glare, beside the markers: the logs kept for a couple of days and the report
///
/// The temperatures and the state of the network go a line every few seconds into one file a day, named after
/// the UTC day - netstate-2026-09-29.jsonl - and a day older than the days kept is deleted, so the logs never
/// grow past a few days' worth. A marker is turned into a report some minutes later, once what came after it is
/// in the logs too: a directory of its own under reports/ with the picture, the lines of every log from the
/// window round the moment, the game's journal and its network log of the same minutes, and a summary to read
namespace evidence
  {
///\brief the day's file of a log - <dir>/<stem>-YYYY-MM-DD.jsonl
[[nodiscard]]
auto daily_file(std::filesystem::path const & dir, std::string_view stem, std::chrono::system_clock::time_point at)
  -> std::filesystem::path;

///\brief a line added to the day's file, and the files of the stem older than keep_days deleted
auto append(
  std::filesystem::path const & dir,
  std::string_view stem,
  std::chrono::system_clock::time_point at,
  std::string_view line,
  uint32_t keep_days
) -> void;

///\brief the files of a log that hold days from..to - there are few, one a day
[[nodiscard]]
auto daily_files(
  std::filesystem::path const & dir,
  std::string_view stem,
  std::chrono::system_clock::time_point from,
  std::chrono::system_clock::time_point to
) -> std::vector<std::filesystem::path>;

///\brief the moment of a JSON line from its "ts_utc" field
[[nodiscard]]
auto line_moment(std::string_view line) -> std::optional<std::chrono::system_clock::time_point>;

///\brief the moment of a journal line from its "timestamp" field - the game's clock
[[nodiscard]]
auto journal_moment(std::string_view line) -> std::optional<std::chrono::system_clock::time_point>;

///\brief the moments of the game's network log: its header says the day, every line the time of day in GMT
///\detail {04:23:17GMT 16.568s} Cancelling request 13 - the day comes from the file's name,
/// netLog.2026-09-29T062302.01.log, which is local time; a line earlier in the day than the file began is
/// taken for the next day
///\brief the local moment a netLog file's name says it began, nullopt for a name that is not one
[[nodiscard]]
auto netlog_local_start(std::string_view file_name) -> std::optional<std::chrono::local_seconds>;

class netlog_clock_t final
  {
public:
  explicit netlog_clock_t(std::string_view file_name, std::chrono::seconds utc_offset);

  [[nodiscard]]
  auto moment(std::string_view line) -> std::optional<std::chrono::system_clock::time_point>;

  [[nodiscard]]
  auto valid() const noexcept -> bool
    { return start_.has_value(); }

private:
  std::optional<std::chrono::system_clock::time_point> start_;
  std::chrono::system_clock::time_point last_{};
  };

///\brief what the report says at the top - worked out of the lines of the window
struct summary_t
  {
  ///\brief the most any TCP socket of the game waited for an answer, before the moment and round it
  std::optional<uint32_t> rtt_before_us;
  std::optional<uint32_t> rtt_round_us;
  uint64_t tcp_retrans_before{};
  uint64_t tcp_retrans_round{};
  uint64_t udp_errors_before{};
  uint64_t udp_errors_round{};
  uint64_t udp_drops_before{};
  uint64_t udp_drops_round{};
  uint32_t cancelled_before{};
  uint32_t cancelled_round{};
  uint32_t zero_hashes_round{};
  std::optional<double> gpu_c;
  std::optional<double> cpu_c;
  };

///\brief the summary out of the windows' lines; round is the minute each side of the moment, before all earlier
[[nodiscard]]
auto summarise(
  std::chrono::system_clock::time_point moment,
  std::vector<std::string> const & netstate,
  std::vector<std::string> const & sensors,
  std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> const & netlog
) -> summary_t;

///\brief the report's text, Markdown - what a person reads first, the files beside it for the rest
[[nodiscard]]
auto report_text(std::string_view marker_json, summary_t const & summary, std::string_view picture) -> std::string;
///\brief what a report is made of - the evidence directory and where the game keeps its logs
struct report_input_t
  {
  std::filesystem::path evidence_dir;
  std::string marker_json;
  std::chrono::system_clock::time_point moment;
  ///\brief the picture of the moment, copied into the report; empty for none
  std::filesystem::path picture;
  std::filesystem::path journal_dir;
  ///\brief the game's Logs directory with its netLog files; empty when not known
  std::filesystem::path netlog_dir;
  std::chrono::seconds before{600};
  std::chrono::seconds after{300};
  ///\brief the machine's local time against UTC - the netLog files are named in local time
  std::chrono::seconds utc_offset{};
  };

///\brief the game's Logs directory, where its netLog files are: beside the game of the Steam library whose Wine
/// prefix holds the journals, or by the running game's own working directory; empty when neither says
[[nodiscard]]
auto find_netlog_dir(std::filesystem::path const & journal_dir, std::filesystem::path const & game_cwd)
  -> std::filesystem::path;

///\brief the name of a moment's report directory under reports/ - the moment to the second, 2026-10-04T01-57-39Z
[[nodiscard]]
auto report_name(std::chrono::system_clock::time_point moment) -> std::string;

///\brief the moment a marker's file is named after - 2026-10-04T01:57:39.356Z_eht.json; none for another name
[[nodiscard]]
auto marker_moment(std::string_view file_name) -> std::optional<std::chrono::system_clock::time_point>;

///\brief writes the report into <evidence_dir>/reports/<moment>/ and says where it is
[[nodiscard]]
auto write_report(report_input_t const & input) -> std::filesystem::path;
  }  // namespace evidence
