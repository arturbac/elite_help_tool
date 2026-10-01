#include <boost/ut.hpp>
#include <events/companion_files.h>
#include <events/navigation.h>
#include <events/exploration.h>
#include <star_system.h>
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

  "a codex find in space is a visit to the phenomenon"_test = []
  {
    std::string const space{
      R"({"timestamp":"2026-10-01T20:00:00Z","event":"CodexEntry","EntryID":2100101,"Name":"$Codex_Ent_L_Cld_Name;",)"
      R"("Name_Localised":"Lagrange Cloud","SubCategory":"$Codex_SubCategory_Geology_and_Anomalies;",)"
      R"("Category":"$Codex_Category_Biology;","Region":"$Codex_RegionName_1;","System":"Asura",)"
      R"("SystemAddress":12274907287851,"IsNewEntry":true})"
    };
    ::events::codex_entry_t entry{};
    expect(not eht::json::read_lenient(entry, space));
    auto const seen{std::chrono::sys_seconds{std::chrono::days{20'000}}};
    auto const visit{to_system_signal(entry, seen)};
    expect(fatal(visit.has_value()));
    expect(visit->signal_type == phenomenon_visit_type);
    expect(visit->name == "Lagrange Cloud");
    expect(visit->system_address == 12274907287851_ull);

    // the same geology on the ground has its place, and a star is a body - neither is a phenomenon
    ::events::codex_entry_t ground{entry};
    ground.Latitude = 12.5;
    expect(not to_system_signal(ground, seen).has_value());
    ::events::codex_entry_t star{entry};
    star.Category = "$Codex_Category_StellarBodies;";
    expect(not to_system_signal(star, seen).has_value());
  };

  "a visit outlasts the signals of later visits"_test = []
  {
    using namespace std::chrono;
    sys_seconds const then{days{20'000}};
    sys_seconds const now{then + days{30}};
    std::vector<system_signal_t> rows{
      system_signal_t{.system_address = 1, .name = "Notable stellar phenomena", .signal_type = "Codex", .is_station = false, .last_seen = now},
      system_signal_t{.system_address = 1, .name = "Old beacon", .signal_type = "NavBeacon", .is_station = false, .last_seen = then},
      system_signal_t{.system_address = 1, .name = "Lagrange Cloud", .signal_type = std::string{phenomenon_visit_type}, .is_station = false, .last_seen = then}
    };
    auto const kept{filter_current_visit(std::move(rows))};
    expect(kept.size() == 2_u);
    expect(std::ranges::contains(kept, "Lagrange Cloud", &system_signal_t::name));
    expect(not std::ranges::contains(kept, "Old beacon", &system_signal_t::name));
  };
  }
