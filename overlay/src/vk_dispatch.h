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

#include <array>
#include <format>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ImGuiContext;
struct ImFont;

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
  PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties{};
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR GetPhysicalDeviceSurfaceCapabilitiesKHR{};
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
  // for the picture of the middle of the screen
  PFN_vkCmdPipelineBarrier CmdPipelineBarrier{};
  PFN_vkCmdCopyImageToBuffer CmdCopyImageToBuffer{};
  PFN_vkCreateBuffer CreateBuffer{};
  PFN_vkDestroyBuffer DestroyBuffer{};
  PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements{};
  PFN_vkAllocateMemory AllocateMemory{};
  PFN_vkFreeMemory FreeMemory{};
  PFN_vkBindBufferMemory BindBufferMemory{};
  PFN_vkMapMemory MapMemory{};
  PFN_vkUnmapMemory UnmapMemory{};
  PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges{};
  // for the ground under the blocks, darkened by how bright the game is beneath it
  PFN_vkCreateShaderModule CreateShaderModule{};
  PFN_vkDestroyShaderModule DestroyShaderModule{};
  PFN_vkCreateGraphicsPipelines CreateGraphicsPipelines{};
  PFN_vkDestroyPipeline DestroyPipeline{};
  PFN_vkCmdBindPipeline CmdBindPipeline{};

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
  ///\brief this frame's submission copies the middle of the image out - read it once the fence says done
  bool capture_pending{};
  uint32_t capture_width{};
  uint32_t capture_height{};
  std::string capture_path;
  ///\brief the copy is of the whole screen, and its large buffer is freed once read out
  bool capture_release{};
  };

///\brief the buffer the middle of the screen is copied into, made at the first picture and kept
struct capture_buffer_t
  {
  VkBuffer buffer{};
  VkDeviceMemory memory{};
  VkDeviceSize size{};
  void * mapped{};
  bool coherent{};
  };

struct swapchain_data_t
  {
  device_data_t * device{};
  VkSwapchainKHR swapchain{};
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkExtent2D extent{};
  uint32_t queue_family{VK_QUEUE_FAMILY_IGNORED};
  ///\brief the images can be copied from - the driver took the transfer usage we asked for
  bool capturable{};
  ///\brief a picture that failed once is not tried again; the overlay goes on regardless
  bool capture_broken{};
  capture_buffer_t capture;
  ///\brief a picture asked for and waiting for its moment, with the frame shown to the player meanwhile
  std::optional<overlay::capture_t> armed_capture;
  double capture_due{};
  ///\brief when the last picture was taken and how large, for the flash and the word that it was
  double shutter_at{-1.0};
  float shutter_size{};
  ///\brief a screenshot asked for by the key and not yet copied - kept until a frame can take it
  bool screenshot_wanted{};
  ///\brief when the last screenshot was copied, for the word that it was
  double screenshot_at{-1.0};
  ///\brief the pictures the atlas holds, in the order their rectangles were reserved - -1 for a file
  /// that could not be read
  std::vector<std::string> picture_paths;
  std::vector<int> picture_rects;

  VkRenderPass render_pass{};
  VkDescriptorPool descriptor_pool{};
  ///\brief the ground pipeline, made at the first draw from ImGui's own pipeline layout; broken when that failed
  VkPipeline ground_pipeline{};
  bool ground_broken{};
  ///\brief the same for the lit balls of the diagrams; broken leaves them flat discs
  VkPipeline sphere_pipeline{};
  bool sphere_broken{};
  VkCommandPool command_pool{};
  std::vector<VkImage> images;
  std::vector<frame_resources_t> frames;

  ImGuiContext * imgui{};
  ///\brief where each emblem sits in the font atlas, indexed by emblem_e; -1 means it never got there
  std::array<int, 4> emblem_rects{-1, -1, -1, -1};
  ///\brief the same font rasterised smaller, for blocks that are lists rather than glances
  ImFont * small_font{};
  ///\brief the text scale and small-text ratio the atlas was rasterised at - a layout asking for others
  /// has the fonts built again
  float font_scale{};
  float font_small{};
  bool ready{};
  ///\brief an initialisation that failed once is not worth retrying every frame
  bool broken{};

  uint64_t drawn_frames{};
  double last_draw_seconds{};

  ///\brief what the debug log reports every few seconds - the frame rate the game reaches, and how much
  /// of each frame is ours: the drawing on the CPU, and the wait for our previous submission of the image
  struct report_t
    {
    double started{};
    uint64_t frames{};
    double worst_gap{};
    double drawing{};
    double waiting{};
    };
  report_t report;
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
