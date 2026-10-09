#include <boost/ut.hpp>
#include <events/ships.h>
#include <json_io.h>
#include <ship_health.h>
#include <vector>

auto main() -> int
  {
  using namespace boost::ut;

  "the journal's two spellings of a module are the same item"_test = []
  {
    expect(ship_health::same_item("int_hyperdrive_overcharge_size5_class5", "$int_hyperdrive_overcharge_size5_class5_name;"));
    expect(ship_health::same_item("Int_HyperDrive_Size5_Class5", "$int_hyperdrive_size5_class5_name;")) << "either case";
    expect(ship_health::same_item("int_hyperdrive_size5_class5", "int_hyperdrive_size5_class5"));
    expect(not ship_health::same_item("int_hyperdrive_size5_class5", "$int_hyperdrive_size5_class4_name;"));
    expect(not ship_health::same_item("int_hyperdrive_size5_class5", "Wear"));
  };

  "the drive is found by its slot"_test = []
  {
    std::vector<::events::module_t> modules{
      {.Slot = "PowerPlant", .Item = "int_powerplant_size6_class5", .On = true, .Priority = 1u, .Health = 1.f},
      {.Slot = "FrameShiftDrive", .Item = "int_hyperdrive_overcharge_size5_class5", .On = true, .Priority = 0u, .Health = 0.9f}
    };
    ::events::module_t const * fsd{ship_health::frame_shift_drive(std::span{std::as_const(modules)})};
    expect(fsd != nullptr and fsd->Slot == std::string{"FrameShiftDrive"});
    expect(ship_health::frame_shift_drive(std::span<::events::module_t const>{}) == nullptr);
  };

  "only an overcharge drive wears per jump, and only between readings"_test = []
  {
    ::events::module_t const sco{.Slot = "FrameShiftDrive", .Item = "int_hyperdrive_overcharge_size5_class5", .On = true, .Priority = 0u, .Health = 1.f};
    ::events::module_t const plain{.Slot = "FrameShiftDrive", .Item = "int_hyperdrive_size5_class5", .On = true, .Priority = 0u, .Health = 0.95f};

    auto const worn{ship_health::fsd_reading(sco, 10u, 0.01)};
    expect(worn.estimated);
    expect(ship_health::percent(worn.health) == 90_u) << "1 - 10 * 0.01 lands a hair under 0.9";

    auto const read{ship_health::fsd_reading(sco, 0u, 0.01)};
    expect(not read.estimated and read.health == 1.0_d);

    auto const other{ship_health::fsd_reading(plain, 10u, 0.01)};
    expect(not other.estimated);
    expect(ship_health::percent(other.health) == 95_u);

    expect(not ship_health::fsd_reading(sco, 10u, 0.0).estimated) << "0 shows the readings alone";
    expect(ship_health::fsd_reading(sco, 500u, 0.01).health == 0.0_d) << "no lower than nothing";
  };

  "the line tells a guess from a reading"_test = []
  {
    expect(ship_health::fsd_line({.health = 0.88, .estimated = true}, 12u) == std::string{"FSD ~88% est., 12 jumps"});
    expect(ship_health::fsd_line({.health = 0.99, .estimated = true}, 1u) == std::string{"FSD ~99% est., 1 jump"});
    expect(ship_health::fsd_line({.health = 0.986461, .estimated = false}, 0u) == std::string{"FSD 98%"});
    expect(ship_health::percent(0.899) == 89_u) << "rounded down";
  };

  "the journal's repairs are read"_test = []
  {
    ::events::afmu_repairs_t afmu{};
    expect(not eht::json::read_lenient(
      afmu,
      R"j({"timestamp":"2026-08-02T01:20:35Z","event":"AfmuRepairs","Module":"$int_hyperdrive_overcharge_size5_class5_name;",)j"
      R"j("Module_Localised":"FSD (SCO)","FullyRepaired":false,"Health":0.992212})j"
    ));
    expect(afmu.Module == std::string{"$int_hyperdrive_overcharge_size5_class5_name;"});
    expect(not afmu.FullyRepaired);
    expect(afmu.Health > 0.99f and afmu.Health < 0.993f);

    ::events::repair_t repair{};
    expect(not eht::json::read_lenient(
      repair, R"j({"timestamp":"2025-12-08T21:56:21Z","event":"Repair","Items":["$int_sensors_size8_class5_name;","Wear"],"Cost":3572})j"
    ));
    expect(repair.Items.size() == 2_u and repair.Cost == 3572_u);
  };
  }
