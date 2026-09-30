#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include <simple_enum/simple_enum.hpp>
#include <span>
#include <string>
#include <string_view>
#include <vector>

///\brief disconnects and technical failures, found automatically - the journal never says a word of any of them
///
/// The game shows its own disconnect dialogs - a colour and a ship name, "Orange Sidewinder" and the like -
/// and that label reaches no log anywhere, checked against the whole of netLog's history: it is drawn by the
/// client from an internal code and never written down as text. What is written down, in the game's own
/// netLog (separate from the journal, kept in its Logs folder - see find_netlog_dir in evidence_log.h) is the
/// technical detail underneath: checksum failures, and structured `Disconnect: type=N&reason=...` lines
/// naming things like SaveFailed or a mission timeout. A journal file that ends without a Shutdown event is
/// the third sign - about a fifth of a real, long-running commander's sessions end this way, so it is not
/// read as "a crash": Alt+F4, closing the window, or stopping the game some other way than its own exit menu
/// may just as well be the reason, and there is no way to tell which from the file alone.
///
/// Once a moment is known, net-monitor.service's own log (syslog identifier "net-monitor", read if a service
/// under that name happens to be running - see doc/network_incidents.md for what feeds it) says whether the
/// local network was in trouble right then: no code's own name is trusted for that, only the local reading
namespace network_incident
  {
///\brief one code the community has seen in the game's dialogs, and which side it usually points to - kept
/// only as a legend to read beside a detected incident, since no log ever carries the code itself
struct known_code_t
  {
  std::string_view code;
  std::string_view meaning;
  std::string_view side;
  };

///\brief the codes gathered in doc/network_incidents.md - not official Frontier documentation
[[nodiscard]]
auto known_codes() -> std::span<known_code_t const>;

///\brief one incident found automatically, before it is correlated against net-monitor and stored
struct detected_incident_t
  {
  std::chrono::system_clock::time_point occurred;
  ///\brief what kind of trouble - "checksum failure", "disconnect: SaveFailed", "ended without Shutdown", ...
  std::string category;
  ///\brief the raw line or two behind it, for whoever reads the row later
  std::string detail;
  ///\brief how long the game had heard nothing from its server when it gave up - LastRx on netLog's
  /// "Releasing server on disconnection"; the network went quiet this long before the disconnect itself
  std::optional<double> last_rx_s;
  ///\brief seconds from the disconnect to the next ConnectToServerActivity: state=Init - mostly the player
  /// reading the dialog and clicking on, not the network
  std::optional<double> reconnect_started_s;
  ///\brief seconds from the disconnect to the first "Connected:" after that - the server reached again
  std::optional<double> reconnected_s;
  };

///\brief netLog's lines that mark real trouble: bursts of checksum failures (several within a couple of
/// seconds collapse into one incident) and structured `Disconnect: type=N&reason=...` lines; oldest first
///\param file_name the netLog file's own name, netlog_clock_t needs it for the day its lines belong to
///\param utc_offset the machine's local time against UTC, the same netlog_clock_t asks for elsewhere
[[nodiscard]]
auto scan_netlog(std::string_view file_name, std::chrono::seconds utc_offset, std::string_view text)
  -> std::vector<detected_incident_t>;

///\brief a completed journal file with no Shutdown event in it - a crash, a forced close (Alt+F4, closing the
/// window, stopping the process some other way) or the machine going down all look the same on disk; empty
/// when the file has its Shutdown, or is blank
///\detail only call this for a journal file known to be finished - the currently live one still growing
/// looks the same way until the game's process is confirmed gone
[[nodiscard]]
auto scan_journal_for_crash(std::string_view text) -> std::optional<detected_incident_t>;

///\brief what net-monitor's log said of the local network around a moment
enum struct verdict_e : uint8_t
  {
  ///\brief no net-monitor line falls in the window - the service was not running, or journald aged it out
  unknown,
  ///\brief only "UP" lines and up heartbeats in the window
  clean,
  ///\brief the local Unbound resolver alone failed - the network path itself tested fine
  unbound_only,
  ///\brief the gateway or the WAN ping failed, or DNS failed through both resolvers - a real local/upstream problem
  network_down
  };

consteval auto adl_enum_bounds(verdict_e)
  {
  using enum verdict_e;
  return simple_enum::adl_info{unknown, network_down};
  }

///\brief the net-monitor.service lines (syslog identifier "net-monitor") whose moment falls in
/// [at - before, at + after], oldest first, one a line; empty when none are found or the journal cannot be read
[[nodiscard]]
auto net_monitor_log(std::chrono::system_clock::time_point at, std::chrono::seconds before, std::chrono::seconds after)
  -> std::string;

///\brief the worst state the log's lines show - a DOWN over an UP, both resolvers down over Unbound alone
[[nodiscard]]
auto classify(std::string_view net_monitor_log) -> verdict_e;
  }  // namespace network_incident
