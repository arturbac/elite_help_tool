#pragma once

// the one thing the layer and the overlay plugin share: plain C, so the two need not be built together
//
// The layer the Vulkan loader knows (libeht_overlay.so) only tracks the game's instances, devices, queues
// and swapchains and patches the swapchain's usage. Everything drawn, pictured or read lives in the plugin
// (libeht_overlay_plugin.so), which the layer loads beside itself and loads again whenever the file is
// replaced - a new overlay without restarting the game. Attaching, detaching and the shutdown never run
// beside any other call; presents of different swapchains may come from different threads at once
#define VK_NO_PROTOTYPES
#include <vulkan/vk_layer.h>
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C"
  {
#endif

// raised whenever a struct below or a function's meaning changes - a plugin of another number is refused
#define EHT_OVERLAY_PLUGIN_ABI 1u

// the name the layer looks the plugin up by
#define EHT_OVERLAY_PLUGIN_ENTRY "eht_overlay_plugin"

// what the layer knows of one device of the game
struct eht_plugin_device_t
  {
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  // the next layer down the chain - the plugin must not call the loader, it would come back in at the top
  PFN_vkGetInstanceProcAddr next_gipa;
  PFN_vkGetDeviceProcAddr next_gdpa;
  // every dispatchable handle the plugin makes, a command buffer, needs the loader's table put into it
  PFN_vkSetDeviceLoaderData set_device_loader_data;
  uint32_t api_version;
  // the family a queue of this device was taken from, VK_QUEUE_FAMILY_IGNORED when the layer never saw it
  uint32_t (*queue_family)(void * host, VkQueue queue);
  void * host;
  };

// what the layer knows of one swapchain of the game
struct eht_plugin_swapchain_t
  {
  VkSwapchainKHR swapchain;
  VkFormat format;
  VkExtent2D extent;
  uint32_t image_count;
  VkImage const * images;
  // the images can be copied from - the driver took the transfer usage asked for
  uint32_t capturable;
  };

// the plugin's side, filled in by its entry
struct eht_plugin_t
  {
  uint32_t abi;
  uint32_t size;
  // what the plugin keeps for a device or a swapchain, or null when it wants nothing of it
  void * (*attach_device)(struct eht_plugin_device_t const * device);
  // the device is idle, and every swapchain of it has been detached already
  void (*detach_device)(void * device);
  void * (*attach_swapchain)(void * device, struct eht_plugin_swapchain_t const * swapchain);
  // the device is idle; everything of the swapchain is freed
  void (*detach_swapchain)(void * swapchain);
  // draws onto the image before it is shown; the semaphore for the present to wait on, or VK_NULL_HANDLE
  // for the present to go on with the game's own
  VkSemaphore (*present)(
    void * swapchain, VkQueue queue, uint32_t image_index, VkSemaphore const * wait, uint32_t wait_count
  );
  // the present failed in a way that may leave the semaphore signalled
  void (*present_failed)(void * swapchain, uint32_t image_index);
  // the last call: every device and swapchain has been detached, and the plugin stops and joins its threads
  // and frees what it holds - it is unloaded right after
  void (*shutdown)(void);
  };

// fills in the plugin's functions; zero when it cannot run, which leaves the game without an overlay
typedef int (*eht_overlay_plugin_entry_t)(struct eht_plugin_t * plugin);

#ifdef __cplusplus
  }
#endif
