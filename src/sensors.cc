#include <sensors.h>

#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <format>
#include <iterator>

namespace sensors
  {
namespace
  {
///\brief the first line of a small sysfs file, empty when it cannot be read
[[nodiscard]]
auto first_line(std::filesystem::path const & path) -> std::string
  {
  std::ifstream file{path};
  std::string line;
  if(file)
    std::getline(file, line);
  return line;
  }

[[nodiscard]]
auto number_of(std::filesystem::path const & path) -> std::optional<uint64_t>
  {
  std::string const text{first_line(path)};
  uint64_t value{};
  if(text.empty() or std::from_chars(text.data(), text.data() + text.size(), value).ec != std::errc{})
    return std::nullopt;
  return value;
  }

///\brief a temperature of a hwmon directory: tempN_input with its label and its critical level
struct temperature_t
  {
  std::string label;
  std::filesystem::path input;
  std::optional<double> critical;
  };

[[nodiscard]]
auto temperatures_of(std::filesystem::path const & hwmon) -> std::vector<temperature_t>
  {
  std::vector<temperature_t> found;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{hwmon, ec})
    {
    std::string const name{entry.path().filename().string()};
    if(not name.starts_with("temp") or not name.ends_with("_input"))
      continue;
    std::string const stem{name.substr(0u, name.size() - std::string_view{"_input"}.size())};
    // a sensor that reads nothing now is left out - a card asleep, a chip that is not there
    if(not parse_millidegrees(first_line(entry.path())))
      continue;
    std::optional<double> critical{parse_millidegrees(first_line(hwmon / (stem + "_crit")))};
    if(not critical)
      critical = parse_millidegrees(first_line(hwmon / (stem + "_max")));
    found.push_back(
      temperature_t{.label = first_line(hwmon / (stem + "_label")), .input = entry.path(), .critical = critical}
    );
    }
  std::ranges::sort(found, {}, &temperature_t::input);
  return found;
  }

[[nodiscard]]
auto labelled(std::vector<temperature_t> const & all, std::string_view label) -> temperature_t const *
  {
  auto const it{std::ranges::find(all, label, &temperature_t::label)};
  return it != all.end() ? &*it : nullptr;
  }

[[nodiscard]]
auto source_of(temperature_t const & t, std::filesystem::path runtime_status = {}) -> source_t
  { return source_t{.input = t.input, .critical = t.critical, .runtime_status = std::move(runtime_status)}; }

[[nodiscard]]
auto read_fd(int fd) -> std::optional<double>
  {
  if(fd < 0)
    return std::nullopt;
  std::array<char, 32> buffer{};
  ssize_t const size{::pread(fd, buffer.data(), buffer.size() - 1u, 0)};
  if(size <= 0)
    return std::nullopt;
  return parse_millidegrees(std::string_view{buffer.data(), size_t(size)});
  }

[[nodiscard]]
auto open_read(std::filesystem::path const & path) -> int
  { return path.empty() ? -1 : ::open(path.c_str(), O_RDONLY | O_CLOEXEC); }

[[nodiscard]]
auto suspended(std::filesystem::path const & runtime_status) -> bool
  { return not runtime_status.empty() and first_line(runtime_status) == "suspended"; }
  }  // namespace

auto parse_millidegrees(std::string_view text) -> std::optional<double>
  {
  while(not text.empty() and (text.back() == '\n' or text.back() == ' '))
    text.remove_suffix(1u);
  int64_t value{};
  auto const [end, error]{std::from_chars(text.data(), text.data() + text.size(), value)};
  if(text.empty() or error != std::errc{} or end != text.data() + text.size())
    return std::nullopt;
  double const celsius{double(value) / 1000.0};
  if(celsius < -40.0 or celsius > 150.0)
    return std::nullopt;
  return celsius;
  }

auto level_of(double celsius, double critical, double warn_margin, double hysteresis, level_e before) noexcept
  -> level_e
  {
  // going up the borders are where they are; coming down each is left only some degrees below it
  double const warm{critical - warn_margin};
  double const back{before == level_e::normal ? 0.0 : hysteresis};
  if(celsius >= critical - (before == level_e::critical ? back : 0.0))
    return level_e::critical;
  if(celsius >= warm - (before != level_e::normal ? back : 0.0))
    return level_e::warm;
  return level_e::normal;
  }

auto log_line(std::chrono::system_clock::time_point at, temperatures_t const & reading, found_t const & found)
  -> std::string
  {
  auto const number = [](std::optional<reading_t> const & value)
  { return value ? std::format("{:.1f}", value->celsius) : std::string{"null"}; };
  // names from sysfs are plain words and addresses - nothing in them needs escaping
  return std::format(
    R"({{"ts_utc":"{:%FT%T}Z","gpu_c":{},"gpu_sensor":"{}","pci":"{}","cpu_c":{},"cpu_sensor":"{}"}})",
    std::chrono::floor<std::chrono::milliseconds>(at),
    number(reading.gpu),
    found.gpu_sensor,
    found.gpu_pci,
    number(reading.cpu),
    found.cpu_name
  );
  }

auto discover(std::filesystem::path const & sys) -> found_t
  {
  found_t found;
  uint64_t most_memory{};
  std::error_code ec;
  std::vector<std::filesystem::path> hwmons;
  for(auto const & entry: std::filesystem::directory_iterator{sys / "class" / "hwmon", ec})
    hwmons.push_back(entry.path());
  std::ranges::sort(hwmons);

  for(std::filesystem::path const & hwmon: hwmons)
    {
    std::string const driver{first_line(hwmon / "name")};
    std::filesystem::path const device{hwmon / "device"};
    if(driver == "amdgpu" or driver == "nouveau" or driver == "xe" or driver == "i915" or driver == "radeon")
      {
      std::filesystem::path const status{device / "power" / "runtime_status"};
      // another card asleep - a laptop's - is not to be woken by reading its sensors
      if(suspended(status))
        continue;
      auto const all{temperatures_of(hwmon)};
      temperature_t const * chosen{labelled(all, "junction")};
      if(chosen == nullptr)
        chosen = labelled(all, "edge");
      if(chosen == nullptr and not all.empty())
        chosen = &all.front();
      if(chosen == nullptr)
        continue;
      uint64_t const memory{number_of(device / "mem_info_vram_total").value_or(0u)};
      if(not found.gpu or memory > most_memory)
        {
        most_memory = memory;
        found.gpu = source_of(*chosen, std::filesystem::exists(status, ec) ? status : std::filesystem::path{});
        found.gpu_pci = std::filesystem::canonical(device, ec).filename().string();
        found.gpu_name = std::format("{} {}", driver, found.gpu_pci);
        found.gpu_sensor = chosen->label;
        }
      continue;
      }

    if(not found.cpu.empty())
      continue;
    auto const all{temperatures_of(hwmon)};
    if(driver == "k10temp")
      {
      for(temperature_t const & t: all)
        if(t.label.starts_with("Tccd"))
          found.cpu.push_back(source_of(t));
      // an older Ryzen has only Tctl, which some carry with an offset - Tdie, when there, is without it
      if(found.cpu.empty())
        if(temperature_t const * t{labelled(all, "Tdie")}; t != nullptr)
          found.cpu.push_back(source_of(*t));
      if(found.cpu.empty())
        if(temperature_t const * t{labelled(all, "Tctl")}; t != nullptr)
          found.cpu.push_back(source_of(*t));
      }
    else if(driver == "zenpower")
      {
      if(temperature_t const * t{labelled(all, "Tdie")}; t != nullptr)
        found.cpu.push_back(source_of(*t));
      }
    else if(driver == "coretemp")
      {
      for(temperature_t const & t: all)
        if(t.label.starts_with("Package id"))
          found.cpu.push_back(source_of(t));
      }
    if(not found.cpu.empty())
      found.cpu_name = driver;
    }

  // without a hwmon for the processor the thermal zone of its package, as some systems give only that
  if(found.cpu.empty())
    for(auto const & entry: std::filesystem::directory_iterator{sys / "class" / "thermal", ec})
      if(std::string const type{first_line(entry.path() / "type")}; type == "x86_pkg_temp" or type == "cpu-thermal")
        if(parse_millidegrees(first_line(entry.path() / "temp")))
          {
          found.cpu.push_back(source_t{.input = entry.path() / "temp", .critical = {}, .runtime_status = {}});
          found.cpu_name = type;
          break;
          }
  return found;
  }

///\brief the few calls of NVIDIA's library that give a card's temperature
struct reader_t::nvml_t
  {
  using init_t = int (*)();
  using handle_t = int (*)(unsigned, void **);
  using temperature_t = int (*)(void *, int, unsigned *);
  using shutdown_t = int (*)();

  void * library{};
  void * device{};
  temperature_t temperature{};
  shutdown_t shutdown{};

  [[nodiscard]]
  static auto open() -> nvml_t *
    {
    void * const library{::dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL)};
    if(library == nullptr)
      return nullptr;
    auto const init{reinterpret_cast<init_t>(::dlsym(library, "nvmlInit_v2"))};
    auto const handle{reinterpret_cast<handle_t>(::dlsym(library, "nvmlDeviceGetHandleByIndex_v2"))};
    auto const temperature{reinterpret_cast<temperature_t>(::dlsym(library, "nvmlDeviceGetTemperature"))};
    auto const shutdown{reinterpret_cast<shutdown_t>(::dlsym(library, "nvmlShutdown"))};
    void * device{};
    if(init == nullptr or handle == nullptr or temperature == nullptr or init() != 0)
      {
      ::dlclose(library);
      return nullptr;
      }
    if(handle(0u, &device) != 0)
      {
      if(shutdown != nullptr)
        shutdown();
      ::dlclose(library);
      return nullptr;
      }
    return new nvml_t{.library = library, .device = device, .temperature = temperature, .shutdown = shutdown};
    }

  [[nodiscard]]
  auto read() const -> std::optional<double>
    {
    unsigned celsius{};
    // NVML_TEMPERATURE_GPU is 0
    if(temperature(device, 0, &celsius) != 0)
      return std::nullopt;
    return double(celsius);
    }

  ~nvml_t()
    {
    if(shutdown != nullptr)
      shutdown();
    ::dlclose(library);
    }
  };

reader_t::reader_t(found_t found) : found_{std::move(found)}
  {
  if(found_.gpu)
    gpu_fd_ = open_read(found_.gpu->input);
  else
    {
    nvml_ = nvml_t::open();
    if(nvml_ != nullptr)
      found_.gpu_name = "nvidia";
    }
  for(source_t const & source: found_.cpu)
    if(int const fd{open_read(source.input)}; fd >= 0)
      cpu_fds_.push_back(fd);
  }

reader_t::~reader_t()
  {
  if(gpu_fd_ >= 0)
    ::close(gpu_fd_);
  for(int const fd: cpu_fds_)
    ::close(fd);
  delete nvml_;
  }

auto reader_t::read() -> temperatures_t
  {
  temperatures_t result;
  if(found_.gpu and not suspended(found_.gpu->runtime_status))
    if(auto const celsius{read_fd(gpu_fd_)}; celsius)
      result.gpu = reading_t{.celsius = *celsius, .critical = found_.gpu->critical};
  if(nvml_ != nullptr)
    if(auto const celsius{nvml_->read()}; celsius)
      result.gpu = reading_t{.celsius = *celsius, .critical = std::nullopt};

  std::optional<double> hottest;
  for(int const fd: cpu_fds_)
    if(auto const celsius{read_fd(fd)}; celsius and (not hottest or *celsius > *hottest))
      hottest = celsius;
  if(hottest)
    result.cpu = reading_t{
      .celsius = *hottest, .critical = found_.cpu.empty() ? std::nullopt : found_.cpu.front().critical
    };
  return result;
  }
  }  // namespace sensors
