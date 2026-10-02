#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

///\brief the state of the game's network connections, read the way any user may - no packets, no root
///
/// To tell whether the white glare of a settlement comes with trouble on the network, the game's own sockets
/// are looked at every couple of seconds: the kernel's socket diagnostics (netlink sock_diag) answer any user
/// about the sockets of their own processes - for TCP the round trip, its variance, retransmissions and losses,
/// for UDP the queues and the datagrams dropped. Beside them the system's own UDP and TCP counters, as deltas,
/// and how many default routes there are. Which sockets are the game's comes from its /proc/<pid>/fd.
/// The lines hold the addresses the game talks to, other players' among them - kept on this machine, and
/// given to nobody but Frontier
namespace netstate
  {
///\brief one socket of the game
struct socket_t
  {
  std::string protocol;
  std::string local;
  std::string remote;
  uint8_t state{};
  uint32_t receive_queue{};
  uint32_t send_queue{};
  ///\brief datagrams or segments the socket dropped, from its memory info
  std::optional<uint32_t> drops;
  // TCP alone
  std::optional<uint32_t> rtt_us;
  std::optional<uint32_t> rttvar_us;
  std::optional<uint32_t> retransmits;
  std::optional<uint32_t> total_retransmits;
  std::optional<uint32_t> lost;
  std::optional<uint32_t> cwnd;
  };

///\brief the system's counters from /proc/net/snmp that say something of loss
struct counters_t
  {
  uint64_t udp_in{};
  uint64_t udp_out{};
  uint64_t udp_in_errors{};
  uint64_t udp_rcvbuf_errors{};
  uint64_t udp_no_ports{};
  uint64_t tcp_out{};
  uint64_t tcp_retrans{};
  };

///\brief the counters out of the text of /proc/net/snmp - the header line names the columns of the next
[[nodiscard]]
auto parse_snmp(std::string_view text) -> std::optional<counters_t>;

///\brief how many default routes /proc/net/route lists
[[nodiscard]]
auto default_routes(std::string_view route_table) -> uint32_t;

///\brief the inode of a link from /proc/<pid>/fd, "socket:[12345]"; nothing for any other file
[[nodiscard]]
auto socket_inode(std::string_view link) -> std::optional<uint64_t>;

///\brief the game's process, the one whose Wine prefix holds the journal directory - two accounts may play at once
[[nodiscard]]
auto find_game(std::filesystem::path const & journal_dir) -> std::optional<int>;

///\brief the inodes of the process' sockets
[[nodiscard]]
auto socket_inodes(int pid) -> std::set<uint64_t>;

///\brief the sockets among those inodes, TCP and UDP, IPv4 and IPv6, from the kernel's socket diagnostics
///\detail waits at most a second for the kernel; nothing on any error
[[nodiscard]]
auto sockets_of(std::set<uint64_t> const & inodes) -> std::vector<socket_t>;

///\brief one line of the log, JSON
[[nodiscard]]
auto log_line(
  std::chrono::system_clock::time_point at,
  int pid,
  std::vector<socket_t> const & sockets,
  std::optional<counters_t> const & delta,
  uint32_t routes
) -> std::string;

///\brief what happened between two readings of the counters
[[nodiscard]]
auto delta(counters_t const & before, counters_t const & now) noexcept -> counters_t;

///\brief the bytes all the machine's interfaces but the loopback received and sent
struct interface_bytes_t
  {
  uint64_t received{};
  uint64_t sent{};
  };

///\brief the bytes out of the text of /proc/net/dev - "  eth0: <8 received columns> <8 sent columns>"
[[nodiscard]]
auto parse_net_dev(std::string_view text) -> std::optional<interface_bytes_t>;

///\brief what came and went over some seconds, beside what netLog says of the other players - written down
/// to learn how much traffic a player brings whom the game reaches through its server alone
struct traffic_t
  {
  double seconds{};
  ///\brief the system's counters over those seconds - the whole machine's, both accounts' games and any
  /// other program's
  counters_t delta;
  interface_bytes_t bytes;
  ///\brief Status.json says more than the main menu's nothing
  bool in_session{};
  ///\brief other players linked in the instance, from netLog
  uint32_t players{};
  ///\brief the game's own figures of its last ten minutes, from netLog: "machines=N&...&act1=X&act2=Y"
  std::optional<uint32_t> machines;
  std::optional<double> act1;
  std::optional<double> act2;
  };

///\brief one line of the traffic log, JSON
[[nodiscard]]
auto traffic_line(std::chrono::system_clock::time_point at, int pid, traffic_t const & traffic) -> std::string;
  }  // namespace netstate
