#include <boost/ut.hpp>
#include <elite_events.h>
#include <json_io.h>

#include <filesystem>
#include <fstream>

auto main() -> int
  {
  using namespace boost::ut;

  "an event read through the facade"_test = []
  {
    // the enum by its name - the adapter is in sight where the reading is instantiated
    std::string const line{
      R"({"timestamp":"2026-09-30T20:00:00Z","event":"StartJump","JumpType":"Supercruise","SomethingNew":1})"
    };
    ::events::start_jump_t jump{};
    expect(not eht::json::read_lenient(jump, line));
    expect(jump.JumpType == ::events::jump_type_e::Supercruise);
    // a field the line lacks is left as it was
    expect(not jump.StarSystem.has_value());
  };

  "a broken line says where"_test = []
  {
    ::events::start_jump_t jump{};
    auto const err{eht::json::read_lenient(jump, std::string{R"({"JumpType":"Warp"})"})};
    expect(static_cast<bool>(err));
    expect(not err.what.empty());
  };

  "a file beside the journals"_test = []
  {
    auto const path{std::filesystem::temp_directory_path() / "eht_json_io_ut_status.json"};
      {
      std::ofstream out{path};
      out << R"({"timestamp":"2026-09-30T20:00:00Z","event":"Status","Flags":16,"Flags2":0,"LegalState":"Clean"})";
      }
    ::events::status_file_t status{};
    expect(not eht::json::read_file_lenient(status, path.string()));
    expect(status.Flags == 16_ull);
    // on foot the game leaves GuiFocus out, and it must read as 0
    expect(status.GuiFocus == 0_u);
    expect(status.LegalState == "Clean");
    std::filesystem::remove(path);

    expect(static_cast<bool>(eht::json::read_file_lenient(status, path.string() + ".missing")));
  };
  }
