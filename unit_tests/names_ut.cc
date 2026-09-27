#include <boost/ut.hpp>
#include <elite_events.h>

auto main() -> int
  {
  using namespace boost::ut;

  "body short name"_test = []
  {
    expect(body_short_name("Bleia Eohn QT-O d7-43", "Bleia Eohn QT-O d7-43 A 2") == "A 2");
    expect(body_short_name("Bleia Eohn QT-O d7-43", "Bleia Eohn QT-O d7-43") == "");
    // on foot in a port Status.json names the port, shorter than the system
    expect(body_short_name("Bleia Eohn QT-O d7-43", "Coppel City") == "Coppel City");
  };

  "planet of a ring"_test = []
  {
    expect(planet_name_from_ring_name("18 Camelopardalis", "18 Camelopardalis AB 3 A Ring") == "AB 3");
  };
  }
