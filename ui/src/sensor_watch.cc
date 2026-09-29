#include <sensor_watch.h>
#include <eht_settings.h>

#include <spdlog/spdlog.h>

#include <chrono>
#include <condition_variable>

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
