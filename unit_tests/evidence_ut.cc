#include <boost/ut.hpp>
#include <evidence_log.h>
#include <netstate.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>

namespace
  {
using namespace std::chrono_literals;
using sys_clock_t = std::chrono::system_clock;

constexpr std::string_view snmp{
  "Ip: Forwarding DefaultTTL\n"
  "Ip: 1 64\n"
  "Tcp: RtoAlgorithm RtoMin OutSegs RetransSegs\n"
  "Tcp: 1 200 5000 12\n"
  "Udp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors\n"
  "Udp: 900 3 7 800 2 0\n"
};

[[nodiscard]]
auto at(std::chrono::hours h, std::chrono::minutes m, std::chrono::seconds s = {}) -> sys_clock_t::time_point
  { return sys_clock_t::time_point{std::chrono::sys_days{std::chrono::September / 29 / 2026}} + h + m + s; }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  "the system's counters are read by the names in the line above them"_test = []
  {
    auto const counters{netstate::parse_snmp(snmp)};
    expect(counters.has_value());
    expect(counters->udp_in == 900u and counters->udp_out == 800u);
    expect(counters->udp_in_errors == 7u and counters->udp_rcvbuf_errors == 2u and counters->udp_no_ports == 3u);
    expect(counters->tcp_out == 5000u and counters->tcp_retrans == 12u);
    expect(not netstate::parse_snmp("nothing").has_value());

    auto later{*counters};
    later.udp_in_errors = 10u;
    later.tcp_retrans = 3u;
    auto const d{netstate::delta(*counters, later)};
    expect(d.udp_in_errors == 3u);
    // gone backwards - a reset, all of it new
    expect(d.tcp_retrans == 3u);
  };

  "default routes, sockets among the fds"_test = []
  {
    std::string const route{
      "Iface\tDestination\tGateway \tFlags\tRefCnt\tUse\tMetric\tMask\t\tMTU\tWindow\tIRTT\n"
      "enp113s0\t00000000\t0101A8C0\t0003\t0\t0\t100\t00000000\t0\t0\t0\n"
      "enp113s0\t0001A8C0\t00000000\t0001\t0\t0\t100\t00FFFFFF\t0\t0\t0\n"
    };
    expect(netstate::default_routes(route) == 1u);
    expect(netstate::socket_inode("socket:[4242]") == std::optional<uint64_t>{4242u});
    expect(not netstate::socket_inode("pipe:[4242]").has_value());
    expect(not netstate::socket_inode("/dev/null").has_value());
  };

  "a netstate line names the moment and every socket"_test = []
  {
    netstate::socket_t const tcp{
      .protocol = "tcp", .local = "192.168.1.122:40000", .remote = "1.2.3.4:443", .state = 1u,
      .receive_queue = 0u, .send_queue = 10u, .drops = 0u, .rtt_us = 25000u, .rttvar_us = 3000u,
      .retransmits = 0u, .total_retransmits = 2u, .lost = 0u, .cwnd = 10u
    };
    std::string const line{netstate::log_line(at(5h, 47min), 99, {tcp}, netstate::parse_snmp(snmp), 1u)};
    expect(line.starts_with(R"({"ts_utc":"2026-09-29T05:47:00.000Z","pid":99,"udp":{"in":900)"));
    expect(line.contains(R"("rtt_us":25000)"));
    expect(evidence::line_moment(line) == std::optional{at(5h, 47min)});
  };

  "a day's file, and days older than those kept deleted when a new day starts"_test = []
  {
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / std::format("eht_evidence_ut_{}", ::getpid())};
    evidence::append(dir, "netstate", at(5h, 0min) - 72h, "old", 2u);
    evidence::append(dir, "netstate", at(5h, 0min) - 24h, "yesterday", 2u);
    evidence::append(dir, "sensors", at(5h, 0min) - 72h, "other log", 2u);
    evidence::append(dir, "netstate", at(5h, 0min), "today", 2u);
    expect(not std::filesystem::exists(dir / "netstate-2026-09-26.jsonl"));
    expect(std::filesystem::exists(dir / "netstate-2026-09-28.jsonl"));
    expect(std::filesystem::exists(dir / "netstate-2026-09-29.jsonl"));
    // another log's files are its own
    expect(std::filesystem::exists(dir / "sensors-2026-09-26.jsonl"));
    expect(evidence::daily_files(dir, "netstate", at(0h, 0min) - 1h, at(6h, 0min)).size() == 2u);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
  };

  "the network log's lines get their day from the file's name, and roll over at midnight"_test = []
  {
    evidence::netlog_clock_t clock{"netLog.2026-09-29T062302.01.log", 2h};
    expect(clock.valid());
    expect(clock.moment("{04:23:17GMT 16.568s} Cancelling request 13") == std::optional{at(4h, 23min, 17s)});
    expect(clock.moment("{23:59:59GMT 1.0s} x") == std::optional{at(23h, 59min, 59s)});
    expect(clock.moment("{00:00:05GMT 1.0s} x") == std::optional{at(24h, 0min, 5s)});
    expect(not clock.moment("no time").has_value());
    expect(not evidence::netlog_clock_t{"Journal.log", 0s}.valid());
  };

  "the summary sets the minute round the moment against the minutes before"_test = []
  {
    std::vector<std::string> const netstate{
      R"({"ts_utc":"2026-09-29T05:40:00.000Z","udp":{"in_errors":1,"rcvbuf_errors":0},"tcp":{"retrans":1},"sockets":[{"proto":"tcp","local":"a","remote":"b","rtt_us":20000},{"proto":"udp","local":"c","remote":"d","drops":5}]})",
      R"({"ts_utc":"2026-09-29T05:44:00.000Z","udp":{"in_errors":0,"rcvbuf_errors":0},"tcp":{"retrans":0},"sockets":[{"proto":"udp","local":"c","remote":"d","drops":6}]})",
      R"({"ts_utc":"2026-09-29T05:47:10.000Z","udp":{"in_errors":4,"rcvbuf_errors":2},"tcp":{"retrans":9},"sockets":[{"proto":"tcp","local":"a","remote":"b","rtt_us":400000},{"proto":"udp","local":"c","remote":"d","drops":6}]})",
      R"({"ts_utc":"2026-09-29T05:47:30.000Z","sockets":[{"proto":"udp","local":"c","remote":"d","drops":16}]})",
      R"({"ts_utc":"2026-09-29T05:55:00.000Z","tcp":{"retrans":100},"sockets":[]})"
    };
    std::vector<std::string> const sensors{
      R"({"ts_utc":"2026-09-29T05:40:00.000Z","gpu_c":50.0,"cpu_c":40.0})",
      R"({"ts_utc":"2026-09-29T05:47:00.000Z","gpu_c":71.0,"cpu_c":null})"
    };
    std::vector<std::pair<sys_clock_t::time_point, std::string>> const netlog{
      {at(5h, 40min), "Cancelling request 1"},
      {at(5h, 46min, 50s), "Cancelling request 2"},
      {at(5h, 47min, 30s), "Calculated hash of 0 for Human_Small_Extraction_01_Gameplay"}
    };
    auto const s{evidence::summarise(at(5h, 47min, 21s), netstate, sensors, netlog)};
    expect(s.rtt_before_us == std::optional<uint32_t>{20000u});
    expect(s.rtt_round_us == std::optional<uint32_t>{400000u});
    expect(s.tcp_retrans_before == 1u and s.tcp_retrans_round == 9u);
    expect(s.udp_errors_before == 1u and s.udp_errors_round == 6u);
    expect(s.udp_drops_before == 1u and s.udp_drops_round == 10u);
    expect(s.cancelled_before == 1u and s.cancelled_round == 1u and s.zero_hashes_round == 1u);
    expect(s.gpu_c == std::optional{71.0} and not s.cpu_c.has_value());
    std::string const report{evidence::report_text("{}", s, "picture.png")};
    expect(report.contains("| 20.0 ms | 400.0 ms |"));
    expect(report.contains("![the screen at the moment](picture.png)"));
  };
  
  "the kernel tells the state of a process' own sockets, no privileges asked"_test = []
  {
    int const tcp{::socket(AF_INET, SOCK_STREAM, 0)};
    int const udp{::socket(AF_INET, SOCK_DGRAM, 0)};
    sockaddr_in here{.sin_family = AF_INET, .sin_port = 0, .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)}, .sin_zero = {}};
    expect(::bind(tcp, reinterpret_cast<sockaddr const *>(&here), sizeof(here)) == 0);
    expect(::listen(tcp, 1) == 0);
    expect(::bind(udp, reinterpret_cast<sockaddr const *>(&here), sizeof(here)) == 0);

    auto const inodes{netstate::socket_inodes(::getpid())};
    expect(inodes.size() >= 2u);
    auto const sockets{netstate::sockets_of(inodes)};
    expect(std::ranges::count(sockets, std::string{"tcp"}, &netstate::socket_t::protocol) >= 1);
    expect(std::ranges::count(sockets, std::string{"udp"}, &netstate::socket_t::protocol) >= 1);
    for(netstate::socket_t const & socket: sockets)
      expect(socket.local.starts_with("127.0.0.1:"));
    ::close(tcp);
    ::close(udp);
  };

  "the netLog is found beside the Steam library's game, or by the game's working directory"_test = []
  {
    std::filesystem::path const root{std::filesystem::temp_directory_path() / std::format("eht_netlog_ut_{}", ::getpid())};
    std::filesystem::path const journals{
      root / "steamapps/compatdata/359320/pfx/drive_c/users/steamuser/Saved Games/Frontier Developments/Elite Dangerous"
    };
    std::filesystem::path const logs{root / "steamapps/common/Elite Dangerous/Products/elite-dangerous-odyssey-64/Logs"};
    std::filesystem::create_directories(journals);
    std::filesystem::create_directories(logs);
    std::ofstream{logs / "netLog.2026-09-29T062302.01.log"} << "x\n";
    expect(evidence::find_netlog_dir(journals, {}) == logs);

    std::filesystem::path const launcher{root / "ed-frontier/game/Products/elite-dangerous-odyssey-64"};
    std::filesystem::create_directories(launcher / "Logs");
    std::ofstream{launcher / "Logs" / "netLog.2026-09-29T002721.01.log"} << "x\n";
    expect(evidence::find_netlog_dir(root / "elsewhere", launcher) == launcher / "Logs");
    expect(evidence::find_netlog_dir(root / "elsewhere", root / "ed-frontier/game") == launcher / "Logs");
    expect(evidence::find_netlog_dir(root / "elsewhere", {}).empty());
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  };
  }
