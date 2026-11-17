#include <atmosphere_print.h>

#include <boost/ut.hpp>

#include <vector>

using namespace boost::ut;
using namespace std::string_view_literals;

auto main() -> int
  {
  using atmosphere_print::planet_t;
  using atmosphere_print::read_t;
  using atmosphere_print::verdict_e;

  // Spoihee US-J c25-17: 5 thin nitrogen (read 2026-11-17, a known print), 2 nitrogen too, 4 carbon dioxide
  std::vector<planet_t> const planets{
    {"Spoihee US-J c25-17 2"sv, 4302943.5, "Nitrogen"sv},
    {"Spoihee US-J c25-17 4"sv, 2968214.25, "CarbonDioxide"sv},
    {"Spoihee US-J c25-17 5"sv, 2947339.5, "Nitrogen"sv}};

  "a known print of the journal's kind, the planet by its radius as a float"_test = [&]
  {
    read_t const read{.ok = true, .known = true, .kind = "Nitrogen", .radius = 2947340.f, .r_g = 0.6953f, .b_g = 1.7551f};
    auto const result{atmosphere_print::judge(read, planets)};
    expect(fatal(result.has_value()));
    expect(result->name == "Spoihee US-J c25-17 5"sv and result->verdict == verdict_e::known);
  };

  "a print edworld does not know is a case never seen"_test = [&]
  {
    read_t const read{.ok = true, .known = false, .radius = 2968214.f, .r_g = 0.70f, .b_g = 1.70f};
    auto const result{atmosphere_print::judge(read, planets)};
    expect(fatal(result.has_value()));
    expect(result->atmosphere_type == "CarbonDioxide"sv and result->verdict == verdict_e::unknown);
  };

  "a known print of another kind, and a kind's second print"_test = [&]
  {
    read_t differs{.ok = true, .known = true, .kind = "Argon", .radius = 2968214.f};
    expect(atmosphere_print::judge(differs, planets)->verdict == verdict_e::differs);
    read_t const second{.ok = true, .known = true, .second = true, .kind = "Nitrogen", .radius = 4302943.f};
    expect(atmosphere_print::judge(second, planets)->verdict == verdict_e::second);
  };

  "no hue, or a planet of another system: nothing"_test = [&]
  {
    expect(not atmosphere_print::judge(read_t{.ok = false, .radius = 2947340.f}, planets).has_value());
    expect(not atmosphere_print::judge(read_t{.ok = true, .known = true, .kind = "Nitrogen", .radius = 1000000.f}, planets).has_value());
  };
  }
