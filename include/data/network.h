#pragma once
#include <network_incident.h>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

///\brief database rows: disconnects and crashes found in the logs
namespace info
  {
///\brief a disconnect, crash or technical failure found automatically - see network_incident.h for how.
/// net_monitor_log and verdict are worked out once, right when the incident is found, since journald's own
/// retention will not keep that window forever
struct network_incident_t
  {
  int64_t oid{-1};
  std::chrono::sys_seconds occurred;
  ///\brief what kind of trouble - "checksum failure", "disconnect: SaveFailed", "ended without Shutdown", ...
  std::string category;
  ///\brief the raw evidence behind it - the netLog line(s), or the journal event running when it stopped
  std::string detail;
  network_incident::verdict_e verdict;
  std::string net_monitor_log;
  ///\brief how long the game had heard nothing from its server when it gave up - LastRx on netLog's
  /// "Releasing server on disconnection"; the network went quiet this long before the disconnect itself
  std::optional<double> last_rx_s;
  ///\brief seconds from the disconnect to the next ConnectToServerActivity: state=Init - mostly the player
  /// reading the dialog and clicking on, not the network
  std::optional<double> reconnect_started_s;
  ///\brief seconds from the disconnect to the first "Connected:" after that - the server reached again
  std::optional<double> reconnected_s;
  };

///\brief how far the automatic scan for network_incident_t rows has gotten - a single row, so the whole of
/// netLog's history (going back years) is not reread on every start
struct incident_scan_progress_t
  {
  uint32_t id{1};
  ///\brief the newest netLog file name fully scanned; empty before the first scan
  std::string netlog_through;
  ///\brief the newest completed journal file name checked for a missing Shutdown; empty before the first scan
  std::string journal_through;
  };
  }  // namespace info
