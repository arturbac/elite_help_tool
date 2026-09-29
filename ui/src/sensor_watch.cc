#include <sensor_watch.h>
#include <eht_settings.h>
#include <backup.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>

sensor_watch_t::sensor_watch_t()
  {
  worker_ = std::jthread{[this](std::stop_token stoken)
                         {
                           try
                             {
                             std::optional<sensors::reader_t> reader;
                             std::mutex sleep_mutex;
                             std::condition_variable_any wake;
                             while(not stoken.stop_requested())
                               {
                               auto const cfg{eht::settings()};
                               if(not cfg->sensors.enabled)
                                 {
                                 reader.reset();
                                 std::lock_guard const lock{mutex_};
                                 latest_.reset();
                                 }
                               else
                                 {
                                 if(not reader)
                                   {
                                   reader.emplace(sensors::discover("/sys"));
                                   auto const & found{reader->found()};
                                   spdlog::info(
                                     "sensors: graphics card {}, processor {}",
                                     found.gpu_name.empty() ? "not found" : found.gpu_name,
                                     found.cpu_name.empty() ? "not found" : found.cpu_name
                                   );
                                   }
                                 auto reading{reader->read()};
                                 log(reading, reader->found());
                                 std::lock_guard const lock{mutex_};
                                 latest_ = std::move(reading);
                                 }
                               std::unique_lock lock{sleep_mutex};
                               wake.wait_for(
                                 lock,
                                 stoken,
                                 std::chrono::milliseconds{std::max(cfg->sensors.interval_ms, 250u)},
                                 [] { return false; }
                               );
                               }
                             }
                           catch(std::exception const & e)
                             {
                             spdlog::error("sensors: stopped reading: {}", e.what());
                             }
                         }};
  }

auto sensor_watch_t::latest() const -> std::optional<sensors::temperatures_t>
  {
  std::lock_guard const lock{mutex_};
  return latest_;
  }

auto sensor_watch_t::log(sensors::temperatures_t const & reading, sensors::found_t const & found) -> void
  {
  auto const cfg{eht::settings()};
  auto const now{std::chrono::steady_clock::now()};
  if(cfg->evidence.dir.empty() or cfg->sensors.log_interval_s == 0u
     or now - logged_ < std::chrono::seconds{cfg->sensors.log_interval_s})
    return;
  logged_ = now;
  std::error_code ec;
  std::filesystem::path const dir{backup::expand_home(cfg->evidence.dir)};
  std::filesystem::create_directories(dir, ec);
  std::filesystem::path const file{dir / "sensors.jsonl"};
  if(auto const size{std::filesystem::file_size(file, ec)}; not ec and size > uint64_t{cfg->sensors.log_max_mb} << 20u)
    std::filesystem::rename(file, dir / "sensors.jsonl.1", ec);
  std::ofstream out{file, std::ios::app};
  if(out)
    out << sensors::log_line(std::chrono::system_clock::now(), reading, found) << '\n';
  }
