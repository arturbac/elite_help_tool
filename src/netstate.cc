#include <netstate.h>

#include <arpa/inet.h>
#include <linux/inet_diag.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>
#include <linux/tcp.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <format>
#include <fstream>
#include <iterator>
#include <map>
#include <ranges>

namespace netstate
  {
namespace
  {
[[nodiscard]]
auto whole(std::filesystem::path const & path) -> std::string
  {
  std::ifstream file{path, std::ios::binary};
  if(not file)
    return {};
  return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  }

[[nodiscard]]
auto words(std::string_view line) -> std::vector<std::string_view>
  {
  std::vector<std::string_view> result;
  for(auto const part: std::views::split(line, ' '))
    if(std::string_view const word{part.begin(), part.end()}; not word.empty())
      result.push_back(word);
  return result;
  }

[[nodiscard]]
auto address(uint8_t family, std::array<uint32_t, 4> const & raw, uint16_t port_be) -> std::string
  {
  std::array<char, INET6_ADDRSTRLEN> text{};
  ::inet_ntop(family, raw.data(), text.data(), text.size());
  uint16_t const port{ntohs(port_be)};
  return family == AF_INET6 ? std::format("[{}]:{}", text.data(), port) : std::format("{}:{}", text.data(), port);
  }

///\brief one dump of the kernel's sockets of a family and a protocol, the game's picked out
auto dump(int fd, uint8_t family, uint8_t protocol, std::set<uint64_t> const & inodes, std::vector<socket_t> & out)
  -> void
  {
  struct request_t
    {
    nlmsghdr header;
    inet_diag_req_v2 body;
    };

  request_t request{};
  request.header.nlmsg_len = sizeof(request);
  request.header.nlmsg_type = SOCK_DIAG_BY_FAMILY;
  request.header.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  request.body.sdiag_family = family;
  request.body.sdiag_protocol = protocol;
  request.body.idiag_states = ~0u;
  request.body.idiag_ext = uint8_t((1u << (INET_DIAG_INFO - 1)) | (1u << (INET_DIAG_SKMEMINFO - 1)));
  sockaddr_nl kernel{};
  kernel.nl_family = AF_NETLINK;
  if(::sendto(fd, &request, sizeof(request), 0, reinterpret_cast<sockaddr const *>(&kernel), sizeof(kernel)) < 0)
    return;

  alignas(nlmsghdr) std::array<char, 32768> buffer{};
  for(;;)
    {
    ssize_t const size{::recv(fd, buffer.data(), buffer.size(), 0)};
    if(size <= 0)
      return;
    auto const * header{reinterpret_cast<nlmsghdr const *>(buffer.data())};
    for(auto left{int(size)}; NLMSG_OK(header, left); header = NLMSG_NEXT(header, left))
      {
      if(header->nlmsg_type == NLMSG_DONE or header->nlmsg_type == NLMSG_ERROR)
        return;
      auto const * message{static_cast<inet_diag_msg const *>(NLMSG_DATA(header))};
      if(not inodes.contains(message->idiag_inode))
        continue;
      std::array<uint32_t, 4> source{};
      std::array<uint32_t, 4> destination{};
      std::memcpy(source.data(), message->id.idiag_src, sizeof(source));
      std::memcpy(destination.data(), message->id.idiag_dst, sizeof(destination));
      socket_t socket{
        .protocol = protocol == IPPROTO_TCP ? "tcp" : "udp",
        .local = address(message->idiag_family, source, message->id.idiag_sport),
        .remote = address(message->idiag_family, destination, message->id.idiag_dport),
        .state = message->idiag_state,
        .receive_queue = message->idiag_rqueue,
        .send_queue = message->idiag_wqueue
      };
      int attributes_left{int(header->nlmsg_len - NLMSG_LENGTH(sizeof(*message)))};
      for(auto const * attribute{reinterpret_cast<rtattr const *>(message + 1)}; RTA_OK(attribute, attributes_left);
          attribute = RTA_NEXT(attribute, attributes_left))
        {
        size_t const length{RTA_PAYLOAD(attribute)};
        if(attribute->rta_type == INET_DIAG_SKMEMINFO and length >= sizeof(uint32_t) * (SK_MEMINFO_DROPS + 1))
          {
          uint32_t drops{};
          std::memcpy(
            &drops, static_cast<char const *>(RTA_DATA(attribute)) + sizeof(uint32_t) * SK_MEMINFO_DROPS, sizeof(drops)
          );
          socket.drops = drops;
          }
        else if(attribute->rta_type == INET_DIAG_INFO and protocol == IPPROTO_TCP)
          {
          // an older kernel sends a shorter tcp_info - what it does not send stays zero
          tcp_info info{};
          std::memcpy(&info, RTA_DATA(attribute), std::min(length, sizeof(info)));
          socket.rtt_us = info.tcpi_rtt;
          socket.rttvar_us = info.tcpi_rttvar;
          socket.retransmits = info.tcpi_retransmits;
          socket.total_retransmits = info.tcpi_total_retrans;
          socket.lost = info.tcpi_lost;
          socket.cwnd = info.tcpi_snd_cwnd;
          }
        }
      out.push_back(std::move(socket));
      }
    }
  }

auto optional_field(std::string & line, std::string_view name, std::optional<uint32_t> const & value) -> void
  {
  if(value)
    line += std::format(R"(,"{}":{})", name, *value);
  }
  }  // namespace

auto parse_snmp(std::string_view text) -> std::optional<counters_t>
  {
  std::map<std::string, uint64_t> values;
  std::vector<std::string_view> header;
  for(auto const part: std::views::split(text, '\n'))
    {
    std::string_view const line{part.begin(), part.end()};
    auto const colon{line.find(':')};
    if(colon == std::string_view::npos)
      continue;
    std::string_view const group{line.substr(0u, colon)};
    auto const fields{words(line.substr(colon + 1u))};
    if(fields.empty())
      continue;
    // the names come first, the numbers in the next line of the same group
    if(fields.front().front() < '0' or fields.front().front() > '9')
      {
      header = fields;
      continue;
      }
    for(size_t ix{}; ix != std::min(header.size(), fields.size()); ++ix)
      {
      uint64_t value{};
      if(std::from_chars(fields[ix].data(), fields[ix].data() + fields[ix].size(), value).ec == std::errc{})
        values[std::format("{}.{}", group, header[ix])] = value;
      }
    header.clear();
    }
  if(not values.contains("Udp.InDatagrams"))
    return std::nullopt;
  auto const get = [&](char const * key) { return values.contains(key) ? values[key] : 0u; };
  return counters_t{
    .udp_in = get("Udp.InDatagrams"),
    .udp_out = get("Udp.OutDatagrams"),
    .udp_in_errors = get("Udp.InErrors"),
    .udp_rcvbuf_errors = get("Udp.RcvbufErrors"),
    .udp_no_ports = get("Udp.NoPorts"),
    .tcp_out = get("Tcp.OutSegs"),
    .tcp_retrans = get("Tcp.RetransSegs")
  };
  }

auto default_routes(std::string_view route_table) -> uint32_t
  {
  uint32_t count{};
  bool header{true};
  for(auto const part: std::views::split(route_table, '\n'))
    {
    if(std::exchange(header, false))
      continue;
    // Iface Destination Gateway Flags RefCnt Use Metric Mask ..., tab separated; a default has both zero
    std::vector<std::string_view> columns;
    for(auto const column: std::views::split(std::string_view{part.begin(), part.end()}, '\t'))
      columns.emplace_back(column.begin(), column.end());
    if(columns.size() > 7u and columns[1] == "00000000" and columns[7] == "00000000")
      ++count;
    }
  return count;
  }

auto socket_inode(std::string_view link) -> std::optional<uint64_t>
  {
  constexpr std::string_view prefix{"socket:["};
  if(not link.starts_with(prefix) or not link.ends_with(']'))
    return std::nullopt;
  std::string_view const digits{link.substr(prefix.size(), link.size() - prefix.size() - 1u)};
  uint64_t inode{};
  if(std::from_chars(digits.data(), digits.data() + digits.size(), inode).ec != std::errc{})
    return std::nullopt;
  return inode;
  }

auto find_game(std::filesystem::path const & journal_dir) -> std::optional<int>
  {
  std::error_code ec;
  std::string const journals{std::filesystem::canonical(journal_dir, ec).string()};
  if(ec)
    return std::nullopt;
  for(auto const & entry: std::filesystem::directory_iterator{"/proc", ec})
    {
    std::string const name{entry.path().filename().string()};
    int pid{};
    if(std::from_chars(name.data(), name.data() + name.size(), pid).ec != std::errc{})
      continue;
    std::string const command{whole(entry.path() / "cmdline")};
    if(not command.contains("EliteDangerous64.exe"))
      continue;
    std::string const environment{whole(entry.path() / "environ")};
    for(auto const part: std::views::split(environment, '\0'))
      if(std::string_view const variable{part.begin(), part.end()}; variable.starts_with("WINEPREFIX="))
        {
        std::string const prefix{
          std::filesystem::canonical(std::string{variable.substr(std::string_view{"WINEPREFIX="}.size())}, ec).string()
        };
        if(not ec and journals.starts_with(prefix + "/"))
          return pid;
        }
    }
  return std::nullopt;
  }

auto socket_inodes(int pid) -> std::set<uint64_t>
  {
  std::set<uint64_t> inodes;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{std::format("/proc/{}/fd", pid), ec})
    {
    std::error_code link_ec;
    std::string const link{std::filesystem::read_symlink(entry.path(), link_ec).string()};
    if(auto const inode{socket_inode(link)}; not link_ec and inode)
      inodes.insert(*inode);
    }
  return inodes;
  }

auto sockets_of(std::set<uint64_t> const & inodes) -> std::vector<socket_t>
  {
  std::vector<socket_t> sockets;
  if(inodes.empty())
    return sockets;
  int const fd{::socket(AF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_SOCK_DIAG)};
  if(fd < 0)
    return sockets;
  timeval const limit{.tv_sec = 1, .tv_usec = 0};
  ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
  for(uint8_t const family: {uint8_t(AF_INET), uint8_t(AF_INET6)})
    for(uint8_t const protocol: {uint8_t(IPPROTO_TCP), uint8_t(IPPROTO_UDP)})
      dump(fd, family, protocol, inodes, sockets);
  ::close(fd);
  return sockets;
  }

auto delta(counters_t const & before, counters_t const & now) noexcept -> counters_t
  {
  // a counter that went backwards was reset - its new value is all that happened since
  auto const d = [](uint64_t a, uint64_t b) { return b >= a ? b - a : b; };
  return counters_t{
    .udp_in = d(before.udp_in, now.udp_in),
    .udp_out = d(before.udp_out, now.udp_out),
    .udp_in_errors = d(before.udp_in_errors, now.udp_in_errors),
    .udp_rcvbuf_errors = d(before.udp_rcvbuf_errors, now.udp_rcvbuf_errors),
    .udp_no_ports = d(before.udp_no_ports, now.udp_no_ports),
    .tcp_out = d(before.tcp_out, now.tcp_out),
    .tcp_retrans = d(before.tcp_retrans, now.tcp_retrans)
  };
  }

auto log_line(
  std::chrono::system_clock::time_point at,
  int pid,
  std::vector<socket_t> const & sockets,
  std::optional<counters_t> const & delta,
  uint32_t routes
) -> std::string
  {
  std::string line{
    std::format(R"({{"ts_utc":"{:%FT%T}Z","pid":{})", std::chrono::floor<std::chrono::milliseconds>(at), pid)
  };
  if(delta)
    line += std::format(
      R"(,"udp":{{"in":{},"out":{},"in_errors":{},"rcvbuf_errors":{},"no_ports":{}}},"tcp":{{"out":{},"retrans":{}}})",
      delta->udp_in,
      delta->udp_out,
      delta->udp_in_errors,
      delta->udp_rcvbuf_errors,
      delta->udp_no_ports,
      delta->tcp_out,
      delta->tcp_retrans
    );
  line += std::format(R"(,"default_routes":{},"sockets":[)", routes);
  bool first{true};
  for(socket_t const & socket: sockets)
    {
    if(not std::exchange(first, false))
      line += ',';
    line += std::format(
      R"({{"proto":"{}","local":"{}","remote":"{}","state":{},"rq":{},"wq":{})",
      socket.protocol,
      socket.local,
      socket.remote,
      socket.state,
      socket.receive_queue,
      socket.send_queue
    );
    optional_field(line, "drops", socket.drops);
    optional_field(line, "rtt_us", socket.rtt_us);
    optional_field(line, "rttvar_us", socket.rttvar_us);
    optional_field(line, "retransmits", socket.retransmits);
    optional_field(line, "total_retrans", socket.total_retransmits);
    optional_field(line, "lost", socket.lost);
    optional_field(line, "cwnd", socket.cwnd);
    line += '}';
    }
  line += "]}";
  return line;
  }
  }  // namespace netstate
