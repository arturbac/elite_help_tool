#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace vision
  {
///\brief what the game's status file says that a whole-screen picture follows - another screen or another state
struct shot_state_t
  {
  uint64_t flags{};
  uint64_t flags2{};
  uint32_t gui_focus{};

  auto operator==(shot_state_t const &) const -> bool = default;
  };

///\brief why a whole-screen picture was asked for - written into its name
enum struct shot_reason_e : uint8_t
  {
  every,
  change
  };

[[nodiscard]]
constexpr auto reason_name(shot_reason_e reason) noexcept -> std::string_view
  { return reason == shot_reason_e::change ? "change" : "every"; }

///\brief decides when the tool asks the layer for a whole-screen picture for eht_vision
///\detail one now and then, and one each time the state changed and settled - a state that keeps changing
/// waits until it stops, so the picture is of the state written beside it
class shot_clock_t
  {
public:
  struct timing_t
    {
    uint64_t every_ms;
    uint64_t after_change_ms;
    uint64_t min_gap_ms;
    };

  ///\brief whether to ask now, and why; a picture asked for is taken as taken
  [[nodiscard]]
  auto tick(shot_state_t const & state, uint64_t now_ms, timing_t const & timing) -> std::optional<shot_reason_e>
    {
    if(not state_ or *state_ != state)
      {
      // the first state seen is no change - nothing was on the screen before it to tell apart
      changed_ = state_.has_value();
      state_ = state;
      changed_at_ = now_ms;
      }
    bool const room{not last_ or now_ms - *last_ >= timing.min_gap_ms};
    std::optional<shot_reason_e> reason;
    if(changed_ and room and now_ms - changed_at_ >= timing.after_change_ms)
      reason = shot_reason_e::change;
    else if(timing.every_ms != 0u and room and (not last_ or now_ms - *last_ >= timing.every_ms))
      reason = shot_reason_e::every;
    if(reason)
      {
      last_ = now_ms;
      changed_ = false;
      }
    return reason;
    }

private:
  std::optional<shot_state_t> state_;
  uint64_t changed_at_{};
  bool changed_{};
  std::optional<uint64_t> last_;
  };
  }  // namespace vision
