// the overlay as a plugin of the layer: everything it draws, pictures and reads, behind plugin_api.h
#include "plugin_api.h"
#include "vk_capture.h"
#include "vk_dispatch.h"
#include "vk_draw.h"
#include "vk_faces.h"
#include "vk_keyboard.h"
#include "vk_service.h"

#include <memory>

namespace eht_overlay
  {
namespace
  {
  ///\brief what the plugin keeps of one device: its functions, and those of its instance it uses
  struct plugin_device_t
    {
    instance_data_t instance;
    device_data_t device;
    };

  service_t<overlay::client_t> clients;

  ///\brief how long a detach waits for the overlay's own work on the card, at most - a hung device
  /// must not hang the game with it
  constexpr uint64_t detach_wait_ns{1'000'000'000u};

  auto attach_device(eht_plugin_device_t const * host) -> void *
    {
    try
      {
      auto plugin{std::make_unique<plugin_device_t>()};
      instance_data_t & instance{plugin->instance};
      instance.instance = host->instance;
      instance.next_gipa = host->next_gipa;
      instance.api_version = host->api_version;
      instance.GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        host->next_gipa(host->instance, "vkGetPhysicalDeviceQueueFamilyProperties")
      );
      instance.GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        host->next_gipa(host->instance, "vkGetPhysicalDeviceMemoryProperties")
      );
      instance.GetPhysicalDeviceFormatProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
        host->next_gipa(host->instance, "vkGetPhysicalDeviceFormatProperties")
      );

      device_data_t & data{plugin->device};
      data.instance = &instance;
      data.physical_device = host->physical_device;
      data.device = host->device;
      data.next_gdpa = host->next_gdpa;
      data.set_device_loader_data = host->set_device_loader_data;
      data.queue_family = host->queue_family;
      data.host = host->host;

      VkDevice const device{host->device};
#define EHT_LOAD(name) data.name = reinterpret_cast<PFN_vk##name>(host->next_gdpa(device, "vk" #name))
      EHT_LOAD(QueueSubmit);
      EHT_LOAD(QueueWaitIdle);
      EHT_LOAD(DeviceWaitIdle);
      EHT_LOAD(CreateImageView);
      EHT_LOAD(DestroyImageView);
      EHT_LOAD(CreateFramebuffer);
      EHT_LOAD(DestroyFramebuffer);
      EHT_LOAD(CreateRenderPass);
      EHT_LOAD(DestroyRenderPass);
      EHT_LOAD(CreateCommandPool);
      EHT_LOAD(DestroyCommandPool);
      EHT_LOAD(AllocateCommandBuffers);
      EHT_LOAD(BeginCommandBuffer);
      EHT_LOAD(EndCommandBuffer);
      EHT_LOAD(ResetCommandBuffer);
      EHT_LOAD(CreateFence);
      EHT_LOAD(DestroyFence);
      EHT_LOAD(WaitForFences);
      EHT_LOAD(ResetFences);
      EHT_LOAD(CreateSemaphore);
      EHT_LOAD(DestroySemaphore);
      EHT_LOAD(CreateDescriptorPool);
      EHT_LOAD(DestroyDescriptorPool);
      EHT_LOAD(CmdBeginRenderPass);
      EHT_LOAD(CmdEndRenderPass);
      EHT_LOAD(CmdPipelineBarrier);
      EHT_LOAD(CmdCopyImageToBuffer);
      EHT_LOAD(CreateBuffer);
      EHT_LOAD(DestroyBuffer);
      EHT_LOAD(GetBufferMemoryRequirements);
      EHT_LOAD(AllocateMemory);
      EHT_LOAD(FreeMemory);
      EHT_LOAD(BindBufferMemory);
      EHT_LOAD(MapMemory);
      EHT_LOAD(UnmapMemory);
      EHT_LOAD(InvalidateMappedMemoryRanges);
      EHT_LOAD(CreateShaderModule);
      EHT_LOAD(DestroyShaderModule);
      EHT_LOAD(CreateGraphicsPipelines);
      EHT_LOAD(DestroyPipeline);
      EHT_LOAD(CmdBindPipeline);
      EHT_LOAD(CreateImage);
      EHT_LOAD(DestroyImage);
      EHT_LOAD(GetImageMemoryRequirements);
      EHT_LOAD(BindImageMemory);
      EHT_LOAD(CreateSampler);
      EHT_LOAD(DestroySampler);
      EHT_LOAD(CmdCopyBufferToImage);
      EHT_LOAD(CmdBlitImage);
#undef EHT_LOAD

      if(instance.GetPhysicalDeviceQueueFamilyProperties != nullptr)
        {
        uint32_t count{};
        instance.GetPhysicalDeviceQueueFamilyProperties(host->physical_device, &count, nullptr);
        data.queue_families.resize(count);
        instance.GetPhysicalDeviceQueueFamilyProperties(host->physical_device, &count, data.queue_families.data());
        }
      log("device attached, {} queue families", data.queue_families.size());
      return plugin.release();
      }
    catch(...)
      {
      report("overlay: device could not be attached ({}), no overlay on it", exception_text());
      return nullptr;
      }
    }

  auto detach_device(void * device) -> void
    { delete static_cast<plugin_device_t *>(device); }

  auto attach_swapchain(void * device, eht_plugin_swapchain_t const * host) -> void *
    {
    if(device == nullptr)
      return nullptr;
    try
      {
      auto entry{std::make_unique<swapchain_data_t>()};
      entry->device = &static_cast<plugin_device_t *>(device)->device;
      entry->swapchain = host->swapchain;
      entry->format = host->format;
      entry->extent = host->extent;
      entry->capturable = host->capturable != 0u;
      entry->images.assign(host->images, host->images + host->image_count);
      if(entry->images.empty())
        entry->broken = true;
      // the ipc client is made only now - a process without a swapchain pays for nothing
      (void)ipc_client();
      log("swapchain {}x{} attached, {} images", entry->extent.width, entry->extent.height, entry->images.size());
      return entry.release();
      }
    catch(...)
      {
      report("overlay: swapchain could not be attached ({}), no overlay on it", exception_text());
      return nullptr;
      }
    }

  auto detach_swapchain(void * swapchain) -> void
    {
    std::unique_ptr<swapchain_data_t> entry{static_cast<swapchain_data_t *>(swapchain)};
    if(not entry)
      return;
    // the layer waited for the queue presenting it; the overlay's own submissions may have gone elsewhere
    device_data_t & device{*entry->device};
    for(frame_resources_t const & frame: entry->frames)
      if(frame.submitted and frame.fence != VK_NULL_HANDLE and device.WaitForFences != nullptr)
        (void)device.WaitForFences(device.device, 1u, &frame.fence, VK_TRUE, detach_wait_ns);
    destroy_resources(*entry);
    }

  auto present(void * swapchain, VkQueue queue, uint32_t image_index, VkSemaphore const * wait, uint32_t wait_count)
    -> VkSemaphore
    {
    auto * const entry{static_cast<swapchain_data_t *>(swapchain)};
    if(entry == nullptr or entry->broken)
      return VK_NULL_HANDLE;
    return draw_overlay(*entry, queue, image_index, wait, wait_count);
    }

  auto present_failed(void * swapchain, uint32_t image_index) -> void
    {
    if(swapchain != nullptr)
      renew_present_semaphore(*static_cast<swapchain_data_t *>(swapchain), image_index);
    }

  auto shutdown() -> void
    {
    // the keyboard's thread asks the client for the key, so it goes first
    stop_keyboard();
    stop_face_loader();
    stop_capture_writer();
    clients.stop();
    log("plugin shut down");
    }
  }  // namespace

auto ipc_client() -> overlay::client_t &
  { return clients.get([] { return std::make_unique<overlay::client_t>(overlay::default_socket_path()); }); }
  }  // namespace eht_overlay

extern "C" __attribute__((visibility("default"))) auto eht_overlay_plugin(eht_plugin_t * plugin) -> int
  {
  if(plugin == nullptr or plugin->abi != EHT_OVERLAY_PLUGIN_ABI or plugin->size != sizeof(eht_plugin_t))
    return 0;
  plugin->attach_device = &eht_overlay::attach_device;
  plugin->detach_device = &eht_overlay::detach_device;
  plugin->attach_swapchain = &eht_overlay::attach_swapchain;
  plugin->detach_swapchain = &eht_overlay::detach_swapchain;
  plugin->present = &eht_overlay::present;
  plugin->present_failed = &eht_overlay::present_failed;
  plugin->shutdown = &eht_overlay::shutdown;
  return 1;
  }
