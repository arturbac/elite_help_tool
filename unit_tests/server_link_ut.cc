#include <boost/ut.hpp>
#include <server_link.h>

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;
  using server_link::level_e;
  using server_link::time_point_t;

  time_point_t const t0{std::chrono::sys_days{std::chrono::year{2026} / 10 / 1} + 21h + 13min};

  "a web request failing and a slow one are the API's trouble, not the game server's"_test = [t0]
  {
    // what the game wrote on stepping out of the ship, 1 Oct 2026 - the session went on
    server_link::netlog_state_t state;
    feed(state, t0 + 25s, "{21:13:25GMT 6267.726s} Webserver request failed: code 502, details '<html>");
    feed(state, t0 + 39s, "{21:13:39GMT 6281.059s} Webserver request failed: code 502, details '<html>");
    feed(
      state,
      t0 + 39s,
      "{21:13:39GMT 6281.063s} HTTP Request took 10.31 sec to complete: "
      "https://api.orerve.net:443/2.0/elite/commander/inventory/transferbackpack?U0l5bN"
    );
    auto const shown{warnings(state, std::nullopt, t0 + 45s)};
    expect(fatal(shown.size() == 2u));
    expect(shown[0].level == level_e::degraded);
    expect(shown[0].text == "Frontier API: 2 requests failed in the last minute (HTTP 502)") << shown[0].text;
    expect(shown[1].text == "Frontier API: commander/inventory/transferbackpack took 10 s") << shown[1].text;

    expect(warnings(state, std::nullopt, t0 + 39s + 61s).empty());
  };

  "code 0 is no answer at all"_test = [t0]
  {
    server_link::netlog_state_t state;
    feed(state, t0, "{15:19:14GMT 8878.036s} Webserver request failed: code 0, details ''");
    auto const shown{warnings(state, std::nullopt, t0 + 1s)};
    expect(fatal(shown.size() == 1u));
    expect(shown[0].text == "Frontier API: 1 request failed in the last minute (no answer)") << shown[0].text;
  };

  "lost packets, a dropped server and the disconnect, as on 27 Sep 2026"_test = [t0]
  {
    server_link::netlog_state_t state;
    feed(
      state,
      t0,
      "{15:18:35GMT 8838.603s} From 268097820749581 x 15 [0/2]((108.129.72.136:19364))EDServer#6222 - Several LOST "
      "packet# (between 31114..31147)"
    );
    auto shown{warnings(state, std::nullopt, t0 + 5s)};
    expect(fatal(shown.size() == 1u));
    expect(shown[0].level == level_e::degraded);
    expect(shown[0].text == "EDServer#6222: packets lost 5 s ago") << shown[0].text;

    feed(
      state,
      t0 + 33s,
      "{15:19:08GMT 8871.904s} Disconnected: 126918575052865 x 11 [0/2]((34.254.175.226:19364))EDServer#6228 (Too many "
      "retries)"
    );
    shown = warnings(state, 33s, t0 + 40s);
    expect(fatal(shown.size() == 3u));
    expect(shown[0].level == level_e::failing);
    expect(shown[0].text == "game server silent 33 s") << shown[0].text;
    expect(shown[1].text == "EDServer#6228 dropped 7 s ago - a disconnect may follow") << shown[1].text;

    feed(
      state,
      t0 + 51s,
      "{15:19:26GMT 8890.138s} Disconnect: type=1&reason=CheckInGameExitConditions&primary=EDServer_6222&missions="
      "EDServer_6228&systemAddr=1487912553027"
    );
    shown = warnings(state, std::nullopt, t0 + 52s);
    expect(fatal(shown.size() == 1u));
    expect(shown[0].text == "disconnected: CheckInGameExitConditions, 1 s ago") << shown[0].text;

    feed(state, t0 + 70s, "{15:19:45GMT 8909.0s} Connected: 1234 x 1 [0/2]((1.2.3.4:19364))EDServer#6230");
    expect(warnings(state, std::nullopt, t0 + 71s).empty());
  };

  "a link to another player given up is told apart from Frontier's servers"_test = [t0]
  {
    server_link::netlog_state_t state;
    feed(state, t0, "{23:10:49GMT 3935.921s} Disconnected: 260298145259691 x 2 [1/2]((Relay))Name Unknown (Too many retries)");
    auto shown{warnings(state, std::nullopt, t0 + 3s)};
    expect(fatal(shown.size() == 1u));
    expect(shown[0].level == level_e::degraded);
    expect(shown[0].text == "player link dropped 3 s ago (relay, Too many retries)") << shown[0].text;

    // a player leaving in good order, or the game's own machine closing, is no trouble
    server_link::netlog_state_t quiet;
    feed(quiet, t0, "{23:10:49GMT 3935.921s} Disconnected: 260298145259691 x 4 [0/2]((1.2.3.4:5100))Name Unknown (shutdown)");
    feed(quiet, t0, "{23:10:49GMT 3935.921s} Disconnected: 260298145259691 x 4 ThisMachine Name Unknown (shutdown)");
    // the game closing its link to a server it no longer needs, as it does many times an hour
    feed(quiet, t0, "{08:21:03GMT 715.974s} Disconnected: 73955298349590 x 4 [0/2]((54.74.32.89:19364))EDServer#6229 (ServerLink::~)");
    feed(quiet, t0, "{08:21:03GMT 715.974s} Disconnected: 73955298349590 x 4 [0/2]((54.74.32.89:19364))Name Unknown (ServerLink::~)");
    expect(warnings(quiet, std::nullopt, t0 + 1s).empty());
  };

  "a silence counts only after traffic, and only once it is long enough"_test = [t0]
  {
    server_link::udp_silence_t silence;
    feed(silence, t0, 100u);
    // a game at its main menu hears nothing - no alarm
    expect(not silent_for(silence, t0 + 10s, 3000ms));

    for(uint64_t step{1u}; step <= 6u; ++step)
      feed(silence, t0 + 10s + step * 500ms, 100u + step);
    auto const last{t0 + 13s};
    feed(silence, last + 2s, 106u);
    expect(not silent_for(silence, last + 2s, 3000ms));
    auto const quiet{silent_for(silence, last + 4s, 3000ms)};
    expect(fatal(quiet.has_value()));
    expect(*quiet == 4000ms);

    // traffic again - the silence is over and must be earned anew
    feed(silence, last + 5s, 107u);
    expect(not silent_for(silence, last + 9s, 3000ms));
  };

  "the newest netLog is read as it grows, a half written line waiting for its end"_test = []
  {
    // a directory of its own under the system's temporary one, named after this process
    std::filesystem::path const dir{
      std::filesystem::temp_directory_path() / std::format("server_link_ut_{}", ::getpid())
    };
    std::filesystem::create_directories(dir);
    {
    std::ofstream older{dir / "netLog.2026-09-30T100000.01.log"};
    older << "{08:00:01GMT 1.0s} Several LOST packet# (between 1..9)\n";
    }
    std::filesystem::path const current{dir / "netLog.2026-10-01T212859.01.log"};
    {
    std::ofstream file{current};
    file << "{21:13:25GMT 6267.726s} Webserver request failed: code 502, details '<html>\n<head>\n{21:13:39GMT 6";
    }
    server_link::tail_t tail;
    std::vector<std::string> seen;
    auto const keep = [&seen](server_link::time_point_t, std::string_view line) { seen.emplace_back(line); };
    tail.read(dir, keep);
    expect(tail.file() == current);
    // the html's own lines carry no moment and are passed over
    expect(fatal(seen.size() == 1u));
    expect(seen[0].contains("code 502"));

    {
    std::ofstream file{current, std::ios::app};
    file << "281.059s} Webserver request failed: code 502, details ''\n";
    }
    tail.read(dir, keep);
    expect(fatal(seen.size() == 2u));
    expect(seen[1] == "{21:13:39GMT 6281.059s} Webserver request failed: code 502, details ''") << seen[1];

    tail.read(dir, keep);
    expect(seen.size() == 2u);
    std::filesystem::remove(dir / "netLog.2026-09-30T100000.01.log");
    std::filesystem::remove(current);
    std::filesystem::remove(dir);
  };
  }
