#pragma once

#include <chrono>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

///\brief files the screenshots the layer takes at the key - the whole screen, overlay and all
///
/// The layer cannot write a PNG in the game's process, so it leaves a raw PPM in the spool beside the
/// socket. Here each one is turned into the format asked for and moved to the screenshots directory,
/// in a thread of its own: a PNG of an 8000 px wide screen takes seconds, and the tool must not stop
/// for it. A PPM left behind by a tool that was not running is filed at the next start
class screenshots_t final
  {
public:
  screenshots_t() = default;
  screenshots_t(screenshots_t const &) = delete;
  auto operator=(screenshots_t const &) -> screenshots_t & = delete;
  ///\brief waits for the conversions under way - a half-written picture is worse than a slower exit
  ~screenshots_t();

  ///\brief looks into the spool now and then, and starts filing whatever the layer has finished
  auto collect() -> void;

private:
  struct shared_t
    {
    std::mutex mutex;
    ///\brief the spool files being filed now - not to be started twice
    std::set<std::filesystem::path> busy;
    };

  std::shared_ptr<shared_t> shared_{std::make_shared<shared_t>()};
  std::vector<std::future<void>> work_;
  std::chrono::steady_clock::time_point looked_{};
  };
