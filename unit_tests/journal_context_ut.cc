#include <boost/ut.hpp>
#include <journal_context.h>

#include <string>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;
  using journal_context::where_e;

  // a construction site's approach as the journal wrote it (Sjona, 2026-10-23), the lines cut to what is read
  std::string const before{
    "{ \"timestamp\":\"2026-10-23T15:07:50Z\", \"event\":\"LoadGame\", \"Ship\":\"panthermkii\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:07:55Z\", \"event\":\"Location\", \"Docked\":true, \"StationName\":\"Brewer Site\", "
    "\"StarSystem\":\"Bleia Eohn LQ-F b31-7\", \"Body\":\"Bleia Eohn LQ-F b31-7 A 1\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:12:57Z\", \"event\":\"Undocked\", \"StationName\":\"Brewer Site\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:13:40Z\", \"event\":\"SupercruiseEntry\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:16:31Z\", \"event\":\"SupercruiseExit\", \"Body\":\"Bleia Eohn PW-D b32-1 7\", "
    "\"BodyType\":\"Planet\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:16:42Z\", \"event\":\"DockingRequested\", "
    "\"StationName\":\"Planetary Construction Site: Bischoff Legacy\", \"StationType\":\"PlanetaryConstructionDepot\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:16:42Z\", \"event\":\"DockingGranted\", \"LandingPad\":1, "
    "\"StationName\":\"Planetary Construction Site: Bischoff Legacy\", \"StationType\":\"PlanetaryConstructionDepot\" }\r\n"};
  std::string const after{
    "{ \"timestamp\":\"2026-10-23T15:17:29Z\", \"event\":\"Docked\", "
    "\"StationName\":\"Planetary Construction Site: Bischoff Legacy\", \"StationType\":\"PlanetaryConstructionDepot\", "
    "\"StarSystem\":\"Bleia Eohn PW-D b32-1\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:30:00Z\", \"event\":\"Undocked\", \"StationName\":\"Planetary Construction Site: Bischoff Legacy\" }\r\n"
    "{ \"timestamp\":\"2026-10-23T15:31:00Z\", \"event\":\"Liftoff\" "};  // the game still writing it

  "the place before the byte, and the steps within the window after it"_test = [&]
  {
    auto const c{journal_context::read(before + after, before.size(), 60s)};
    expect(c.last_before == "2026-10-23T15:16:42Z") << c.last_before;
    expect(c.before.where == where_e::normal_space) << journal_context::where_name(c.before.where);
    expect(c.before.ship == "panthermkii" and c.before.body == "Bleia Eohn PW-D b32-1 7") << c.before.body;
    expect(c.before.pad == 1u and c.before.station == "Planetary Construction Site: Bischoff Legacy");
    expect(c.before.system == "Bleia Eohn LQ-F b31-7") << "the system as Location gave it: no jump since";
    expect(c.after.where == where_e::docked and c.after.system == "Bleia Eohn PW-D b32-1");
    expect(c.steps.size() == 1u) << "the undocking is past the window, the half line never read";
    expect(c.steps.front().event == "Docked" and c.steps.front().seconds == 47) << c.steps.front().seconds;
  };

  "a wider window reaches the undocking"_test = [&]
  {
    auto const c{journal_context::read(before + after, before.size(), 900s)};
    expect(c.steps.size() == 2u and c.after.where == where_e::normal_space and c.after.pad == 0u);
  };

  "a byte past the end is the state at the end, with no steps"_test = [&]
  {
    auto const c{journal_context::read(before, before.size() + 10u, 60s)};
    expect(c.steps.empty() and c.before.pad == 1u and c.after.pad == 1u);
  };

  "the JSON names the place and the steps"_test = [&]
  {
    auto const json{journal_context::to_json(journal_context::read(before + after, before.size(), 60s))};
    expect(json.find(R"("where": "docked")") != std::string::npos) << json;
    expect(json.find(R"x({"seconds": 47, "event": "Docked", "what": "Planetary Construction Site: Bischoff Legacy (PlanetaryConstructionDepot)"})x")
           != std::string::npos) << json;
  };
  }
