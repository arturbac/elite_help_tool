#include <boost/ut.hpp>
#include <sensors.h>

#include <fstream>

namespace
  {
auto put(std::filesystem::path const & path, std::string_view text) -> void
  {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream{path} << text << '\n';
  }

///\brief a /sys of its own: two cards, a processor, a disk with a dummy sensor
[[nodiscard]]
auto fake_sys() -> std::filesystem::path
  {
  std::filesystem::path const sys{std::filesystem::temp_directory_path() / std::format("eht_sensors_ut_{}", ::getpid())};
  std::filesystem::path const hwmon{sys / "class" / "hwmon"};
  // the processor's own graphics - little memory
  put(hwmon / "hwmon4" / "name", "amdgpu");
  put(hwmon / "hwmon4" / "temp1_input", "46000");
  put(hwmon / "hwmon4" / "temp1_label", "edge");
  put(hwmon / "hwmon4" / "device" / "mem_info_vram_total", "536870912");
  put(hwmon / "hwmon4" / "device" / "power" / "runtime_status", "active");
  // the card the game draws on
  put(hwmon / "hwmon3" / "name", "amdgpu");
  put(hwmon / "hwmon3" / "temp1_input", "45000");
  put(hwmon / "hwmon3" / "temp1_label", "edge");
  put(hwmon / "hwmon3" / "temp2_input", "55000");
  put(hwmon / "hwmon3" / "temp2_label", "junction");
  put(hwmon / "hwmon3" / "temp2_crit", "110000");
  put(hwmon / "hwmon3" / "device" / "mem_info_vram_total", "17163091968");
  put(hwmon / "hwmon3" / "device" / "power" / "runtime_status", "active");
  put(hwmon / "hwmon2" / "name", "k10temp");
  put(hwmon / "hwmon2" / "temp1_input", "50250");
  put(hwmon / "hwmon2" / "temp1_label", "Tctl");
  put(hwmon / "hwmon2" / "temp3_input", "37500");
  put(hwmon / "hwmon2" / "temp3_label", "Tccd1");
  put(hwmon / "hwmon2" / "temp4_input", "41000");
  put(hwmon / "hwmon2" / "temp4_label", "Tccd2");
  put(hwmon / "hwmon0" / "name", "nvme");
  put(hwmon / "hwmon0" / "temp2_input", "103850");
  put(hwmon / "hwmon0" / "temp2_max", "-273150");
  return sys;
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  "millidegrees are read, dummies and garbage are not"_test = []
  {
    expect(sensors::parse_millidegrees("55000\n") == 55.0);
    expect(sensors::parse_millidegrees("-5500") == -5.5);
    expect(not sensors::parse_millidegrees("-273150").has_value());
    expect(not sensors::parse_millidegrees("200000").has_value());
    expect(not sensors::parse_millidegrees("").has_value());
    expect(not sensors::parse_millidegrees("55a").has_value());
  };

  "warm below critical, and each border left only some degrees under it"_test = []
  {
    using sensors::level_e;
    auto const level = [](double c, level_e before) { return sensors::level_of(c, 110.0, 10.0, 3.0, before); };
    expect(level(99.0, level_e::normal) == level_e::normal);
    expect(level(100.0, level_e::normal) == level_e::warm);
    expect(level(98.0, level_e::warm) == level_e::warm);
    expect(level(96.0, level_e::warm) == level_e::normal);
    expect(level(110.0, level_e::warm) == level_e::critical);
    expect(level(108.0, level_e::critical) == level_e::critical);
    expect(level(106.0, level_e::critical) == level_e::warm);
  };

  "the card with the most memory by its hottest spot, the processor by its hottest die"_test = []
  {
    std::filesystem::path const sys{fake_sys()};
    sensors::found_t found{sensors::discover(sys)};
    expect(found.gpu.has_value() and found.gpu->input.filename() == "temp2_input");
    expect(found.gpu.has_value() and found.gpu->critical == 110.0);
    expect(found.cpu.size() == 2u);
    sensors::reader_t reader{std::move(found)};
    auto const now{reader.read()};
    expect(now.gpu.has_value() and now.gpu->celsius == 55.0);
    expect(now.cpu.has_value() and now.cpu->celsius == 41.0);

    using namespace std::chrono_literals;
    std::chrono::system_clock::time_point const at{std::chrono::sys_days{std::chrono::September / 29 / 2026} + 5h + 47min};
    expect(
      sensors::log_line(at, now, reader.found())
      == std::string{
        R"({"ts_utc":"2026-09-29T05:47:00.000Z","gpu_c":55.0,"gpu_sensor":"junction","pci":"device","cpu_c":41.0,"cpu_sensor":"k10temp"})"
      }
    );
    expect(sensors::log_line(at, {}, {}).contains(R"("gpu_c":null)"));

    // a sleeping card is not read
    put(sys / "class" / "hwmon" / "hwmon3" / "device" / "power" / "runtime_status", "suspended");
    expect(not reader.read().gpu.has_value());
    std::error_code ec;
    std::filesystem::remove_all(sys, ec);
  };

  "no sensors at all is no reading, not a failure"_test = []
  {
    auto const found{sensors::discover("/nonexistent-sys")};
    expect(not found.gpu.has_value() and found.cpu.empty());
    sensors::reader_t reader{found};
    auto const now{reader.read()};
    expect(not now.gpu.has_value() and not now.cpu.has_value());
  };
  }
