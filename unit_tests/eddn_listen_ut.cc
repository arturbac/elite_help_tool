#include <eddn_listen.h>

#include <boost/ut.hpp>

#include <string>

using namespace boost::ut;

namespace
  {
[[nodiscard]]
auto as_text(auto const & octets) -> std::string
  { return std::string{reinterpret_cast<char const *>(octets.data()), octets.size()}; }

// the answer the relay gave to the first eleven octets on 31.10: ZMTP 3.1
constexpr std::string_view relay_start{"\xff\x00\x00\x00\x00\x00\x00\x00\x01\x7f\x03\x01", 12};

std::string const shelf_message{R"({
  "$schemaRef": "https://eddn.edcd.io/schemas/fcmaterials_journal/1",
  "header": {"uploaderID": "x", "softwareName": "E:D Market Connector", "softwareVersion": "5"},
  "message": {
    "timestamp": "2026-10-31T20:15:00Z", "event": "FCMaterials", "horizons": true, "odyssey": true,
    "MarketID": 3706381824, "CarrierName": "Maria Sklodowska", "CarrierID": "W1V-NXM",
    "Items": [
      {"id": 128961533, "Name": "$weaponschematic_name;", "Price": 400000, "Stock": 7, "Demand": 0},
      {"id": 128961524, "Name": "$insight_name;", "Price": 9000, "Stock": 0, "Demand": 0}
    ]
  }
})"};
  }  // namespace

auto main() -> int
  {
  "our greeting: signature, 3.0, NULL, a client"_test = []
  {
    auto const g{zmtp::greeting()};
    expect(g[0] == 0xffu and g[9] == 0x7fu and g[10] == 3u and g[11] == 0u);
    expect(as_text(g).substr(12u, 4u) == "NULL" and g[16] == 0u and g[32] == 0u);
  };

  "the relay's greeting is accepted, older or another mechanism is not"_test = []
  {
    std::string peer{relay_start};
    peer += "NULL";
    peer.resize(64u, '\0');
    expect(zmtp::greeting_ok(peer));
    std::string old{peer};
    old[10] = 2;
    expect(not zmtp::greeting_ok(old));
    std::string curve{peer};
    curve.replace(12u, 5u, std::string{"CURVE"});
    expect(not zmtp::greeting_ok(curve));
    expect(not zmtp::greeting_ok(peer.substr(0u, 40u)));
  };

  "READY of a SUB and the subscription to everything"_test = []
  {
    std::string const expected_ready{"\x04\x19\x05READY\x0bSocket-Type\x00\x00\x00\x03SUB", 27};
    expect(zmtp::ready_sub() == expected_ready);
    expect(zmtp::subscribe_all() == std::string{"\x00\x01\x01", 3});
  };

  "frames: a command, a short and a long message, whole or in pieces"_test = []
  {
    zmtp::reader_t reader;
    std::string octets{zmtp::ready_sub()};
    octets += std::string{
      "\x00\x03"
      "abc",
      5
    };
    std::string long_body(300u, 'x');
    octets += std::string{"\x02\x00\x00\x00\x00\x00\x00\x01\x2c", 9} + long_body;
    reader.feed(std::string_view{octets}.substr(0u, 10u));
    expect(not reader.next());
    reader.feed(std::string_view{octets}.substr(10u));
    auto const ready{reader.next()};
    expect(fatal(ready.has_value()));
    expect(ready->command and ready->body.starts_with("\x05READY"));
    auto const short_one{reader.next()};
    expect(fatal(short_one.has_value()));
    expect(not short_one->command and not short_one->more and short_one->body == "abc");
    auto const long_one{reader.next()};
    expect(fatal(long_one.has_value()));
    expect(long_one->body == long_body);
    expect(not reader.next() and not reader.broken());
  };

  "a frame with reserved flags or too large breaks the stream"_test = []
  {
    zmtp::reader_t reserved;
    reserved.feed(
      std::string{
        "\x80\x01"
        "a",
        3
      }
    );
    expect(not reserved.next() and reserved.broken());
    zmtp::reader_t small{100u};
    small.feed(std::string{"\x00\xff", 2});
    expect(not small.next() and small.broken());
  };

  "a carrier's shelf out of a relayed message"_test = []
  {
    auto const shelf{eddn_listen::fcmaterials(shelf_message)};
    expect(fatal(shelf.has_value()));
    expect(shelf->MarketID == 3706381824u and shelf->CarrierID == "W1V-NXM");
    expect(fatal(shelf->Items.size() == 2u));
    expect(shelf->Items[0].Name == "$weaponschematic_name;" and shelf->Items[0].Stock == 7u);
    expect(shelf->Items[0].Price == 400000u and shelf->Items[1].id == 128961524u);
  };

  "the test schema, other schemas and broken text are not a shelf"_test = []
  {
    std::string test{shelf_message};
    test.replace(test.find("fcmaterials_journal/1"), 21u, "fcmaterials_journal/1/test");
    expect(not eddn_listen::fcmaterials(test));
    expect(not eddn_listen::fcmaterials(R"({"$schemaRef": "https://eddn.edcd.io/schemas/journal/1", "message": {}})"));
    expect(not eddn_listen::fcmaterials(shelf_message.substr(0u, 120u)));
    std::string no_market{shelf_message};
    no_market.replace(no_market.find("3706381824"), 10u, "0");
    expect(not eddn_listen::fcmaterials(no_market));
  };

  "a relayed market is read whole, the test schema not"_test = []
  {
    std::string const market{
      R"({"$schemaRef": "https://eddn.edcd.io/schemas/commodity/3", "header": {"uploaderID": "x"}, "message": {)"
      R"("systemName": "Bleia Eohn MQ-F b31-4", "stationName": "Fearnley Horizons", "marketId": 4380064515,)"
      R"( "horizons": true, "timestamp": "2026-11-02T21:00:00Z", "commodities": [)"
      R"({"name": "gold", "meanPrice": 47113, "buyPrice": 44540, "stock": 123, "stockBracket": 1, "sellPrice": 44000,)"
      R"( "demand": 0, "demandBracket": 0},)"
      R"({"name": "steel", "meanPrice": 4179, "buyPrice": 0, "stock": 0, "sellPrice": 5000, "demand": 900}]}})"
    };
    auto const read{eddn_listen::market(market)};
    expect(read.has_value());
    expect(read->marketId == 4380064515u and read->systemName == std::string{"Bleia Eohn MQ-F b31-4"});
    expect(read->commodities.size() == 2u and read->commodities[0].name == std::string{"gold"});
    expect(read->commodities[0].buyPrice == 44540 and read->commodities[0].stock == 123);
    expect(read->timestamp.time_since_epoch().count() != 0);

    std::string test{market};
    test.replace(test.find("commodity/3"), 11u, "commodity/3/test");
    expect(not eddn_listen::market(test));
  };

  "a relayed Docked gives the place, its type, pads and position"_test = []
  {
    std::string const docked{
      R"({"$schemaRef": "https://eddn.edcd.io/schemas/journal/1", "message": {"event": "Docked",)"
      R"( "timestamp": "2026-11-02T21:00:00Z", "StarSystem": "Sol", "SystemAddress": 10477373803, "StarPos": [0.0, 0.0, 0.0],)"
      R"( "StationName": "Abraham Lincoln", "StationType": "Orbis", "MarketID": 128016640, "DistFromStarLS": 497.5,)"
      R"( "LandingPads": {"Small": 8, "Medium": 12, "Large": 8}, "StationFaction": {"Name": "Mother Gaia"}}})"
    };
    auto const read{eddn_listen::journal(docked)};
    expect(read.has_value());
    expect(read->event == std::string{"Docked"} and read->MarketID == 128016640u);
    expect(read->LandingPads.has_value() and read->LandingPads->Large == 8u);
    expect(read->StationFaction.Name == std::string{"Mother Gaia"} and read->StationType == std::string{"Orbis"});
    expect(read->StarPos.has_value());

    expect(not eddn_listen::journal(
      R"({"$schemaRef": "https://eddn.edcd.io/schemas/journal/1", "message": {"event": "Docked", "StarSystem": "Sol"}})"
    ));
  };

  "a position is in reach of the nearest centre"_test = []
  {
    std::array<std::array<double, 3>, 2> const centres{{{0.0, 0.0, 0.0}, {100.0, 0.0, 0.0}}};
    expect(eddn_listen::in_reach({90.0, 0.0, 0.0}, centres, 50.0));
    expect(not eddn_listen::in_reach({50.0, 60.0, 0.0}, centres, 50.0));
  };
  }
