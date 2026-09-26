#pragma once

// the layer does not link the loader - every Vulkan function comes from the chain below us
#define VK_NO_PROTOTYPES
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#include <overlay_ipc.h>

// the Vulkan headers do not carry this macro, and the layer's symbols must break through hidden visibility
#ifndef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#endif

#include <format>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ImGuiContext;

namespace eht_overlay
  {
///\brief the loader keeps the dispatch table in the first word of every dispatchable handle
[[nodiscard]]
inline auto dispatch_key(void * handle) noexcept -> void *
  { return *static_cast<void **>(handle); }

[[nodiscard]]
auto debug_enabled() noexcept -> bool;

auto log_line(std::string_view text) -> void;

template<typename... args_t>
auto log(std::format_string<args_t...> fmt, args_t &&... args) -> void
  {
  if(debug_enabled()) [[unlikely]]
    log_line(std::format(fmt, std::forward<args_t>(args)...));
  }

struct instance_data_t
  {
  VkInstance instance{};
  PFN_vkGetInstanceProcAddr next_gipa{};
  PFN_vkDestroyInstance DestroyInstance{};
  PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties{};
  uint32_t api_version{VK_API_VERSION_1_0};
  };

struct device_data_t
  {
  instance_data_t * instance{};
  VkPhysicalDevice physical_device{};
  VkDevice device{};
  PFN_vkGetDeviceProcAddr next_gdpa{};
  PFN_vkSetDeviceLoaderData set_device_loader_data{};

  PFN_vkDestroyDevice DestroyDevice{};
  PFN_vkGetDeviceQueue GetDeviceQueue{};
  PFN_vkGetDeviceQueue2 GetDeviceQueue2{};
  PFN_vkCreateSwapchainKHR CreateSwapchainKHR{};
  PFN_vkDestroySwapchainKHR DestroySwapchainKHR{};
  PFN_vkGetSwapchainImagesKHR GetSwapchainImagesKHR{};
  PFN_vkQueuePresentKHR QueuePresentKHR{};
  PFN_vkQueueSubmit QueueSubmit{};
  PFN_vkQueueWaitIdle QueueWaitIdle{};
  PFN_vkDeviceWaitIdle DeviceWaitIdle{};
  PFN_vkCreateImageView CreateImageView{};
  PFN_vkDestroyImageView DestroyImageView{};
  PFN_vkCreateFramebuffer CreateFramebuffer{};
  PFN_vkDestroyFramebuffer DestroyFramebuffer{};
  PFN_vkCreateRenderPass CreateRenderPass{};
  PFN_vkDestroyRenderPass DestroyRenderPass{};
  PFN_vkCreateCommandPool CreateCommandPool{};
  PFN_vkDestroyCommandPool DestroyCommandPool{};
  PFN_vkAllocateCommandBuffers AllocateCommandBuffers{};
  PFN_vkBeginCommandBuffer BeginCommandBuffer{};
  PFN_vkEndCommandBuffer EndCommandBuffer{};
  PFN_vkResetCommandBuffer ResetCommandBuffer{};
  PFN_vkCreateFence CreateFence{};
  PFN_vkDestroyFence DestroyFence{};
  PFN_vkWaitForFences WaitForFences{};
  PFN_vkResetFences ResetFences{};
  PFN_vkCreateSemaphore CreateSemaphore{};
  PFN_vkDestroySemaphore DestroySemaphore{};
  PFN_vkCreateDescriptorPool CreateDescriptorPool{};
  PFN_vkDestroyDescriptorPool DestroyDescriptorPool{};
  PFN_vkCmdBeginRenderPass CmdBeginRenderPass{};
  PFN_vkCmdEndRenderPass CmdEndRenderPass{};

  std::vector<VkQueueFamilyProperties> queue_families;

  ///\brief the queue family is only learned from vkGetDeviceQueue, and it is needed at present time
  std::mutex queues_mutex;
  std::unordered_map<void *, uint32_t> queue_family_of;

  [[nodiscard]]
  auto family_of(VkQueue queue) -> uint32_t;
  };

///\brief the resources of one swapchain image
struct frame_resources_t
  {
  VkCommandBuffer command_buffer{};
  VkFence fence{};
  VkSemaphore semaphore{};
  VkImageView view{};
  VkFramebuffer framebuffer{};
  bool submitted{};
  };

struct swapchain_data_t
  {
  device_data_t * device{};
  VkSwapchainKHR swapchain{};
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkExtent2D extent{};
  uint32_t queue_family{VK_QUEUE_FAMILY_IGNORED};

  VkRenderPass render_pass{};
  VkDescriptorPool descriptor_pool{};
  VkCommandPool command_pool{};
  std::vector<VkImage> images;
  std::vector<frame_resources_t> frames;

  ImGuiContext * imgui{};
  bool ready{};
  ///\brief an initialisation that failed once is not worth retrying every frame
  bool broken{};

  uint64_t drawn_frames{};
  double last_draw_seconds{};
  float fps{};
  };

struct registry_t
  {
  std::shared_mutex mutex;
  std::unordered_map<void *, std::unique_ptr<instance_data_t>> instances;
  std::unordered_map<void *, std::unique_ptr<device_data_t>> devices;
  std::unordered_map<VkSwapchainKHR, std::unique_ptr<swapchain_data_t>> swapchains;
  };

[[nodiscard]]
auto registry() -> registry_t &;

///\brief one client per game process, created only once there is really something to draw
[[nodiscard]]
auto ipc_client() -> overlay::client_t &;
  }  // namespace eht_overlay
