#include "vk_world.h"

#include "vk_log.h"

#include <world_follow.h>

#include <chrono>
#include <cstdlib>
#include <mutex>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace eht_overlay
  {
namespace
  {
  std::mutex world_mutex;
  edworld::share_t const * mapped{};
  std::chrono::steady_clock::time_point next_try{};
  bool told_missing{};

  auto share_path() noexcept -> char const *
    {
    char const * const env{std::getenv("EHT_WORLD_SHARE")};
    return env and *env ? env : "/dev/shm/eht/panels";
    }

  ///\brief maps the file once it is there and whole; said once either way
  auto try_map() noexcept -> void
    {
    char const * const path{share_path()};
    int const fd{::open(path, O_RDONLY | O_CLOEXEC)};
    if(fd < 0)
      {
      if(not told_missing)
        {
        told_missing = true;
        log("overlay: no {} - panels are not followed until edworld publishes", path);
        }
      return;
      }
    struct stat st{};
    if(::fstat(fd, &st) != 0 or static_cast<size_t>(st.st_size) < sizeof(edworld::share_t))
      {
      ::close(fd);
      return;
      }
    void * const view{::mmap(nullptr, sizeof(edworld::share_t), PROT_READ, MAP_SHARED, fd, 0)};
    ::close(fd);
    if(view == MAP_FAILED)
      {
      report("overlay: {} could not be mapped, panels are not followed", path);
      return;
      }
    mapped = static_cast<edworld::share_t const *>(view);
    report("overlay: following panels from {}", path);
    }
  }  // namespace

auto world_record(edworld::share_t & copy) noexcept -> bool
  {
  std::lock_guard const lock{world_mutex};
  if(not mapped)
    {
    auto const now{std::chrono::steady_clock::now()};
    if(now < next_try)
      return false;
    next_try = now + std::chrono::seconds{2};
    try_map();
    if(not mapped)
      return false;
    }
  return overlay::world::read_record(*mapped, copy);
  }
  }  // namespace eht_overlay
