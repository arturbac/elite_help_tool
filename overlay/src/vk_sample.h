#pragma once

#include "vk_dispatch.h"

namespace eht_overlay
  {
///\brief records the small copy of the middle of the image into the frame's command buffer, when one is due
/// and the last one has been read out
///\returns true when it was recorded - the submission must then let the transfer wait for the game too
///\detail the part of the image is halved on the card, blit after blit, each step the mean of 2 x 2 pixels,
/// and the last step copied into a buffer the host reads. Anything failing here fails quietly and for good:
/// the sample is lost, the overlay draws on
[[nodiscard]]
auto record_sample(swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index) noexcept -> bool;

///\brief once the frame's fence has passed, puts the copy into the file shared with the tool
auto collect_sample(swapchain_data_t & data, frame_resources_t & frame) noexcept -> void;

///\brief frees the sample's image, buffer and file; the device is idle by then
auto destroy_sample(swapchain_data_t & data) noexcept -> void;
  }  // namespace eht_overlay
