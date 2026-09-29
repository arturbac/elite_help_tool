#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

///\brief the temperatures of the graphics card and the processor, read the way any user may
///
/// Linux gives every sensor under /sys/class/hwmon, readable by everyone (0444) - no root, no group, nothing
/// written. The hwmonN numbers change from one boot to the next, so a sensor is known by its driver's name and
/// the device it belongs to. The files are opened once and read again with pread, which sysfs refreshes. A
/// driver that hides its sensors, a missing file or an error is simply no reading - never a reason to fail.
/// A card from NVIDIA with its own driver has no hwmon; its library, when installed, is asked instead
namespace sensors
  {
///\brief one temperature, with the level the driver calls critical when it gives one
struct reading_t
  {
  double celsius{};
  std::optional<double> critical;
  };

struct temperatures_t
  {
  std::optional<reading_t> gpu;
  std::optional<reading_t> cpu;
  };

///\brief a sensor found - the file and what the driver says of it
struct source_t
  {
  std::filesystem::path input;
  std::optional<double> critical;
  ///\brief the PCI device's power state, read before the sensor: a sleeping card is not woken for a number
  std::filesystem::path runtime_status;
  };

///\brief the sensors found, one for each - the graphics card's hottest spot and the processor's dies
struct found_t
  {
  std::optional<source_t> gpu;
  std::vector<source_t> cpu;
  std::string gpu_name;
  std::string cpu_name;
  };

///\brief looks through <sys>/class/hwmon once - sys is /sys but for a test
///\detail The graphics card is the one with the most video memory - the one a game draws on beside a
/// processor's own. Of its temperatures the junction (the hottest spot, what the card slows down by) is
/// taken, the edge when there is none. For the processor each die (Tccd) of k10temp, else its Tdie or Tctl,
/// zenpower's Tdie, or coretemp's package; the hottest of them is shown
[[nodiscard]]
auto discover(std::filesystem::path const & sys) -> found_t;

///\brief a temperature in millidegrees as sysfs writes it; nothing for a sensor that is not there or a dummy
///\detail some sensors give -273.15 or a value no chip survives - 103 C with an unset limit is what one NVMe says
[[nodiscard]]
auto parse_millidegrees(std::string_view text) -> std::optional<double>;

///\brief how warm a reading is against its limit
enum struct level_e : uint8_t
  {
  normal,
  warm,
  critical
  };

///\brief the level, with a margin before critical, and some degrees of hysteresis so the colour does not
/// flicker on the border
[[nodiscard]]
auto level_of(double celsius, double critical, double warn_margin, double hysteresis, level_e before) noexcept
  -> level_e;

///\brief reads the sensors found - a file at a time, each opened once and kept open
class reader_t final
  {
public:
  explicit reader_t(found_t found);
  reader_t(reader_t const &) = delete;
  auto operator=(reader_t const &) -> reader_t & = delete;
  ~reader_t();

  [[nodiscard]]
  auto read() -> temperatures_t;

  [[nodiscard]]
  auto found() const noexcept -> found_t const &
    { return found_; }

private:
  found_t found_;
  int gpu_fd_{-1};
  std::vector<int> cpu_fds_;
  ///\brief the NVIDIA library, when the card has no hwmon and the library is there
  struct nvml_t;
  nvml_t * nvml_{};
  };
  }  // namespace sensors
