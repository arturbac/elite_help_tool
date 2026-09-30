#include <network_incident.h>

#include <evidence_log.h>

#include <algorithm>
#include <array>
#include <format>
#include <memory>
#include <ranges>
#include <systemd/sd-journal.h>

namespace network_incident
  {
namespace
  {
///\brief closes a sd_journal the way its own header wants it done, from any early return
struct journal_deleter_t
  {
  auto operator()(sd_journal * journal) const noexcept -> void { sd_journal_close(journal); }
  };
using journal_ptr_t = std::unique_ptr<sd_journal, journal_deleter_t>;

[[nodiscard]]
auto to_usec(std::chrono::system_clock::time_point at) -> uint64_t
  {
  return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(at.time_since_epoch()).count());
  }
  }  // namespace

auto known_codes() -> std::span<known_code_t const>
  {
  // doc/network_incidents.md carries the same table with the reasoning behind each row - community wisdom
  // from Frontier's and Steam's forums, not an official document
  static constexpr std::array<known_code_t, 6> codes{
    {{.code = "Orange Sidewinder",
      .meaning = "Generic connection error to the Elite servers; the CMDR cannot load",
      .side = "local"},
     {.code = "Yellow/Black Adder",
      .meaning = "Unrecoverable transaction server error (CMDR/module data)",
      .side = "remote"},
     {.code = "Mauve Adder", .meaning = "Matchmaking server connection error", .side = "remote"},
     {.code = "Scarlet/Magenta Krait", .meaning = "Transaction server connection error", .side = "remote"},
     {.code = "Purple Python / Blue, Gold, Teal, Taupe Cobra",
      .meaning = "Mission/adjudication server errors",
      .side = "remote"},
     {.code = "Silver Fer-de-Lance", .meaning = "Multicrew connection timeout between players", .side = "local/P2P"}}
  };
  return codes;
  }

auto net_monitor_log(std::chrono::system_clock::time_point at, std::chrono::seconds before, std::chrono::seconds after)
  -> std::string
  {
  sd_journal * raw{};
  if(sd_journal_open(&raw, SD_JOURNAL_LOCAL_ONLY) < 0) [[unlikely]]
    return {};
  journal_ptr_t const journal{raw};

  if(sd_journal_add_match(journal.get(), "SYSLOG_IDENTIFIER=net-monitor", 0) < 0) [[unlikely]]
    return {};

  uint64_t const until_usec{to_usec(at + after)};
  if(sd_journal_seek_realtime_usec(journal.get(), to_usec(at - before)) < 0) [[unlikely]]
    return {};

  std::string result;
  for(;;)
    {
    int const step{sd_journal_next(journal.get())};
    if(step <= 0)
      break;

    uint64_t entry_usec{};
    if(sd_journal_get_realtime_usec(journal.get(), &entry_usec) < 0) [[unlikely]]
      break;
    if(entry_usec > until_usec)
      break;

    void const * data{};
    size_t length{};
    if(sd_journal_get_data(journal.get(), "MESSAGE", &data, &length) < 0) [[unlikely]]
      continue;

    std::string_view const field{static_cast<char const *>(data), length};
    // the field comes back as "MESSAGE=<text>"
    auto const eq{field.find('=')};
    std::string_view const message{eq == std::string_view::npos ? field : field.substr(eq + 1)};
    auto const moment{std::chrono::sys_time<std::chrono::microseconds>{std::chrono::microseconds{entry_usec}}};

    if(not result.empty())
      result += '\n';
    result += std::format("{:%Y-%m-%dT%H:%M:%SZ} {}", std::chrono::floor<std::chrono::seconds>(moment), message);
    }
  return result;
  }

auto scan_netlog(std::string_view file_name, std::chrono::seconds utc_offset, std::string_view text)
  -> std::vector<detected_incident_t>
  {
  evidence::netlog_clock_t clock{file_name, utc_offset};
  std::vector<detected_incident_t> result;

  std::optional<std::chrono::system_clock::time_point> burst_start;
  std::chrono::system_clock::time_point burst_last{};
  uint32_t burst_count{};
  std::string burst_target;

  auto const flush_burst = [&result, &burst_start, &burst_count, &burst_target]
  {
    if(not burst_start)
      return;
    result.push_back(detected_incident_t{
      .occurred = *burst_start,
      .category = "checksum failure",
      .detail = std::format("{} checksum failures against {}", burst_count, burst_target)
    });
    burst_start.reset();
    burst_count = 0;
    };

  for(auto const part: std::views::split(text, '\n'))
    {
    std::string_view const line{part.begin(), part.end()};
    auto const moment{clock.moment(line)};
    if(not moment)
      continue;

    if(line.contains("checksum failure"))
      {
      // a burst more than 2 s apart is a fresh incident, not the tail of the one before
      if(burst_start and *moment - burst_last > std::chrono::seconds{2})
        flush_burst();
      if(not burst_start)
        {
        burst_start = moment;
        burst_target = "?";
        if(auto const pos{line.find("IP4:")}; pos != std::string_view::npos)
          {
          std::string_view const rest{line.substr(pos + 4)};
          burst_target = std::string{rest.substr(0, rest.find(','))};
          }
        }
      burst_last = *moment;
      ++burst_count;
      continue;
      }

    if(line.contains("Disconnect: type="))
      if(auto const pos{line.find("reason=")}; pos != std::string_view::npos)
        {
        auto const value_start{pos + std::string_view{"reason="}.size()};
        auto const amp{line.find('&', value_start)};
        std::string_view const reason{
          line.substr(value_start, amp == std::string_view::npos ? std::string_view::npos : amp - value_start)
        };
        result.push_back(detected_incident_t{
          .occurred = *moment, .category = std::format("disconnect: {}", reason), .detail = std::string{line}
        });
        }
    }
  flush_burst();
  return result;
  }

auto scan_journal_for_crash(std::string_view text) -> std::optional<detected_incident_t>
  {
  if(text.contains(R"("event":"Shutdown")"))
    return std::nullopt;

  // the last non-empty line carries the moment and whatever the game was doing when it stopped
  std::string_view last_line;
  for(auto const part: std::views::split(text, '\n'))
    if(std::string_view const line{part.begin(), part.end()}; not line.empty())
      last_line = line;

  auto const moment{evidence::journal_moment(last_line)};
  if(not moment)
    return std::nullopt;

  std::string event{"?"};
  if(auto const pos{last_line.find(R"("event":")")}; pos != std::string_view::npos)
    {
    auto const start{pos + std::string_view{R"("event":")"}.size()};
    if(auto const end{last_line.find('"', start)}; end != std::string_view::npos)
      event = std::string{last_line.substr(start, end - start)};
    }

  return detected_incident_t{
    .occurred = *moment,
    .category = "ended without Shutdown",
    .detail = std::format("last event before it stopped: {}", event)
  };
  }

auto classify(std::string_view net_monitor_log) -> verdict_e
  {
  if(net_monitor_log.empty())
    return verdict_e::unknown;

  // the worst line in the window decides the verdict - a DOWN outranks an UP, both resolvers down outranks
  // Unbound alone
  auto worst{verdict_e::clean};
  for(auto const part: std::views::split(net_monitor_log, '\n'))
    {
    std::string_view const line{part.begin(), part.end()};
    if(line.contains("PING DOWN") or line.contains("DNS DOWN  [both"))
      worst = verdict_e::network_down;
    else if(line.contains("DNS DOWN  [local Unbound only]") and worst != verdict_e::network_down)
      worst = verdict_e::unbound_only;
    else if(line.contains("gw=down") or line.contains("wan=down"))
      worst = verdict_e::network_down;
    else if(line.contains("dns_local=down") and worst != verdict_e::network_down)
      worst = verdict_e::unbound_only;
    }
  return worst;
  }
  }  // namespace network_incident
