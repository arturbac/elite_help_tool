#include <netstate_watch.h>
#include <backup.h>
#include <eht_settings.h>
#include <evidence_log.h>
#include <netstate.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <fstream>
#include <iterator>

namespace
  {
[[nodiscard]]
auto whole(char const * path) -> std::string
  {
  std::ifstream file{path};
  return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  }
  }  // namespace

netstate_watch_t::netstate_watch_t()
  {
  worker_ = std::jthread{[this](std::stop_token stoken)
                         {
                           try
                             {
                             std::mutex sleep_mutex;
                             std::condition_variable_any wake;
                             std::optional<netstate::counters_t> counters;
                             std::chrono::steady_clock::time_point looked{};
                             while(not stoken.stop_requested())
                               {
                               auto const cfg{eht::settings()};
                               auto const now{std::chrono::steady_clock::now()};
                               std::optional<int> pid{game()};
                               // a game that ended leaves its /proc behind
                               if(pid and not std::filesystem::exists(std::format("/proc/{}", *pid)))
                                 {
                                 spdlog::info("netstate: the game's process {} is gone", *pid);
                                 pid.reset();
                                 counters.reset();
                                 }
                               if(cfg->evidence.dir.empty())
                                 pid.reset();
                               else if(not pid and now - looked > std::chrono::seconds{10})
                                 {
                                 looked = now;
                                 std::filesystem::path journals;
                                   {
                                   std::lock_guard const lock{mutex_};
                                   journals = journal_dir_;
                                   }
                                 pid = netstate::find_game(journals);
                                 if(pid)
                                   spdlog::info("netstate: watching the game's connections, process {}", *pid);
                                 }
                                 {
                                 std::lock_guard const lock{mutex_};
                                 pid_ = pid;
                                 }
                               if(pid)
                                 {
                                 auto const sockets{netstate::sockets_of(netstate::socket_inodes(*pid))};
                                 auto const now_counters{netstate::parse_snmp(whole("/proc/net/snmp"))};
                                 std::optional<netstate::counters_t> delta;
                                 if(counters and now_counters)
                                   delta = netstate::delta(*counters, *now_counters);
                                 counters = now_counters;
                                 auto const at{std::chrono::system_clock::now()};
                                 evidence::append(
                                   backup::expand_home(cfg->evidence.dir),
                                   "netstate",
                                   at,
                                   netstate::log_line(
                                     at, *pid, sockets, delta, netstate::default_routes(whole("/proc/net/route"))
                                   ),
                                   cfg->evidence.keep_days
                                 );
                                 }
                               std::unique_lock lock{sleep_mutex};
                               wake.wait_for(
                                 lock,
                                 stoken,
                                 std::chrono::milliseconds{std::max(cfg->evidence.netstate_interval_ms, 500u)},
                                 [] { return false; }
                               );
                               }
                             }
                           catch(std::exception const & e)
                             {
                             spdlog::error("netstate: stopped: {}", e.what());
                             }
                         }};
  }

auto netstate_watch_t::set_journal_dir(std::filesystem::path dir) -> void
  {
  std::lock_guard const lock{mutex_};
  journal_dir_ = std::move(dir);
  }

auto netstate_watch_t::game() const -> std::optional<int>
  {
  std::lock_guard const lock{mutex_};
  return pid_;
  }
