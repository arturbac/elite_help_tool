#include <boost/ut.hpp>
#include <mission_order.h>

#include <algorithm>
#include <vector>

auto main() -> int
  {
  using namespace boost::ut;
  using mission_order::before;
  using mission_order::key_t;

  "the work to do comes before what waits to be handed in, however near"_test = []
  {
    key_t const far_open{.done = false, .away = {.ly = 90.0, .ls = 10.0}};
    key_t const near_done{.done = true, .away = {.ly = 0.0, .ls = 1.0}};
    expect(before(far_open, near_done));
    expect(not before(near_done, far_open));
  };

  "then the nearest, in light years first and in Ls within a system"_test = []
  {
    key_t const here_near{.away = {.ly = 0.0, .ls = 5.0}};
    key_t const here_far{.away = {.ly = 0.0, .ls = 500.0}};
    key_t const next{.away = {.ly = 7.0, .ls = 1.0}};
    std::vector<key_t> keys{next, here_far, here_near};
    std::ranges::sort(keys, before);
    expect(keys[0].away.ls == 5.0);
    expect(keys[1].away.ls == 500.0);
    expect(keys[2].away.ly == 7.0);
  };

  "what is not known stands last, and the expiry settles the rest"_test = []
  {
    using namespace std::chrono_literals;
    key_t const nowhere{};
    key_t const known{.away = {.ly = 900.0, .ls = {}}};
    expect(before(known, nowhere)) << "a mission with no destination after those with one";
    key_t const place_known{.away = {.ly = 3.0, .ls = 900.0}};
    key_t const place_unknown{.away = {.ly = 3.0, .ls = {}}};
    expect(before(place_known, place_unknown));
    key_t const sooner{.expiry = std::chrono::sys_seconds{100s}};
    key_t const later{.expiry = std::chrono::sys_seconds{200s}};
    expect(before(sooner, later));
  };

  "the Ls from the commander in this system, from the star in another"_test = []
  {
    expect(mission_order::ls_away(100.0, true, 130.0) == 30.0);
    expect(mission_order::ls_away(100.0, false, 130.0) == 100.0);
    expect(mission_order::ls_away(100.0, true, std::nullopt) == 100.0);
    expect(not mission_order::ls_away(std::nullopt, true, 130.0).has_value());
  };
  }
