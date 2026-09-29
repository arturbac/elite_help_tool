#include <boost/ut.hpp>
#include <glare.h>

#include <vector>

auto main() -> int
  {
  using namespace boost::ut;

  "a white room is nearly all burnt out, a dark one not at all"_test = []
  {
    // nine in ten pixels white, the rest the few edges still to be seen
    std::vector<uint8_t> white(1000u, 255u);
    std::fill_n(white.begin(), 100u, 40u);
    auto const glare{glare::measure(white, 245u)};
    expect(glare.overexposed_pct > 89.9 and glare.overexposed_pct < 90.1);
    expect(glare.luma_p99 == 1.0);
    expect(glare::is_glare(glare, 60.0));

    std::vector<uint8_t> const dark(1000u, 30u);
    auto const room{glare::measure(dark, 245u)};
    expect(room.overexposed_pct == 0.0);
    expect(std::abs(room.luma_mean - 30.0 / 255.0) < 1e-9);
    expect(std::abs(room.luma_p99 - 30.0 / 255.0) < 1e-9);
    expect(not glare::is_glare(room, 60.0));
    expect(glare::measure({}, 245u).luma_mean == 0.0);
  };

  "the p99 leaves out the brightest pixel in a hundred"_test = []
  {
    std::vector<uint8_t> grey(100u, 100u);
    grey.back() = 255u;
    expect(glare::measure(grey, 245u).luma_p99 == 100.0 / 255.0);
    grey[0] = 255u;
    expect(glare::measure(grey, 245u).luma_p99 == 1.0);
  };

  "the marker is named after the machine's moment and its source"_test = []
  {
    using namespace std::chrono_literals;
    std::chrono::system_clock::time_point const at{
      std::chrono::sys_days{std::chrono::September / 29 / 2026} + 5h + 47min + 21s + 123ms
    };
    glare::marker_t marker{.ts_utc = glare::iso_utc(at)};
    expect(marker.ts_utc == std::string{"2026-09-29T05:47:21.123Z"});
    expect(glare::file_name(marker) == std::string{"2026-09-29T05:47:21.123Z_eht.json"});
    marker.game.settlement = "Simon Mining Rigs";
    std::string const json{glare::to_json(marker)};
    expect(json.contains(R"("kind": "auto-luma")"));
    expect(json.contains(R"("settlement": "Simon Mining Rigs")"));
  };
  }
