#include <hot_drop.h>

#include <boost/ut.hpp>

#include <cmath>
#include <vector>

using namespace boost::ut;

namespace
  {
auto near(double a, double b) -> bool
  { return std::abs(a - b) < 1e-6; }

auto antoniadi() -> hot_drop::context_t
  {
  return hot_drop::context_t{
    .system = "Bleia Eohn LO-F b31-7", .station = "Antoniadi City", .market_id = 4379301379u, .ship = "panthermkii"
  };
  }

auto read_at(uint64_t ms, double ls, int32_t seconds) -> hot_drop::reading_t
  { return hot_drop::reading_t{.ms = ms, .distance_ls = ls, .seconds = seconds}; }
  }  // namespace

int main()
  {
  "the distances the HUD writes"_test = []
  {
    expect(near(*hot_drop::parse_distance_ls("238Ls"), 238.0));
    expect(near(*hot_drop::parse_distance_ls("1.32Ls"), 1.32));
    // the reader took a 5 for an S
    expect(near(*hot_drop::parse_distance_ls("53.SLs"), 53.5));
    expect(near(*hot_drop::parse_distance_ls("1.06Mm"), 1.06 / hot_drop::mm_per_ls));
    expect(near(*hot_drop::parse_distance_ls("530km"), 0.53 / hot_drop::mm_per_ls));
    expect(not hot_drop::parse_distance_ls("ANTONIADI CITY"));
    expect(not hot_drop::parse_distance_ls("> 1.00Mm/s"));
    expect(not hot_drop::parse_distance_ls("44.76LY"));
  };

  "the time to the target"_test = []
  {
    expect(*hot_drop::parse_seconds("1:45") == 105u);
    expect(*hot_drop::parse_seconds("0:05") == 5u);
    expect(*hot_drop::parse_seconds("1:02:03") == 3723u);
    // one line read alone ends with a line end
    expect(*hot_drop::parse_seconds("0:04\n") == 4u);
    expect(near(*hot_drop::parse_distance_ls("18.2Ls\n"), 18.2));
    expect(not hot_drop::parse_seconds("238Ls"));
    expect(not hot_drop::parse_seconds("1:75"));
  };

  "the time read is checked against the distances"_test = []
  {
    // 1:45 read as 4:45, and 0:05 read as 6:05 - the minutes are the estimate's, the seconds as read
    expect(hot_drop::checked_seconds(285, 104.0) == 105);
    expect(hot_drop::checked_seconds(365, 5.2) == 5);
    // 0:05 read as 0:95 - nothing near the estimate, which stands instead
    expect(hot_drop::checked_seconds(95, 5.2) == 5);
    expect(hot_drop::checked_seconds(-1, 4.4) == 4);
    expect(hot_drop::checked_seconds(285, std::nullopt) == 285);

    std::vector<hot_drop::reading_t> const earlier{read_at(0u, 30.0, 7), read_at(1500u, 25.0, 5)};
    // the furthest back within four seconds: 10 Ls in 2 s
    expect(near(*hot_drop::estimate_seconds(earlier, 2000u, 20.0), 4.0));
    // too near in time to the readings to say a speed
    expect(not hot_drop::estimate_seconds(std::vector{read_at(1500u, 25.0, 5)}, 1800u, 24.0));
    // flying away
    expect(not hot_drop::estimate_seconds(earlier, 2000u, 31.0));
  };

  "the label is the name with the distance and the time under it"_test = []
  {
    // as tesseract read the label of Antoniadi City off a 4K screen, beside a panel of other ports
    std::vector<hot_drop::text_line_t> const lines{
      {.text = "46.5Ls", .left = 100, .top = 400, .right = 180, .bottom = 422},
      {.text = "ANTONIAD! CITY", .left = 1066, .top = 434, .right = 1301, .bottom = 455},
      {.text = "238Ls", .left = 1102, .top = 486, .right = 1193, .bottom = 508},
      {.text = "1:45", .left = 1102, .top = 529, .right = 1173, .bottom = 551},
    };
    auto const label{hot_drop::read_label(lines, "Antoniadi City")};
    expect(fatal(label.has_value()));
    expect(near(label->distance_ls, 238.0));
    expect(label->seconds == 105u);
    expect(not hot_drop::read_label(lines, "Wilkinson's Chemical"));
  };

  "a drop at the port after overspeed"_test = []
  {
    hot_drop::tracker_t tracker;
    expect(not tracker.approach(1000u, antoniadi()));
    // 5 Ls/s at 25 Ls is the 0:05 the HUD writes
    expect(not tracker.reading(read_at(1000u, 30.0, 7)));
    expect(not tracker.reading(read_at(2000u, 25.0, 5)));
    expect(not tracker.reading(read_at(3000u, 21.0, 4)));
    expect(not tracker.reading(read_at(4000u, 0.02, 4)));
    auto const done{tracker.dropped(4500u, "Antoniadi City", 4379301379u)};
    expect(fatal(done.has_value()));
    expect(done->outcome == hot_drop::outcome_e::dropped);
    auto const s{hot_drop::summarise(*done)};
    expect(s.overspeed);
    expect(near(s.overspeed_from_ls, 25.0));
    // the reading in the drop zone says nothing of overspeed
    expect(s.least_seconds == 4);
    expect(s.end_speed_mm_s > 0.0);
    // supercruise over a moment later ends nothing more
    expect(not tracker.approach(5000u, std::nullopt));
  };

  "the port passed is an overshoot, and the way back a new approach"_test = []
  {
    hot_drop::tracker_t tracker;
    expect(not tracker.approach(0u, antoniadi()));
    expect(not tracker.reading(read_at(0u, 40.0, 4)));
    expect(not tracker.reading(read_at(1000u, 0.1, 3)));
    expect(not tracker.reading(read_at(1500u, 0.3, -1)));
    auto const done{tracker.reading(read_at(2000u, 0.8, -1))};
    expect(fatal(done.has_value()));
    expect(done->outcome == hot_drop::outcome_e::overshot);
    expect(tracker.active());
  };

  "supercruise over without the journal's word is broken off, once the wait is over"_test = []
  {
    hot_drop::tracker_t tracker;
    expect(not tracker.approach(0u, antoniadi()));
    expect(not tracker.reading(read_at(0u, 5.0, 7)));
    expect(not tracker.approach(1000u, std::nullopt));
    expect(not tracker.active());
    auto const done{tracker.approach(1000u + hot_drop::tracker_t::drop_wait_ms, std::nullopt)};
    expect(fatal(done.has_value()));
    expect(done->outcome == hot_drop::outcome_e::broken_off);
  };

  "an approach never read is not handed over"_test = []
  {
    hot_drop::tracker_t tracker;
    expect(not tracker.approach(0u, antoniadi()));
    expect(not tracker.dropped(100u, "Antoniadi City", 4379301379u));
  };

  "a line read back and advised from"_test = []
  {
    hot_drop::tracker_t tracker;
    expect(not tracker.approach(0u, antoniadi()));
    expect(not tracker.reading(read_at(0u, 20.0, 5)));
    expect(not tracker.reading(read_at(1000u, 1.0, 4)));
    auto const done{tracker.dropped(2000u, "Antoniadi City", 4379301379u)};
    expect(fatal(done.has_value()));
    auto const record{hot_drop::parse_attempt_line(hot_drop::attempt_line(*done))};
    expect(fatal(record.has_value()));
    expect(record->outcome == hot_drop::outcome_e::dropped);
    expect(near(record->summary.overspeed_from_ls, 20.0));

    std::vector<hot_drop::record_t> records{*record};
    records.push_back(
      hot_drop::record_t{
        .station = "Antoniadi City",
        .market_id = 4379301379u,
        .ship = "panthermkii",
        .outcome = hot_drop::outcome_e::overshot,
        .summary = {.overspeed = true, .overspeed_from_ls = 45.0, .least_seconds = 4}
      }
    );
    auto const advice{hot_drop::advise(records, "Antoniadi City", 4379301379u, "panthermkii")};
    expect(near(*advice.dropped_from_ls, 20.0));
    expect(near(*advice.overshot_from_ls, 45.0));
    expect(not hot_drop::advise(records, "Antoniadi City", 4379301379u, "mandalay").dropped_from_ls);
  };
  }
