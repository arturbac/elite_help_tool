#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <thread>

///\brief hears EDDN's relay for the bartender's shelf of the commander's own fleet carrier, as other commanders'
/// tools send it on opening the bar there, and keeps each as a reading of the shelf - so Bartender stock and Bar
/// sales show what sold without flying to the carrier
///\detail a thread of its own with a database connection of its own; the relay sends everything EDDN carries and
/// the filtering is here. Nothing is sent to the relay but the greeting and the subscription. Off unless
/// eddn.listen is set; the setting is read live
namespace eddn
  {
class listener_t final
  {
public:
  ///\param account called from the listener's thread for the account whose fleet carrier is heard
  ///\param stored called from the listener's thread after a reading was kept
  ///\param here called from the listener's thread for where the commander is - the markets around are kept
  listener_t(
    std::string db_path,
    std::function<std::string()> account,
    std::function<void()> stored,
    std::function<std::optional<std::array<double, 3>>()> here
  );
  listener_t(listener_t const &) = delete;
  auto operator=(listener_t const &) -> listener_t & = delete;
  ~listener_t() = default;

private:
  std::jthread worker_;
  };
  }  // namespace eddn
