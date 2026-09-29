#pragma once

#include <sensors.h>

#include <chrono>
#include <mutex>
#include <optional>
#include <thread>

///\brief the temperatures, read in a thread of its own every couple of seconds - see sensors.h
///\detail the sensors are looked for in that thread too, after the start, so a slow or strange /sys never
/// holds the tool back; the overlay only ever takes the last reading
class sensor_watch_t final
  {
public:
  sensor_watch_t();
  sensor_watch_t(sensor_watch_t const &) = delete;
  auto operator=(sensor_watch_t const &) -> sensor_watch_t & = delete;
  ~sensor_watch_t() = default;

  ///\brief the last reading; nothing while switched off or before the first
  [[nodiscard]]
  auto latest() const -> std::optional<sensors::temperatures_t>;

private:
  mutable std::mutex mutex_;
  std::optional<sensors::temperatures_t> latest_;
  ///\brief the last line of the log - written from the worker alone
  std::chrono::steady_clock::time_point logged_{};
  std::jthread worker_;

  ///\brief a line to sensors.jsonl beside the glare's markers, now and then
  auto log(sensors::temperatures_t const & reading, sensors::found_t const & found) -> void;
  };
