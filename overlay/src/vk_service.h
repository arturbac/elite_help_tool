#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace eht_overlay
  {
///\brief one object of the overlay's that runs a thread of its own, made at its first use and stopped on
/// demand
///\detail stop() destroys it, and its destructor joins the thread: nothing of it is left running or
/// allocated, which the plugin must guarantee before it is unloaded. Nothing calls get() once stop() has
/// begun - the host detaches every device first - so a reference given out never outlives the object
template<typename object_t>
class service_t
  {
public:
  template<typename make_t>
  [[nodiscard]]
  auto get(make_t && make) -> object_t &
    {
    std::scoped_lock const lock{mutex_};
    if(not object_)
      object_ = std::forward<make_t>(make)();
    return *object_;
    }

  ///\brief the object if it was made, without making it
  [[nodiscard]]
  auto peek() -> object_t *
    {
    std::scoped_lock const lock{mutex_};
    return object_.get();
    }

  auto stop() noexcept -> void
    {
    std::unique_ptr<object_t> gone;
      {
      std::scoped_lock const lock{mutex_};
      gone = std::move(object_);
      }
    // joined outside the lock - the thread itself may still be asking for another service
    gone.reset();
    }

private:
  std::mutex mutex_;
  std::unique_ptr<object_t> object_;
  };
  }  // namespace eht_overlay
