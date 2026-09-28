#include <boost/ut.hpp>
#include <picture_records.h>

#include <filesystem>
#include <fstream>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  "names"_test = []
  {
    expect(pictures::file_safe("Leamue MS-U f2-3706 A") == std::string{"Leamue_MS-U_f2-3706_A"});
    std::chrono::sys_seconds const at{std::chrono::sys_days{std::chrono::year{2026} / 9 / 28} + 22h + 35min + 12s};
    expect(pictures::stem(at, "Leamue MS-U f2-3706") == std::string{"20260928-223512_Leamue_MS-U_f2-3706"});
    expect(
      pictures::star_detail("H", 0u, "VII", 6.79, 0.0, 20'000.0) == std::string{"H0 VII, 6.79 solar masses, radius 20 km"}
    ) << "no nought kelvin for a black hole";
    expect(
      pictures::planet_detail("Rocky body", "thin carbon dioxide atmosphere", 9.80665, 180.4)
      == std::string{"Rocky body, thin carbon dioxide atmosphere, 1.00 g, 180 K"}
    );
  };

  "rebuilt from the journals"_test = []
  {
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / "eht_picture_records_ut"};
    std::filesystem::create_directories(dir);
    std::filesystem::path const journal{dir / "Journal.2026-09-28T200000.01.log"};
    {
    std::ofstream out{journal, std::ios::trunc};
    out << R"({ "timestamp":"2026-09-28T22:35:12Z", "event":"FSDJump", "StarSystem":"Leamue MS-U f2-3706", "SystemAddress":77 })" "\n"
        << R"({ "timestamp":"2026-09-28T22:35:17Z", "event":"Scan", "ScanType":"AutoScan", "BodyName":"Leamue MS-U f2-3706", "BodyID":0, "StarSystem":"Leamue MS-U f2-3706", "SystemAddress":77, "DistanceFromArrivalLS":0.0, "StarType":"H", "Subclass":0, "StellarMass":6.79, "Radius":20000.0, "SurfaceTemperature":0.0, "Luminosity":"VII", "WasDiscovered":false })" "\n"
        << R"({ "timestamp":"2026-09-28T22:40:00Z", "event":"Scan", "ScanType":"Detailed", "BodyName":"Leamue MS-U f2-3706 1", "BodyID":1, "StarSystem":"Leamue MS-U f2-3706", "SystemAddress":77, "DistanceFromArrivalLS":120.0, "PlanetClass":"Rocky body", "Atmosphere":"", "SurfaceGravity":9.80665, "SurfaceTemperature":180.4, "WasDiscovered":true })" "\n"
        << R"({ "timestamp":"2026-09-28T22:45:30Z", "event":"SAAScanComplete", "BodyName":"Leamue MS-U f2-3706 1", "SystemAddress":77, "BodyID":1 })" "\n"
        << R"({ "timestamp":"2026-09-28T22:50:00Z", "event":"Touchdown", "SystemAddress":77, "Body":"Leamue MS-U f2-3706 1", "BodyID":1 })" "\n"
        << R"({ "timestamp":"2026-09-28T22:52:56Z", "event":"ScanOrganic", "ScanType":"Log", "Genus_Localised":"Aleoida", "Species_Localised":"Aleoida Coronamus", "Variant_Localised":"Aleoida Coronamus - Emerald", "SystemAddress":77, "Body":1 })" "\n";
    }

    auto const sky{pictures::rebuild_sky(
      {"sky/Leamue_MS-U_f2-3706/20260928-223512_Leamue_MS-U_f2-3706-1.0s.jpg",
       "sky/Leamue_MS-U_f2-3706/20260928-224530_Leamue_MS-U_f2-3706_1.jpg",
       "sky/Somewhere/20260101-000000_Nothing.jpg"},
      dir
    )};
    expect(fatal(sky.size() == 3_ul));
    expect(sky[0].kind == std::string{"star"});
    expect(sky[0].body == std::string{"Leamue MS-U f2-3706"});
    expect(sky[0].detail == std::string{"H0 VII, 6.79 solar masses, radius 20 km"});
    expect(sky[0].first);
    expect(sky[0].taken == std::string{"2026-09-28 22:35:12  (1.0s after)"});
    expect(sky[1].kind == std::string{"planet"});
    expect(sky[1].body == std::string{"Leamue MS-U f2-3706 1"});
    expect(sky[1].system == std::string{"Leamue MS-U f2-3706"});
    expect(sky[1].detail == std::string{"Rocky body, 1.00 g, 180 K"});
    expect(not sky[1].first);
    // no journal knows it - it stays listed with what its name says
    expect(sky[2].kind.empty());
    expect(sky[2].system == std::string{"Somewhere"});

    auto const codex{pictures::rebuild_codex({"pictures/20260928-225256_Aleoida_Coronamus_128.jpg"}, dir)};
    expect(fatal(codex.size() == 1_ul));
    expect(codex[0].species == std::string{"Aleoida Coronamus"});
    expect(codex[0].variant == std::string{"Aleoida Coronamus - Emerald"});
    expect(codex[0].genus == std::string{"Aleoida"});
    expect(codex[0].scan == std::string{"Log"});
    expect(codex[0].system == std::string{"Leamue MS-U f2-3706"});
    expect(codex[0].body == std::string{"Leamue MS-U f2-3706 1"});
    expect(codex[0].body_id == 1_u);
    expect(codex[0].taken == std::string{"2026-09-28 22:52:56"});

    std::error_code ec;
    std::filesystem::remove(journal, ec);
  };
  }
