#include <evidence_log.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <map>

namespace evidence
  {
///\brief the lines of the logs as the summary reads them - glaze wants types with linkage
namespace detail
  {
struct socket_line_t
  {
  std::string proto;
  std::string local;
  std::string remote;
  std::optional<uint32_t> drops;
  std::optional<uint32_t> rtt_us;
  };

struct udp_line_t
  {
  uint64_t in_errors{};
  uint64_t rcvbuf_errors{};
  };

struct tcp_line_t
  {
  uint64_t retrans{};
  };

struct netstate_line_t
  {
  std::string ts_utc;
  std::optional<udp_line_t> udp;
  std::optional<tcp_line_t> tcp;
  std::vector<socket_line_t> sockets;
  };

struct sensors_line_t
  {
  std::string ts_utc;
  std::optional<double> gpu_c;
  std::optional<double> cpu_c;
  };

  }  // namespace detail

namespace
  {
using namespace detail;
using sys_clock_t = std::chrono::system_clock;

[[nodiscard]]
auto number(std::string_view text, size_t at, size_t length) -> std::optional<int>
  {
  if(at + length > text.size())
    return std::nullopt;
  int value{};
  auto const [end, error]{std::from_chars(text.data() + at, text.data() + at + length, value)};
  if(error != std::errc{} or end != text.data() + at + length)
    return std::nullopt;
  return value;
  }

///\brief 2026-09-29T05:47:21Z or 2026-09-29T05:47:21.123Z
[[nodiscard]]
auto parse_iso(std::string_view text) -> std::optional<sys_clock_t::time_point>
  {
  auto const year{number(text, 0u, 4u)};
  auto const month{number(text, 5u, 2u)};
  auto const day{number(text, 8u, 2u)};
  auto const hour{number(text, 11u, 2u)};
  auto const minute{number(text, 14u, 2u)};
  auto const second{number(text, 17u, 2u)};
  if(not year or not month or not day or not hour or not minute or not second or text.size() < 20u or text[10] != 'T')
    return std::nullopt;
  std::chrono::year_month_day const date{
    std::chrono::year{*year}, std::chrono::month{unsigned(*month)}, std::chrono::day{unsigned(*day)}
  };
  if(not date.ok())
    return std::nullopt;
  std::chrono::milliseconds fraction{};
  if(text[19] == '.')
    if(auto const ms{number(text, 20u, 3u)}; ms)
      fraction = std::chrono::milliseconds{*ms};
  return sys_clock_t::time_point{std::chrono::sys_days{date}}
         + std::chrono::hours{*hour} + std::chrono::minutes{*minute} + std::chrono::seconds{*second} + fraction;
  }

[[nodiscard]]
auto field_moment(std::string_view line, std::string_view field) -> std::optional<sys_clock_t::time_point>
  {
  std::string const key{std::format(R"("{}":")", field)};
  auto const at{line.find(key)};
  if(at == std::string_view::npos)
    return std::nullopt;
  return parse_iso(line.substr(at + key.size()));
  }

[[nodiscard]]
auto day_of(std::string_view name, std::string_view stem) -> std::optional<std::chrono::sys_days>
  {
  // <stem>-YYYY-MM-DD.jsonl
  if(not name.starts_with(stem) or name.size() != stem.size() + 1u + 10u + 6u or not name.ends_with(".jsonl"))
    return std::nullopt;
  auto const moment{parse_iso(std::format("{}T00:00:00Z", name.substr(stem.size() + 1u, 10u)))};
  if(not moment)
    return std::nullopt;
  return std::chrono::floor<std::chrono::days>(*moment);
  }

constexpr auto lenient{glz::opts{.error_on_unknown_keys = false}};
  }  // namespace

auto daily_file(std::filesystem::path const & dir, std::string_view stem, sys_clock_t::time_point at)
  -> std::filesystem::path
  { return dir / std::format("{}-{:%F}.jsonl", stem, std::chrono::floor<std::chrono::days>(at)); }

auto append(
  std::filesystem::path const & dir, std::string_view stem, sys_clock_t::time_point at, std::string_view line, uint32_t keep_days
) -> void
  {
  std::error_code ec;
  std::filesystem::path const file{daily_file(dir, stem, at)};
  bool const new_day{not std::filesystem::exists(file, ec)};
  if(new_day)
    {
    std::filesystem::create_directories(dir, ec);
    // a new day's file is the moment to let go of the oldest - only files of this log, by the day in their name
    auto const oldest{std::chrono::floor<std::chrono::days>(at) - std::chrono::days{keep_days}};
    for(auto const & entry: std::filesystem::directory_iterator{dir, ec})
      if(auto const day{day_of(entry.path().filename().string(), stem)}; day and *day < oldest)
        std::filesystem::remove(entry.path(), ec);
    }
  std::ofstream out{file, std::ios::app};
  if(out)
    out << line << '\n';
  }

auto daily_files(std::filesystem::path const & dir, std::string_view stem, sys_clock_t::time_point from, sys_clock_t::time_point to)
  -> std::vector<std::filesystem::path>
  {
  std::vector<std::filesystem::path> files;
  std::error_code ec;
  for(auto day{std::chrono::floor<std::chrono::days>(from)}; day <= std::chrono::floor<std::chrono::days>(to);
      day += std::chrono::days{1})
    if(std::filesystem::path const file{daily_file(dir, stem, day)}; std::filesystem::exists(file, ec))
      files.push_back(file);
  return files;
  }

auto line_moment(std::string_view line) -> std::optional<sys_clock_t::time_point>
  { return field_moment(line, "ts_utc"); }

auto journal_moment(std::string_view line) -> std::optional<sys_clock_t::time_point>
  { return field_moment(line, "timestamp"); }

netlog_clock_t::netlog_clock_t(std::string_view file_name, std::chrono::seconds utc_offset)
  {
  // netLog.2026-09-29T062302.01.log
  constexpr std::string_view prefix{"netLog."};
  if(not file_name.starts_with(prefix) or file_name.size() < prefix.size() + 17u)
    return;
  std::string_view const stamp{file_name.substr(prefix.size(), 17u)};
  auto const day{parse_iso(std::format("{}T00:00:00Z", stamp.substr(0u, 10u)))};
  auto const hour{number(stamp, 11u, 2u)};
  auto const minute{number(stamp, 13u, 2u)};
  auto const second{number(stamp, 15u, 2u)};
  if(not day or not hour or not minute or not second)
    return;
  start_ = *day + std::chrono::hours{*hour} + std::chrono::minutes{*minute} + std::chrono::seconds{*second} - utc_offset;
  last_ = *start_;
  }

auto netlog_clock_t::moment(std::string_view line) -> std::optional<sys_clock_t::time_point>
  {
  // {04:23:17GMT 16.568s}
  if(not start_ or line.size() < 13u or line[0] != '{' or line.substr(9u, 3u) != "GMT")
    return std::nullopt;
  auto const hour{number(line, 1u, 2u)};
  auto const minute{number(line, 4u, 2u)};
  auto const second{number(line, 7u, 2u)};
  if(not hour or not minute or not second)
    return std::nullopt;
  auto const time_of_day{std::chrono::hours{*hour} + std::chrono::minutes{*minute} + std::chrono::seconds{*second}};
  sys_clock_t::time_point moment{std::chrono::floor<std::chrono::days>(last_) + time_of_day};
  // past midnight the time of day starts again
  if(moment + std::chrono::hours{12} < last_)
    moment += std::chrono::days{1};
  last_ = moment;
  return moment;
  }

auto summarise(
  sys_clock_t::time_point moment,
  std::vector<std::string> const & netstate,
  std::vector<std::string> const & sensors,
  std::vector<std::pair<sys_clock_t::time_point, std::string>> const & netlog
) -> summary_t
  {
  summary_t summary;
  auto const round_from{moment - std::chrono::minutes{1}};
  auto const round_to{moment + std::chrono::minutes{1}};
  auto const in_round = [&](sys_clock_t::time_point at) { return at >= round_from and at <= round_to; };

  // a socket's drops are counted since it opened - what happened is the growth over each part of the window
  std::map<std::pair<std::string, std::string>, std::pair<uint32_t, uint32_t>> drops_before;
  std::map<std::pair<std::string, std::string>, std::pair<uint32_t, uint32_t>> drops_round;
  auto const grow = [](auto & map, socket_line_t const & socket)
  {
    auto const [it, fresh]{map.try_emplace({socket.local, socket.remote}, *socket.drops, *socket.drops)};
    it->second.first = std::min(it->second.first, *socket.drops);
    it->second.second = std::max(it->second.second, *socket.drops);
    (void)fresh;
  };
  for(std::string const & text: netstate)
    {
    netstate_line_t line;
    if(glz::read<lenient>(line, text))
      continue;
    auto const at{parse_iso(line.ts_utc)};
    if(not at or *at > round_to)
      continue;
    bool const round{in_round(*at)};
    if(line.udp)
      (round ? summary.udp_errors_round : summary.udp_errors_before) += line.udp->in_errors + line.udp->rcvbuf_errors;
    if(line.tcp)
      (round ? summary.tcp_retrans_round : summary.tcp_retrans_before) += line.tcp->retrans;
    for(socket_line_t const & socket: line.sockets)
      {
      if(socket.rtt_us)
        {
        auto & most{round ? summary.rtt_round_us : summary.rtt_before_us};
        most = std::max(most.value_or(0u), *socket.rtt_us);
        }
      if(socket.drops and socket.proto == "udp")
        grow(round ? drops_round : drops_before, socket);
      }
    }
  for(auto const & [key, span]: drops_before)
    summary.udp_drops_before += span.second - span.first;
  for(auto const & [key, span]: drops_round)
    summary.udp_drops_round += span.second - span.first;

  // the temperatures of the reading nearest the moment
  std::optional<std::chrono::milliseconds> nearest;
  for(std::string const & text: sensors)
    {
    sensors_line_t line;
    if(glz::read<lenient>(line, text))
      continue;
    auto const at{parse_iso(line.ts_utc)};
    if(not at)
      continue;
    auto const distance{std::chrono::abs(std::chrono::duration_cast<std::chrono::milliseconds>(*at - moment))};
    if(not nearest or distance < *nearest)
      {
      nearest = distance;
      summary.gpu_c = line.gpu_c;
      summary.cpu_c = line.cpu_c;
      }
    }

  for(auto const & [at, text]: netlog)
    {
    if(at > round_to)
      continue;
    bool const round{in_round(at)};
    if(text.contains("Cancelling request"))
      ++(round ? summary.cancelled_round : summary.cancelled_before);
    if(round and text.contains("Calculated hash of 0"))
      ++summary.zero_hashes_round;
    }
  return summary;
  }

auto report_text(std::string_view marker_json, summary_t const & summary, std::string_view picture) -> std::string
  {
  auto const celsius = [](std::optional<double> const & value) -> std::string
  { return value ? std::format("{:.0f} C", *value) : std::string{"-"}; };
  auto const ms = [](std::optional<uint32_t> const & us) -> std::string
  { return us ? std::format("{:.1f} ms", double(*us) / 1000.0) : std::string{"-"}; };

  std::string text{"# Settlement lighting defect - evidence\n\n"};
  text += "Written by Elite Help Tool from what any user of the machine may read: no packet capture, nothing\n"
          "run as root. The white glare of a settlement's room was measured on the screen; the files beside\n"
          "this one hold the minutes round that moment.\n\n";
  if(not picture.empty())
    text += std::format("![the screen at the moment]({})\n\n", picture);
  text += "## Summary\n\n";
  text += "| | before (from 10 min to 1 min earlier) | round the moment (1 min either side) |\n|---|---|---|\n";
  text += std::format("| longest TCP round trip of the game | {} | {} |\n", ms(summary.rtt_before_us), ms(summary.rtt_round_us));
  text += std::format(
    "| TCP retransmissions, whole system | {} | {} |\n", summary.tcp_retrans_before, summary.tcp_retrans_round
  );
  text += std::format(
    "| UDP receive errors, whole system | {} | {} |\n", summary.udp_errors_before, summary.udp_errors_round
  );
  text += std::format(
    "| UDP datagrams dropped by the game's sockets | {} | {} |\n", summary.udp_drops_before, summary.udp_drops_round
  );
  text += std::format(
    "| netLog \"Cancelling request\" | {} | {} |\n", summary.cancelled_before, summary.cancelled_round
  );
  text += std::format("| netLog \"Calculated hash of 0\" | | {} |\n\n", summary.zero_hashes_round);
  text += std::format(
    "Temperatures at the moment: graphics card {}, processor {}.\n\n", celsius(summary.gpu_c), celsius(summary.cpu_c)
  );
  text += "## Files\n\n"
          "- `marker.json` - the moment, the measure of the screen, the place in the game\n"
          "- `netstate.jsonl` - the game's sockets every 2 s: TCP round trip, retransmissions, losses; UDP queues and\n"
          "  drops; the system's UDP and TCP counters as deltas. **Holds the addresses of other players - not for\n"
          "  publishing, only for Frontier.**\n"
          "- `sensors.jsonl` - graphics card and processor temperatures\n"
          "- `journal.log` - the game's journal of the same minutes (the game's clock)\n"
          "- `netlog.log` - the game's network log of the same minutes\n\n";
  text += "## Marker\n\n```json\n";
  text += marker_json;
  text += "\n```\n";
  return text;
  }
  namespace
  {
///\brief the lines of files whose moment falls in the window
auto lines_in(
  std::vector<std::filesystem::path> const & files,
  sys_clock_t::time_point from,
  sys_clock_t::time_point to,
  auto moment_of
) -> std::vector<std::string>
  {
  std::vector<std::string> lines;
  for(std::filesystem::path const & file: files)
    {
    std::ifstream in{file};
    for(std::string line; std::getline(in, line);)
      if(auto const at{moment_of(line)}; at and *at >= from and *at <= to)
        lines.push_back(std::move(line));
    }
  return lines;
  }

///\brief the files of a directory whose name starts so, changed since the moment - the journals and netLogs of the window
[[nodiscard]]
auto recent_files(std::filesystem::path const & dir, std::string_view prefix, sys_clock_t::time_point since)
  -> std::vector<std::filesystem::path>
  {
  std::vector<std::filesystem::path> files;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{dir, ec})
    {
    if(not entry.path().filename().string().starts_with(prefix))
      continue;
    auto const changed{std::chrono::clock_cast<sys_clock_t>(entry.last_write_time(ec))};
    if(not ec and changed >= since)
      files.push_back(entry.path());
    }
  std::ranges::sort(files);
  return files;
  }

auto write_lines(std::filesystem::path const & path, std::vector<std::string> const & lines) -> void
  {
  std::ofstream out{path};
  for(std::string const & line: lines)
    out << line << '\n';
  }
  }  // namespace

auto find_netlog_dir(std::filesystem::path const & journal_dir, std::filesystem::path const & game_cwd)
  -> std::filesystem::path
  {
  std::error_code ec;
  // of the products - Odyssey, Horizons - the one whose netLog was written last
  auto const newest_logs = [&](std::filesystem::path const & products) -> std::filesystem::path
  {
    std::filesystem::path best;
    auto newest{std::filesystem::file_time_type::min()};
    for(auto const & product: std::filesystem::directory_iterator{products, ec})
      for(auto const & file: std::filesystem::directory_iterator{product.path() / "Logs", ec})
        if(file.path().filename().string().starts_with("netLog.") and file.last_write_time(ec) > newest)
          {
          newest = file.last_write_time(ec);
          best = product.path() / "Logs";
          }
    return best;
  };

  std::string const journals{std::filesystem::weakly_canonical(journal_dir, ec).string()};
  if(auto const at{journals.find("/steamapps/compatdata/")}; at != std::string::npos)
    if(auto found{newest_logs(
         std::filesystem::path{journals.substr(0u, at)} / "steamapps" / "common" / "Elite Dangerous" / "Products"
       )};
       not found.empty())
      return found;

  if(not game_cwd.empty())
    {
    if(std::filesystem::path const logs{game_cwd / "Logs"}; std::filesystem::is_directory(logs, ec))
      return logs;
    // a launcher's game: <game>/Products/<product>/, the working directory somewhere in it
    for(std::filesystem::path dir{game_cwd}; dir.has_relative_path(); dir = dir.parent_path())
      {
      if(dir.filename() == "Products")
        if(auto found{newest_logs(dir)}; not found.empty())
          return found;
      if(std::filesystem::is_directory(dir / "Products", ec))
        if(auto found{newest_logs(dir / "Products")}; not found.empty())
          return found;
      }
    }
  return {};
  }

auto write_report(report_input_t const & input) -> std::filesystem::path
  {
  auto const from{input.moment - input.before};
  auto const to{input.moment + input.after};
  std::string const name{std::format("{:%FT%H-%M-%S}Z", std::chrono::floor<std::chrono::seconds>(input.moment))};
  std::filesystem::path const dir{input.evidence_dir / "reports" / name};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);

  std::vector<std::string> const netstate{
    lines_in(daily_files(input.evidence_dir, "netstate", from, to), from, to, line_moment)
  };
  std::vector<std::string> const sensors{lines_in(daily_files(input.evidence_dir, "sensors", from, to), from, to, line_moment)
  };
  write_lines(dir / "netstate.jsonl", netstate);
  write_lines(dir / "sensors.jsonl", sensors);
  write_lines(dir / "journal.log", lines_in(recent_files(input.journal_dir, "Journal.", from), from, to, journal_moment));

  // the netLog's lines carry the time of day alone - each file is read with a clock of its own
  std::vector<std::pair<sys_clock_t::time_point, std::string>> netlog;
  if(not input.netlog_dir.empty())
    for(std::filesystem::path const & file: recent_files(input.netlog_dir, "netLog.", from))
      {
      netlog_clock_t clock{file.filename().string(), input.utc_offset};
      if(not clock.valid())
        continue;
      std::ifstream in{file};
      for(std::string line; std::getline(in, line);)
        if(auto const at{clock.moment(line)}; at and *at >= from and *at <= to)
          netlog.emplace_back(*at, std::move(line));
      }
  {
  std::ofstream out{dir / "netlog.log"};
  for(auto const & [at, line]: netlog)
    out << line << '\n';
  }

  std::string picture;
  if(not input.picture.empty())
    {
    picture = input.picture.filename().string();
    std::filesystem::copy_file(input.picture, dir / picture, std::filesystem::copy_options::overwrite_existing, ec);
    if(ec)
      picture.clear();
    }
  {
  std::ofstream out{dir / "marker.json"};
  out << input.marker_json << '\n';
  }
  summary_t const summary{summarise(input.moment, netstate, sensors, netlog)};
  std::string const text{report_text(input.marker_json, summary, picture)};
  std::filesystem::path const partial{dir / "report.md.tmp"};
  {
  std::ofstream out{partial};
  out << text;
  }
  // the report last, and whole - its presence says the rest is complete
  std::filesystem::rename(partial, dir / "report.md", ec);
  return dir;
  }
  }  // namespace evidence
