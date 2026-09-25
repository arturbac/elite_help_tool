#pragma once

#include "vk_dispatch.h"

namespace eht_overlay
  {
///\brief zasoby powstaja dopiero przy pierwszym present, bo wtedy znamy kolejke i jej rodzine
[[nodiscard]]
auto ensure_resources(swapchain_data_t & data, VkQueue queue) -> bool;

auto destroy_resources(swapchain_data_t & data) -> void;

///\brief rysuje overlay na wskazanym obrazie; zwraca semafor do odczekania przez present
///
/// VK_NULL_HANDLE oznacza ze nic nie narysowano i present ma isc dalej z oryginalnymi semaforami.
/// ta funkcja nie moze rzucic ani zablokowac - jest na sciezce klatki gry
[[nodiscard]]
auto draw_overlay(
  swapchain_data_t & data, VkQueue queue, uint32_t image_index, VkSemaphore const * wait, uint32_t wait_count
) noexcept -> VkSemaphore;
  }  // namespace eht_overlay
