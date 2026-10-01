#include <server_link.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <fstream>
#include <ranges>

namespace server_link
  {
namespace
  {
using namespace std::chrono_literals;

///\brief how long each thing netLog said stays on the screen
constexpr auto lost_shown{60s};
constexpr auto dropped_shown{90s};
constexpr auto disconnect_shown{60s};
constexpr auto api_shown{60s};
///\brief growths of the count in a row before a silence means anything - a couple of seconds of traffic
constexpr uint32_t flowing_needed{5u};
///\brief the newest netLog is looked for this often - the game starts a new one only when it starts
constexpr auto list_every{10s};

[[nodiscard]]
auto seconds_between(time_point_t from, time_point_t to) -> long long
  {
  return std::chrono::duration_cast<std::chrono::seconds>(to - from).count();
  }

///\brief the text after needle up to the first of the stops, empty without needle
[[nodiscard]]
auto after(std::string_view line, std::string_view needle, std::string_view stops) -> std::string_view
  {
  auto const pos{line.find(needle)};
  if(pos == std::string_view::npos)
    return {};
  std::string_view const rest{line.substr(pos + needle.size())};
  return rest.substr(0, rest.find_first_of(stops));
  }

///\brief the server a line of netLog speaks of - "EDServer#6222"
[[nodiscard]]
auto server_name(std::string_view line) -> std::string
  {
  auto const pos{line.find("EDServer#")};
  if(pos == std::string_view::npos)
    return "game server";
  std::string_view const rest{line.substr(pos)};
  return std::string{rest.substr(0, rest.find_first_of(" -("))};
  }

///\brief true when the thing said at `at` is still to be shown - not too old, and no server reached since
[[nodiscard]]
auto still_shown(
  std::optional<time_point_t> at, std::optional<time_point_t> connected, time_point_t now, std::chrono::seconds shown
) -> bool
  {
  return at and now - *at < shown and not(connected and *connected >= *at);
  }

///\brief the machine's offset from UTC when the netLog file began - its name is in local time
[[nodiscard]]
auto local_utc_offset(std::string_view netlog_name) -> std::chrono::seconds
  {
  try
    {
    if(auto const local{evidence::netlog_local_start(netlog_name)}; local)
      return std::chrono::current_zone()->get_info(*local).first.offset;
    return std::chrono::current_zone()->get_info(std::chrono::system_clock::now()).offset;
    }
  catch(...)
    {
    return std::chrono::seconds{0};
    }
  }
  }  // namespace

auto feed(netlog_state_t & state, time_point_t at, std::string_view line) -> void
  {
  if(line.contains("Several LOST packet#"))
    {
    state.lost_at = at;
    state.lost_server = server_name(line);
    }
  else if(line.contains("} Disconnected: ") and line.contains("(Too many retries)"))
    {
    state.dropped_at = at;
    state.dropped_server = server_name(line);
    }
  else if(line.contains("Disconnect: type="))
    {
    state.disconnected_at = at;
    state.disconnect_reason = std::string{after(line, "reason=", "&")};
    if(state.disconnect_reason.empty())
      state.disconnect_reason = "?";
    }
  else if(line.contains("} Connected: "))
    state.connected_at = at;
  else if(line.contains("Webserver request failed: code "))
    {
    std::string_view const code{after(line, "failed: code ", ", ")};
    state.api_failures.emplace_back(at, code == "0" ? std::string{"no answer"} : std::format("HTTP {}", code));
    }
  else if(line.contains("HTTP Request took "))
    {
    std::string_view const took{after(line, "HTTP Request took ", " ")};
    double seconds{};
    if(auto const [end, ec]{std::from_chars(took.data(), took.data() + took.size(), seconds)}; ec != std::errc{})
      return;
    state.slow_at = at;
    state.slow_s = seconds;
    // https://api.orerve.net:443/2.0/elite/commander/inventory/transferbackpack?... - the path says what it was
    std::string_view what{after(line, "/2.0/elite/", "? ")};
    if(what.empty())
      what = after(line, "complete: ", "? ");
    state.slow_what = std::string{what};
    }

  // the failures are counted over a minute - older ones go
  while(not state.api_failures.empty() and at - state.api_failures.front().first > api_shown)
    state.api_failures.pop_front();
  }

auto feed(udp_silence_t & silence, time_point_t now, uint64_t in_datagrams) -> void
  {
  if(not silence.last_count)
    {
    silence.last_count = in_datagrams;
    silence.last_growth = now;
    return;
    }
  if(in_datagrams == *silence.last_count)
    return;
  // a count that went down is a counter started again - taken as traffic all the same
  bool const steady{silence.last_growth and now - *silence.last_growth < 2s};
  silence.flowing = steady ? std::min(silence.flowing + 1u, 1000u) : 1u;
  silence.last_count = in_datagrams;
  silence.last_growth = now;
  }

auto silent_for(udp_silence_t const & silence, time_point_t now, std::chrono::milliseconds threshold)
  -> std::optional<std::chrono::milliseconds>
  {
  if(silence.flowing < flowing_needed or not silence.last_growth)
    return std::nullopt;
  auto const quiet{std::chrono::duration_cast<std::chrono::milliseconds>(now - *silence.last_growth)};
  if(quiet < threshold)
    return std::nullopt;
  return quiet;
  }

auto warnings(netlog_state_t const & state, std::optional<std::chrono::milliseconds> silence, time_point_t now)
  -> std::vector<warning_t>
  {
  std::vector<warning_t> result;
  if(still_shown(state.disconnected_at, state.connected_at, now, disconnect_shown))
    result.push_back(
      warning_t{
        .level = level_e::server,
        .text = std::format("disconnected: {}, {} s ago", state.disconnect_reason, seconds_between(*state.disconnected_at, now))
      }
    );
  else if(silence)
    result.push_back(
      warning_t{
        .level = level_e::server,
        .text = std::format(
          "game server silent {} s",
          std::chrono::duration_cast<std::chrono::seconds>(*silence).count()
        )
      }
    );

  // once the game has disconnected, what led to it is told by the line above
  bool const disconnected{not result.empty() and state.disconnected_at};
  if(not disconnected and still_shown(state.dropped_at, state.connected_at, now, dropped_shown))
    result.push_back(
      warning_t{
        .level = level_e::server,
        .text = std::format(
          "{} dropped {} s ago - a disconnect may follow", state.dropped_server, seconds_between(*state.dropped_at, now)
        )
      }
    );
  if(not disconnected and still_shown(state.lost_at, state.connected_at, now, lost_shown))
    result.push_back(
      warning_t{
        .level = level_e::server,
        .text = std::format("{}: packets lost {} s ago", state.lost_server, seconds_between(*state.lost_at, now))
      }
    );

  auto const recent{std::ranges::count_if(state.api_failures, [now](auto const & failure) { return now - failure.first < api_shown; })};
  if(recent != 0)
    result.push_back(
      warning_t{
        .level = level_e::api,
        .text = std::format(
          "Frontier API: {} request{} failed in the last minute ({})",
          recent,
          recent == 1 ? "" : "s",
          state.api_failures.back().second
        )
      }
    );
  if(state.slow_at and now - *state.slow_at < api_shown)
    result.push_back(
      warning_t{
        .level = level_e::api,
        .text = std::format("Frontier API: {} took {:.0f} s", state.slow_what, state.slow_s)
      }
    );
  return result;
  }

auto tail_t::read_lines(std::filesystem::path const & dir) -> std::vector<std::string_view>
  {
  std::vector<std::string_view> lines;
  if(dir.empty())
    return lines;

  if(auto const now{std::chrono::steady_clock::now()}; file_.empty() or now - listed_ > list_every)
    {
    listed_ = now;
    std::filesystem::path newest;
    std::error_code ec;
    // netLog.2026-10-01T212859.01.log - the stamp in the name sorts the files by time
    for(auto const & entry: std::filesystem::directory_iterator{dir, ec})
      if(auto const name{entry.path().filename().string()}; name.starts_with("netLog.") and name.ends_with(".log"))
        if(newest.empty() or entry.path().filename() > newest.filename())
          newest = entry.path();
    if(not newest.empty() and newest != file_)
      {
      file_ = newest;
      offset_ = 0u;
      pending_.clear();
      std::string const name{file_.filename().string()};
      clock_.emplace(name, local_utc_offset(name));
      }
    }
  if(file_.empty())
    return lines;

  std::ifstream file{file_, std::ios::binary};
  if(not file)
    return lines;
  file.seekg(0, std::ios::end);
  auto const size{uint64_t(file.tellg())};
  if(size < offset_)
    {
    // the same name written over from its start
    offset_ = 0u;
    pending_.clear();
    }
  if(size == offset_)
    return lines;
  file.seekg(std::streamoff(offset_));
  std::string fresh(size - offset_, '\0');
  file.read(fresh.data(), std::streamsize(fresh.size()));
  fresh.resize(size_t(file.gcount()));
  offset_ += fresh.size();

  buffer_ = std::move(pending_);
  buffer_ += fresh;
  pending_.clear();
  // the last line may still be half written - it waits for the rest
  auto const end{buffer_.rfind('\n')};
  if(end == std::string::npos)
    {
    pending_ = std::move(buffer_);
    buffer_.clear();
    return lines;
    }
  pending_ = buffer_.substr(end + 1u);
  std::string_view const whole{std::string_view{buffer_}.substr(0, end)};
  for(auto const part: std::views::split(whole, '\n'))
    {
    std::string_view line{part.begin(), part.end()};
    if(line.ends_with('\r'))
      line.remove_suffix(1u);
    lines.push_back(line);
    }
  return lines;
  }

auto tail_t::moment(std::string_view line) -> std::optional<time_point_t>
  {
  if(not clock_ or not clock_->valid())
    return std::nullopt;
  return clock_->moment(line);
  }
  }  // namespace server_link
