#include <vision_recorder.h>

#include <boost/ut.hpp>

#include <unistd.h>

#include <format>
#include <fstream>

using namespace boost::ut;

namespace
  {
auto header_at(uint64_t seq, uint64_t taken_ms) -> overlay::sample_header_t
  {
  return overlay::sample_header_t{
    .seq = seq,
    .width = 64u,
    .height = 36u,
    .taken_ms = taken_ms,
    .left = 0.25f,
    .top = 0.f,
    .region_width = 0.5f,
    .region_height = 1.f,
    .surface_width = 9000u,
    .surface_height = 2160u
  };
  }

auto grey(uint32_t width, uint32_t height, uint8_t level) -> std::vector<uint8_t>
  { return std::vector<uint8_t>(size_t{width} * height * 3u, level); }
  }  // namespace

int main()
  {
  "a sample is taken only whole and new"_test = []
  {
    auto const h{header_at(4u, 1000u)};
    expect(vision::sample_valid(h, h, 2u));
    expect(not vision::sample_valid(h, h, 4u)) << "already seen";
    expect(not vision::sample_valid(h, header_at(6u, 1000u), 2u)) << "written over meanwhile";
    expect(not vision::sample_valid(header_at(5u, 1000u), header_at(5u, 1000u), 2u)) << "being written";
    auto big{h};
    big.width = overlay::sample_max_side + 1u;
    expect(not vision::sample_valid(big, big, 2u));
    auto wrong{h};
    wrong.magic = 0u;
    expect(not vision::sample_valid(wrong, wrong, 2u));
  };

  "RGBA loses its alpha"_test = []
  {
    std::vector<uint8_t> const rgba{1, 2, 3, 255, 4, 5, 6, 0};
    expect(vision::rgb_of(rgba) == std::vector<uint8_t>{1, 2, 3, 4, 5, 6});
  };

  "the sample is read from the shared file"_test = []
  {
    std::filesystem::path const file{
      std::filesystem::temp_directory_path() / std::format("eht_vision_ut_{}_sample.bin", getpid())
    };
    auto const h{header_at(8u, 1234u)};
      {
      std::ofstream out{file, std::ios::binary};
      out.write(reinterpret_cast<char const *>(&h), sizeof(h));
      std::vector<uint8_t> const rgba(size_t{h.width} * h.height * 4u, 7u);
      out.write(reinterpret_cast<char const *>(rgba.data()), std::streamsize(rgba.size()));
      }
    auto const read{vision::read_sample(file, 0u)};
    expect(fatal(read.has_value()));
    expect(read->header.taken_ms == 1234_ull);
    expect(read->rgb.size() == size_t{h.width} * h.height * 3u);
    expect(read->rgb.front() == 7_u);
    expect(not vision::read_sample(file, 8u).has_value()) << "already seen";
    std::filesystem::remove(file);
  };

  "a thumbnail is the brightness of its cells"_test = []
  {
    auto const black{vision::thumbnail(grey(64u, 36u, 0u), 64u, 36u)};
    auto const white{vision::thumbnail(grey(64u, 36u, 255u), 64u, 36u)};
    expect(black.front() == 0_u and white.back() == 255_u);
    expect(vision::difference(black, white) == 255._f);
    expect(vision::difference(white, white) == 0._f);
    // smaller than the thumbnail itself still fills every cell
    auto const tiny{vision::thumbnail(grey(4u, 2u, 100u), 4u, 2u)};
    expect(tiny.back() == 100_u);
  };

  "only a changed picture is kept, but one now and then anyway"_test = []
  {
    vision::keeper_t keeper;
    auto const dark{vision::thumbnail(grey(64u, 36u, 10u), 64u, 36u)};
    auto const light{vision::thumbnail(grey(64u, 36u, 40u), 64u, 36u)};
    expect(keeper.judge(dark, header_at(2u, 1000u), 1000u, 2.f, 10000u).keep) << "the first one";
    keeper.kept(dark, header_at(2u, 1000u));
    expect(not keeper.judge(light, header_at(4u, 1500u), 1000u, 2.f, 10000u).keep) << "too soon";
    expect(keeper.judge(light, header_at(4u, 2000u), 1000u, 2.f, 10000u).keep) << "changed";
    expect(not keeper.judge(dark, header_at(4u, 2000u), 1000u, 2.f, 10000u).keep) << "the same";
    expect(keeper.judge(dark, header_at(4u, 11000u), 1000u, 2.f, 10000u).keep) << "the same, but long ago";
    auto moved{header_at(4u, 1200u)};
    moved.region_width = 0.3f;
    expect(keeper.judge(dark, moved, 1000u, 2.f, 10000u).keep) << "another part of the screen";
    expect(keeper.judge(dark, header_at(4u, 500u), 1000u, 2.f, 10000u).keep) << "a clock started again";
  };

  "the day is the UTC one"_test = []
  {
    // 2026-10-01T23:59:59.999Z and a millisecond later
    expect(vision::day_name(1790899199999ull) == "2026-10-01");
    expect(vision::day_name(1790899200000ull) == "2026-10-02");
  };

  "the oldest days go first, today never"_test = []
  {
    std::vector<vision::day_size_t> const days{
      {.name = "2026-10-03", .bytes = 50u}, {.name = "2026-10-01", .bytes = 30u}, {.name = "2026-10-02", .bytes = 40u}
    };
    expect(vision::days_to_drop(days, 200u, "2026-10-03").empty());
    expect(vision::days_to_drop(days, 90u, "2026-10-03") == std::vector<std::string>{"2026-10-01"});
    expect(vision::days_to_drop(days, 60u, "2026-10-03") == std::vector<std::string>{"2026-10-01", "2026-10-02"});
    expect(vision::days_to_drop(days, 10u, "2026-10-03") == std::vector<std::string>{"2026-10-01", "2026-10-02"})
      << "today stays even above the limit";
  };

  "only directories named as days are measured"_test = []
  {
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / std::format("eht_vision_ut_{}", getpid())};
    std::filesystem::create_directories(dir / "2026-10-01");
    std::filesystem::create_directories(dir / "notes");
    std::ofstream{dir / "2026-10-01" / "a.png"} << "12345";
    std::ofstream{dir / "notes" / "b.txt"} << "123";
    auto const days{vision::measure_days(dir)};
    expect(fatal(days.size() == 1u));
    expect(days.front().name == "2026-10-01" and days.front().bytes == 5_ull);
    std::filesystem::remove_all(dir);
  };

  "the lines are JSON, Status.json inside them as it was"_test = []
  {
    std::string const status{R"({"timestamp":"2026-10-01T12:00:00Z","event":"Status","Flags":16842765,"GuiFocus":6})"};
    auto const flags{vision::parse_status(status)};
    expect(fatal(flags.has_value()));
    expect(flags->Flags == 16842765_ull and flags->GuiFocus == 6_u and flags->Flags2 == 0_ull);
    expect(not vision::parse_status("").has_value());
    expect(not vision::parse_status(R"({"timestamp":"2026-10-01T1)").has_value()) << "caught half written";

    std::string const line{vision::status_line(5u, status, true)};
    expect(line == std::format(R"({{"ms":5,"flags_changed":true,"status":{}}})", status)) << line;

    std::string const frame{vision::frame_line(
      vision::frame_record_t{
        .file = "1.png",
        .header = header_at(2u, 1u),
        .difference = -1.f,
        .commander = "A \"B\"",
        .socket = "overlay",
        .status_ms = 0u,
        .status = {}
      }
    )};
    expect(frame.starts_with(R"({"file":"1.png","taken_ms":1,"seq":2,"width":64,"height":36,)")) << frame;
    expect(frame.contains(R"("commander":"A \"B\"")")) << frame;
    expect(frame.ends_with(R"("status_ms":0,"status":null})")) << frame;

    expect(
      vision::event_line(7u, "2026-10-01T12:00:00Z", "FSDJump")
      == R"({"ms":7,"timestamp":"2026-10-01T12:00:00Z","event":"FSDJump"})"
    );
  };

  "the journal is followed from its end, a newer one from its beginning"_test = []
  {
    std::filesystem::path const dir{
      std::filesystem::temp_directory_path() / std::format("eht_vision_ut_journal_{}", getpid())
    };
    std::filesystem::create_directories(dir);
      {
      std::ofstream out{dir / "Journal.2026-10-01T100000.01.log"};
      out << R"({ "timestamp":"2026-10-01T10:00:00Z", "event":"Commander", "FID":"F1", "Name":"Sjona" })" << '\n';
      out << R"({ "timestamp":"2026-10-01T10:00:01Z", "event":"Music" })" << '\n';
      }
    vision::journal_tail_t tail{dir};
    expect(tail.poll().empty()) << "its past is not followed";
    expect(tail.commander() == "Sjona");
      {
      std::ofstream out{dir / "Journal.2026-10-01T100000.01.log", std::ios::app};
      out << R"({ "timestamp":"2026-10-01T10:00:02Z", "event":"FSDJump" })" << '\n';
      out << R"({ "timestamp":"2026-10-01T10:00:03Z", "ev)";
      }
    auto const lines{tail.poll()};
    expect(fatal(lines.size() == 1u));
    expect(vision::parse_event(lines.front())->event == "FSDJump");
      {
      std::ofstream out{dir / "Journal.2026-10-01T100000.01.log", std::ios::app};
      out << R"(ent":"Scan" })" << '\n';
      }
    auto const rest{tail.poll()};
    expect(fatal(rest.size() == 1u));
    expect(vision::parse_event(rest.front())->event == "Scan") << "a line written in two goes is one line";
    std::filesystem::remove_all(dir);
  };

  "a picture is written whole"_test = []
  {
    std::filesystem::path const file{
      std::filesystem::temp_directory_path() / std::format("eht_vision_ut_{}.png", getpid())
    };
    expect(vision::write_png(file, grey(8u, 4u, 128u), 8u, 4u));
    std::ifstream in{file, std::ios::binary};
    std::string magic(8u, '\0');
    in.read(magic.data(), 8);
    expect(magic == "\x89PNG\r\n\x1a\n");
    expect(not std::filesystem::exists(file.string() + ".part"));
    expect(not vision::write_png(file, grey(2u, 2u, 1u), 8u, 4u)) << "too few pixels";
    std::filesystem::remove(file);
  };
  }
