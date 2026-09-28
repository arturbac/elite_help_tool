#include <boost/ut.hpp>
#include <surface_nav.h>

#include <cmath>
#include <filesystem>
#include <fstream>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  auto const near = [](double a, double b) { return std::abs(a - b) < 1e-9; };

  "parse"_test = [&]
  {
    "two plain numbers, latitude first"_test = [&]
    {
      auto const point{nav::parse_point("12.345, -67.89")};
      expect(fatal(point.has_value()));
      expect(near(point->latitude, 12.345));
      expect(near(point->longitude, -67.89));
    };

    "decimal commas"_test = [&]
    {
      auto const point{nav::parse_point("12,345 -67,89")};
      expect(fatal(point.has_value()));
      expect(near(point->latitude, 12.345));
      expect(near(point->longitude, -67.89));
    };

    "whole numbers split by a comma"_test = [&]
    {
      auto const point{nav::parse_point("12, 45")};
      expect(fatal(point.has_value()));
      expect(near(point->latitude, 12.0));
      expect(near(point->longitude, 45.0));
    };

    "labels around the numbers"_test = [&]
    {
      auto const point{nav::parse_point("Lat: -3.25  Lon: 101.5")};
      expect(fatal(point.has_value()));
      expect(near(point->latitude, -3.25));
      expect(near(point->longitude, 101.5));
    };

    "hemisphere letters after the numbers"_test = [&]
    {
      auto const point{nav::parse_point("12.5°S, 56.25°W")};
      expect(fatal(point.has_value()));
      expect(near(point->latitude, -12.5));
      expect(near(point->longitude, -56.25));
    };

    "hemisphere letters before the numbers, longitude first"_test = [&]
    {
      auto const point{nav::parse_point("E 20.5 N 10.25")};
      expect(fatal(point.has_value()));
      expect(near(point->latitude, 10.25));
      expect(near(point->longitude, 20.5));
    };

    "a longitude past 180 counted the other way"_test = [&]
    {
      auto const point{nav::parse_point("0.5 270")};
      expect(fatal(point.has_value()));
      expect(near(point->longitude, -90.0));
    };

    "not a place"_test = []
    {
      expect(not nav::parse_point("").has_value());
      expect(not nav::parse_point("12.5").has_value());
      expect(not nav::parse_point("1 2 3").has_value());
      expect(not nav::parse_point("95.0, 10.0").has_value());
      expect(not nav::parse_point("10 N 20 S").has_value());
    };
  };

  "guide"_test = []
  {
    // a body of 1000 km radius, the target a degree to the east
    double const radius{1'000'000.0};
    auto const facing_north{nav::guide({0.0, 0.0}, 0.0, {0.0, 1.0}, radius)};
    expect(std::abs(facing_north.distance_m - 17'453.3) < 1.0) << facing_north.distance_m;
    expect(std::abs(facing_north.bearing_deg - 90.0) < 0.01);
    expect(fatal(facing_north.turn_deg.has_value()));
    expect(std::abs(*facing_north.turn_deg - 90.0) < 0.01);

    auto const facing_south_east{nav::guide({0.0, 0.0}, 135.0, {0.0, 1.0}, radius)};
    expect(std::abs(*facing_south_east.turn_deg + 45.0) < 0.01) << *facing_south_east.turn_deg;

    // across north: facing 350, the target at 10 is 20 to the right, not 340 to the left
    auto const across_north{nav::guide({0.0, 0.0}, 350.0, {1.0, 0.1763}, radius)};
    expect(std::abs(*across_north.turn_deg - 20.0) < 0.1) << *across_north.turn_deg;

    expect(not nav::guide({0.0, 0.0}, std::nullopt, {0.0, 1.0}, radius).turn_deg.has_value());
  };

  "format"_test = []
  {
    expect(nav::format_distance(850.4) == "850 m");
    expect(nav::format_distance(3'421.0) == "3.42 km");
    expect(nav::format_distance(128'400.0) == "128 km");
    expect(nav::format_turn(2.0) == "ahead");
    expect(nav::format_turn(-42.4) == "42° left");
    expect(nav::format_turn(175.0) == "behind");
    expect(nav::format_duration(40s) == "40 s");
    expect(nav::format_duration(185s) == "3 min");
    expect(nav::format_duration(4'800s) == "1 h 20 min");
  };

  "ground speed"_test = []
  {
    using clock = nav::ground_speed_t::clock;
    double const radius{1'000'000.0};
    // a degree is 17453 m, so a thousandth of it is 17.45 m
    clock::time_point const start{};
    nav::ground_speed_t speed;
    speed.push(start, {0.0, 0.0}, radius);
    expect(not speed.metres_per_second(start).has_value());
    speed.push(start + 1s, {0.0, 0.001}, radius);
    // one second is too short a window to say
    expect(not speed.metres_per_second(start + 1s).has_value());
    speed.push(start + 2s, {0.0, 0.002}, radius);
    auto const walking{speed.metres_per_second(start + 2s)};
    expect(fatal(walking.has_value()));
    expect(std::abs(*walking - 17.45) < 0.1) << *walking;

    // nothing written for longer than the window - we stopped
    expect(speed.metres_per_second(start + 10s) == std::optional{0.0});

    // another body starts from nothing
    speed.push(start + 11s, {0.0, 0.0}, 2'000'000.0);
    expect(not speed.metres_per_second(start + 11s).has_value());
  };
  
  "codex index"_test = []
  {
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / "eht_codex_index_ut"};
    std::filesystem::create_directories(dir);
    std::filesystem::path const journal{dir / "Journal.2026-09-21T200000.01.log"};
    {
    std::ofstream out{journal, std::ios::trunc};
    out << R"({ "timestamp":"2026-09-21T21:50:00Z", "event":"ApproachBody", "StarSystem":"Bleia Eohn PW-D b32-6", "SystemAddress":13861872609553, "Body":"Bleia Eohn PW-D b32-6 2", "BodyID":2 })" "\n"
        << R"({ "timestamp":"2026-09-21T21:54:55Z", "event":"CodexEntry", "EntryID":2400401, "Name":"$Codex_Ent_Osseus_04_Technetium_Name;", "Name_Localised":"Osseus Pumice - Lime", "SubCategory_Localised":"Organic structures", "Category_Localised":"Biological and Geological", "SystemAddress":13861872609553, "BodyID":2, "Latitude":-58.157578, "Longitude":-84.070427, "IsNewEntry":true })" "\n"
        // scanned again where it stands
        << R"({ "timestamp":"2026-09-21T21:56:00Z", "event":"CodexEntry", "Name_Localised":"Osseus Pumice - Lime", "SystemAddress":13861872609553, "BodyID":2, "Latitude":-58.157600, "Longitude":-84.070400 })" "\n"
        // in orbit, with no place on the ground
        << R"({ "timestamp":"2026-09-21T21:57:00Z", "event":"CodexEntry", "Name_Localised":"Ringed planet", "SystemAddress":13861872609553, "BodyID":2 })" "\n"
        // still being written
        << R"({ "timestamp":"2026-09-21T22:02:34Z", "event":"CodexEntry", "Name_Localised":"Fungoida Bullarum - Peach", "SystemAddress":13861872609553, "BodyID":2, "Latitude":-58.148270, "Longitude":-84.038795 })";
    }
    nav::codex_index_t index;
    index.update(dir);
    auto const first{index.on_body("Bleia Eohn PW-D b32-6 2")};
    expect(fatal(first.size() == 1_ul));
    expect(first.front().name == std::string{"Osseus Pumice - Lime"});
    expect(first.front().category == std::string{"Organic structures"});
    expect(first.front().first);
    expect(first.front().seen == std::string{"2026-09-21T21:54:55Z"});
    expect(index.on_body("Bleia Eohn PW-D b32-6 3").empty());

    // the line finished, and read without reading the rest again
    {
    std::ofstream out{journal, std::ios::app};
    out << "\n";
    }
    index.update(dir);
    auto const second{index.on_body("Bleia Eohn PW-D b32-6 2")};
    expect(second.size() == 2_ul);
    if(second.size() == 2u)
      expect(second.back().name == std::string{"Fungoida Bullarum - Peach"});

    std::error_code ec;
    std::filesystem::remove(journal, ec);
  };
  }
