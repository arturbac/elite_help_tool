#pragma once

#include "vk_dispatch.h"

#include <optional>

namespace eht_overlay
  {
///\brief the picture the tool asks for now, if it is a new request - taken by one swapchain only
///\detail the first request seen after the layer started counts as already served: it is the tool's last
/// frame handed to a newcomer, and the moment it was meant for is long gone
[[nodiscard]]
auto take_capture_request() -> std::optional<overlay::capture_t>;

///\brief records the copy of the middle of the image into the frame's command buffer, before the overlay
/// is drawn over it
///\returns true when the copy was recorded - the submission must then let the transfer wait for the game
///\detail everything that can fail here fails quietly: the picture is lost and the overlay draws on
[[nodiscard]]
auto record_capture(
  swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index, overlay::capture_t const & request
) noexcept -> bool;

///\brief takes the copied pixels out once the frame's fence has signalled and writes them in a thread of
/// their own - the game's frame never waits for a disk
auto collect_capture(swapchain_data_t & data, frame_resources_t & frame) noexcept -> void;

auto destroy_capture(swapchain_data_t & data) noexcept -> void;
  }  // namespace eht_overlay
