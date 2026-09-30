#include <boost/ut.hpp>
#include <network_incident.h>

#include <chrono>

auto main() -> int
  {
  using namespace boost::ut;
  using network_incident::classify;
  using network_incident::verdict_e;

  "no lines in the window means nothing is known"_test = []
  { expect(classify("") == verdict_e::unknown); };

  "only up lines and heartbeats mean the network was clean"_test = []
  {
    expect(
      classify(
        "2026-09-30T08:35:59Z net-monitor starting: lan_gw=192.168.1.1 wan_test=1.1.1.1\n"
        "2026-09-30T08:40:59Z heartbeat gw=up wan=up dns_local=up"
      )
      == verdict_e::clean
    );
  };

  "Unbound alone failing does not outrank a real path problem seen in the same window"_test = []
  {
    expect(
      classify(
        "2026-09-30T01:41:10Z DNS DOWN  [local Unbound only] SERVFAIL/timeout resolving github.com via 127.0.0.1,"
        " but @1.1.1.1 direct OK -> local resolver issue, network path OK"
      )
      == verdict_e::unbound_only
    );

    expect(
      classify(
        "2026-09-30T01:41:10Z DNS DOWN  [local Unbound only] SERVFAIL/timeout resolving github.com via 127.0.0.1,"
        " but @1.1.1.1 direct OK -> local resolver issue, network path OK\n"
        "2026-09-30T01:41:20Z PING DOWN [wan 1.1.1.1] unreachable (>=2 consecutive failures)"
      )
      == verdict_e::network_down
    );
  };

  "a gateway or WAN ping failure is a real local problem"_test = []
  {
    expect(classify("2026-09-30T01:41:20Z PING DOWN [gw 192.168.1.1] unreachable (>=2 consecutive failures)")
           == verdict_e::network_down);
    expect(classify("2026-09-30T08:40:59Z heartbeat gw=down wan=up dns_local=up") == verdict_e::network_down);
  };

  "the community's glossary carries at least the codes seen in play, each with a side"_test = []
  {
    auto const codes{network_incident::known_codes()};
    expect(not codes.empty());
    for(auto const & code: codes)
      {
      expect(not code.code.empty());
      expect(not code.side.empty());
      }
  };

  "a burst of checksum failures within a couple of seconds collapses into one incident"_test = []
  {
    auto const found{network_incident::scan_netlog(
      "netLog.2026-09-29T230100.01.log",
      std::chrono::seconds{0},
      "{23:01:15GMT 15.000s} checksum failure, IP4:34.244.115.92:19364,0, from:Disconnected;ckm=1;rpc=1;"
      " runId=1 calculated=aaaa received=bbbb\n"
      "{23:01:15GMT 15.100s} Checksum disconnected\n"
      "{23:01:15GMT 15.300s} checksum failure, IP4:34.244.115.92:19364,0, from:Disconnected;ckm=1;rpc=2;"
      " runId=1 calculated=cccc received=dddd\n"
      "{23:01:15GMT 15.400s} Checksum disconnected"
    )};
    expect(found.size() == 1u);
    expect(found[0].category == std::string{"checksum failure"});
    expect(found[0].detail.contains("34.244.115.92"));
  };

  "checksum failures more than two seconds apart are two separate incidents"_test = []
  {
    auto const found{network_incident::scan_netlog(
      "netLog.2026-09-29T230100.01.log",
      std::chrono::seconds{0},
      "{23:01:15GMT 15.000s} checksum failure, IP4:34.244.115.92:19364,0, from:x\n"
      "{23:05:00GMT 240.000s} checksum failure, IP4:34.244.115.92:19364,0, from:x"
    )};
    expect(found.size() == 2u);
  };

  "a structured Disconnect line is read for its reason"_test = []
  {
    auto const found{network_incident::scan_netlog(
      "netLog.2026-09-29T230100.01.log",
      std::chrono::seconds{0},
      "{23:08:16GMT 316.031s} Disconnect: type=11&reason=WaitForLocationReadyActivity-timeout&primary=&missions=&systemAddr=1"
    )};
    expect(found.size() == 1u);
    expect(found[0].category == std::string{"disconnect: WaitForLocationReadyActivity-timeout"});
  };

  "a journal file with a Shutdown event is not a crash"_test = []
  {
    expect(not network_incident::scan_journal_for_crash(
      R"({ "timestamp":"2026-09-29T23:56:55Z", "event":"Music", "MusicTrack":"NoTrack" })"
      "\n"
      R"({ "timestamp":"2026-09-29T23:56:56Z", "event":"Shutdown" })"
    ).has_value());
  };

  "a journal file with no Shutdown before the next one is flagged, without calling it a crash"_test = []
  {
    auto const found{network_incident::scan_journal_for_crash(
      R"({ "timestamp":"2026-09-29T23:41:41Z", "event":"LeaveBody", "Body":"x", "BodyID":1 })"
      "\n"
      R"({ "timestamp":"2026-09-29T23:46:55Z", "event":"ReservoirReplenished", "FuelMain":1.0 })"
    )};
    expect(found.has_value());
    expect(found->category == std::string{"ended without Shutdown"});
    expect(found->detail.contains("ReservoirReplenished"));
  };
  }
