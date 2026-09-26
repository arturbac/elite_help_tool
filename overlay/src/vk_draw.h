#pragma once

#include "vk_dispatch.h"

namespace eht_overlay
  {
///\brief resources are created at the first present, because only then do we know the queue and its family
[[nodiscard]]
auto ensure_resources(swapchain_data_t & data, VkQueue queue) -> bool;

auto destroy_resources(swapchain_data_t & data) -> void;

///\brief odtwarza semafor prezentacji po nieudanym present
///
/// when vkQueuePresentKHR returns an error, the specification does not guarantee that the semaphore wait
/// was queued at all. such a semaphore may stay signalled for ever, and signalling it again with the
/// next submission is an error that ends in a hung queue
auto renew_present_semaphore(swapchain_data_t & data, uint32_t image_index) noexcept -> void;

///\brief rysuje overlay na wskazanym obrazie; zwraca semafor do odczekania przez present
///
/// VK_NULL_HANDLE means nothing was drawn and the present should go on with the original semaphores.
/// this function may neither throw nor block - it sits on the game's frame path
[[nodiscard]]
auto draw_overlay(
  swapchain_data_t & data, VkQueue queue, uint32_t image_index, VkSemaphore const * wait, uint32_t wait_count
) noexcept -> VkSemaphore;
  }  // namespace eht_overlay
