#include <boost/ut.hpp>
#include <eddn_publisher.h>
#include <eht_settings.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

namespace
  {
///\brief a moment the publisher takes for now - older lines are the past, never sent
auto stamp() -> std::string
  { return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())); }

auto write(std::filesystem::path const & path, std::string const & text) -> void
  {
  std::ofstream out{path, std::ios::trunc};
  out << text;
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  std::filesystem::path const dir{std::filesystem::temp_directory_path() / "eht_eddn_ut"};
  std::filesystem::create_directories(dir);
  std::filesystem::path const settings{dir / "eht_settings.json"};
  std::filesystem::path const held{dir / "eddn_held.jsonl"};
  std::filesystem::remove(held);
  std::filesystem::remove(settings);
  // written with the defaults, then the EDDN part switched on for the two accounts of the test
  eht::load_settings(settings);
  {
  eht::settings_t cfg{*eht::settings()};
  cfg.eddn.enabled = true;
  cfg.eddn.test = true;
  cfg.eddn.exploration_commanders = {"F1"};
  cfg.eddn.bartender_commanders = {"F2"};
  write(settings, glz::write_json(cfg).value_or(""));
  }
  eht::load_settings(settings);

  std::string const now{stamp()};
  auto const line = [&](std::string body) { return std::format(R"({{ "timestamp":"{}", {} }})", now, body); };

  "exploration waits for the sale, and only the undiscovered and unpopulated go"_test = [&]
  {
    std::vector<eddn::message_t> sent;
    eddn::publisher_t publisher{dir, held, [&](eddn::message_t && m) { sent.push_back(std::move(m)); }};

    publisher.feed(line(R"("event":"Fileheader", "gameversion":"4.2.0.1", "build":"r1")"), true);
    publisher.feed(line(R"("event":"LoadGame", "FID":"F1", "Commander":"DUNKAN", "Horizons":true, "Odyssey":true)"), true);

    // undiscovered and empty - held
    publisher.feed(line(R"("event":"FSDJump", "StarSystem":"X", "SystemAddress":5, "StarPos":[1.5,2.0,-3.25], "Population":0, "FuelUsed":1.2, "JumpDist":40.1)"), true);
    publisher.feed(line(R"("event":"FSSDiscoveryScan", "Progress":0.2, "BodyCount":3, "NonBodyCount":0, "SystemName":"X", "SystemAddress":5)"), true);
    publisher.feed(line(R"("event":"Scan", "ScanType":"AutoScan", "BodyName":"X A", "BodyID":0, "StarSystem":"X", "SystemAddress":5, "DistanceFromArrivalLS":0.0, "StarType":"K", "WasDiscovered":false, "WasMapped":false, "Materials":[{"Name":"iron", "Name_Localised":"Iron", "Percent":20.0}])"), true);
    expect(publisher.held_count() == 3_u) << publisher.held_count();

    // somebody lives here - never, whatever the star says
    publisher.feed(line(R"("event":"FSDJump", "StarSystem":"Y", "SystemAddress":6, "StarPos":[0,0,0], "Population":1000, "SystemFaction":{"Name":"Them"})"), true);
    publisher.feed(line(R"("event":"Scan", "ScanType":"AutoScan", "BodyName":"Y", "BodyID":0, "StarSystem":"Y", "SystemAddress":6, "DistanceFromArrivalLS":0.0, "StarType":"G", "WasDiscovered":false)"), true);
    // discovered before - not ours to announce
    publisher.feed(line(R"("event":"FSDJump", "StarSystem":"Z", "SystemAddress":7, "StarPos":[0,0,1], "Population":0)"), true);
    publisher.feed(line(R"("event":"Scan", "ScanType":"AutoScan", "BodyName":"Z", "BodyID":0, "StarSystem":"Z", "SystemAddress":7, "DistanceFromArrivalLS":0.0, "StarType":"M", "WasDiscovered":true)"), true);
    expect(publisher.held_count() == 3_u) << publisher.held_count();
    expect(sent.empty());

    // the replayed past teaches but never sends
    publisher.feed(line(R"("event":"MultiSellExplorationData", "Discovered":[ { "SystemName":"X", "NumBodies":3 } ])"), false);
    expect(sent.empty());

    publisher.feed(line(R"("event":"MultiSellExplorationData", "Discovered":[ { "SystemName":"X", "NumBodies":3 } ])"), true);
    expect(sent.size() == 3_u) << sent.size();
    expect(publisher.held_count() == 0_u);
    for(eddn::message_t const & m: sent)
      {
      expect(m.envelope.contains("/test\"")) << m.envelope;
      expect(m.envelope.contains(R"("uploaderID":"DUNKAN")")) << m.envelope;
      expect(m.envelope.contains(R"("horizons":true)")) << m.envelope;
      expect(not m.envelope.contains("_Localised")) << m.envelope;
      expect(not m.envelope.contains("FuelUsed") and not m.envelope.contains("JumpDist")) << m.envelope;
      expect(not m.envelope.contains("Progress")) << m.envelope;
      expect(m.envelope.contains("StarPos")) << m.envelope;
      }
    expect(sent[0].schema == "journal/1/test") << sent[0].schema;
    expect(sent[1].schema == "fssdiscoveryscan/1/test") << sent[1].schema;
  };

  "what was held outlives the tool"_test = [&]
  {
    std::filesystem::remove(held);
    {
    eddn::publisher_t publisher{dir, held, [](eddn::message_t &&) {}};
    publisher.feed(line(R"("event":"Commander", "FID":"F1", "Name":"DUNKAN")"), true);
    publisher.feed(line(R"("event":"FSDJump", "StarSystem":"W", "SystemAddress":8, "StarPos":[0,1,0], "Population":0)"), true);
    publisher.feed(line(R"("event":"Scan", "ScanType":"AutoScan", "BodyName":"W", "BodyID":0, "StarSystem":"W", "SystemAddress":8, "DistanceFromArrivalLS":0.0, "StarType":"F", "WasDiscovered":false)"), true);
    expect(publisher.held_count() == 2_u);
    }
    std::vector<eddn::message_t> sent;
    eddn::publisher_t again{dir, held, [&](eddn::message_t && m) { sent.push_back(std::move(m)); }};
    expect(again.held_count() == 2_u) << again.held_count();
    again.feed(line(R"("event":"Commander", "FID":"F1", "Name":"DUNKAN")"), true);
    again.feed(line(R"("event":"SellExplorationData", "Systems":[ "W" ], "Discovered":[ "W" ])"), true);
    expect(sent.size() == 2_u) << sent.size();
  };

  "an account on no list sends nothing"_test = [&]
  {
    std::filesystem::remove(held);
    std::vector<eddn::message_t> sent;
    eddn::publisher_t publisher{dir, held, [&](eddn::message_t && m) { sent.push_back(std::move(m)); }};
    publisher.feed(line(R"("event":"Commander", "FID":"F9", "Name":"STRANGER")"), true);
    publisher.feed(line(R"("event":"FSDJump", "StarSystem":"V", "SystemAddress":9, "StarPos":[0,0,0], "Population":0)"), true);
    publisher.feed(line(R"("event":"Scan", "ScanType":"AutoScan", "BodyName":"V", "BodyID":0, "StarSystem":"V", "SystemAddress":9, "DistanceFromArrivalLS":0.0, "StarType":"K", "WasDiscovered":false)"), true);
    expect(publisher.held_count() == 0_u);
    expect(sent.empty());
  };

  "the bar's stock goes at once, and only when it changed"_test = [&]
  {
    std::filesystem::remove(held);
    std::vector<eddn::message_t> sent;
    eddn::publisher_t publisher{dir, held, [&](eddn::message_t && m) { sent.push_back(std::move(m)); }};
    publisher.feed(line(R"("event":"Commander", "FID":"F2", "Name":"SJONA")"), true);
    write(
      dir / "FCMaterials.json",
      std::format(
        R"({{ "timestamp":"{}", "event":"FCMaterials", "MarketID":3700000000, "CarrierName":"C", "CarrierID":"ABC-123", "Items":[ {{ "id":1, "Name":"$x;", "Name_Localised":"X", "Price":100, "Stock":5, "Demand":0 }} ] }})",
        now
      )
    );
    publisher.feed(line(R"("event":"FCMaterials", "MarketID":3700000000, "CarrierName":"C", "CarrierID":"ABC-123")"), true);
    publisher.feed(line(R"("event":"FCMaterials", "MarketID":3700000000, "CarrierName":"C", "CarrierID":"ABC-123")"), true);
    expect(sent.size() == 1_u) << sent.size();
    expect(sent.front().schema == "fcmaterials_journal/1/test") << sent.front().schema;
    expect(not sent.front().envelope.contains("_Localised")) << sent.front().envelope;
    expect(sent.front().envelope.contains(R"("MarketID":3700000000)")) << sent.front().envelope;
  };

  for(char const * name: {"eht_settings.json", "eddn_held.jsonl", "FCMaterials.json"})
    std::filesystem::remove(dir / name);
  std::filesystem::remove(dir);
  }
