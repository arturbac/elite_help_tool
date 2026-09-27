#include "vk_dispatch.h"
#include "vk_draw.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace eht_overlay
  {
namespace
  {
  constexpr char layer_name[]{"VK_LAYER_EHT_overlay"};

  ///\brief the game's launcher draws through Vulkan too, and would get an overlay of its own
  ///\detail where it closes once the game starts that is only a flicker, but a launcher kept open beside
  /// the game - as the one without Steam is - draws the whole overlay into its window for as long as the
  /// game runs, a second client of the tool and a second renderer on the same card. Under Wine the
  /// process is the Windows executable's, so its name is read from the command line
  [[nodiscard]]
  auto process_skipped() noexcept -> bool
    {
    static bool const skipped{[]
                              {
                                std::FILE * const file{std::fopen("/proc/self/cmdline", "rb")};
                                if(file == nullptr)
                                  return false;
                                std::string line;
                                std::array<char, 4096> chunk{};
                                for(size_t read; (read = std::fread(chunk.data(), 1u, chunk.size(), file)) != 0u;)
                                  line.append(chunk.data(), read);
                                std::fclose(file);
                                for(char & c: line)
                                  c = c == '\0' ? ' ' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                                bool const launcher{line.contains("edlaunch.exe")};
                                if(launcher)
                                  log("the game's launcher - no overlay here");
                                return launcher;
                              }()};
    return skipped;
    }

  ///\brief pictures of the screen need the swapchain to be copied from, which can cost the game its frame rate
  [[nodiscard]]
  auto capture_allowed() noexcept -> bool
    {
    static bool const allowed{[]
                              {
                                char const * const value{std::getenv("EHT_OVERLAY_CAPTURE")};
                                return value != nullptr and *value != '\0' and *value != '0';
                              }()};
    return allowed;
    }
  ///\brief presenting several swapchains at once is another matter entirely - we simply do not draw them
  constexpr uint32_t max_swapchains_per_present{8u};

  [[nodiscard]]
  auto instance_chain(VkInstanceCreateInfo const * info, VkLayerFunction function) -> VkLayerInstanceCreateInfo *
    {
    for(auto const * item{static_cast<VkBaseInStructure const *>(info->pNext)}; item != nullptr; item = item->pNext)
      if(item->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO)
        {
        auto * const chain{reinterpret_cast<VkLayerInstanceCreateInfo *>(const_cast<VkBaseInStructure *>(item))};
        if(chain->function == function)
          return chain;
        }
    return nullptr;
    }

  [[nodiscard]]
  auto device_chain(VkDeviceCreateInfo const * info, VkLayerFunction function) -> VkLayerDeviceCreateInfo *
    {
    for(auto const * item{static_cast<VkBaseInStructure const *>(info->pNext)}; item != nullptr; item = item->pNext)
      if(item->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO)
        {
        auto * const chain{reinterpret_cast<VkLayerDeviceCreateInfo *>(const_cast<VkBaseInStructure *>(item))};
        if(chain->function == function)
          return chain;
        }
    return nullptr;
    }

  [[nodiscard]]
  auto find_instance(void * key) -> instance_data_t *
    {
    std::shared_lock const lock{registry().mutex};
    auto const it{registry().instances.find(key)};
    return it != registry().instances.end() ? it->second.get() : nullptr;
    }

  [[nodiscard]]
  auto find_device(void * key) -> device_data_t *
    {
    std::shared_lock const lock{registry().mutex};
    auto const it{registry().devices.find(key)};
    return it != registry().devices.end() ? it->second.get() : nullptr;
    }

  [[nodiscard]]
  auto find_swapchain(VkSwapchainKHR handle) -> swapchain_data_t *
    {
    std::shared_lock const lock{registry().mutex};
    auto const it{registry().swapchains.find(handle)};
    return it != registry().swapchains.end() ? it->second.get() : nullptr;
    }
  }  // namespace

auto debug_enabled() noexcept -> bool
  {
  static bool const enabled{[]
                            {
                              char const * const value{std::getenv("EHT_OVERLAY_DEBUG")};
                              return value != nullptr and *value != '\0' and *value != '0';
                            }()};
  return enabled;
  }

auto log_line(std::string_view text) -> void
  {
  std::fprintf(stderr, "[eht-overlay] %.*s\n", static_cast<int>(text.size()), text.data());
  std::fflush(stderr);
  }

auto registry() -> registry_t &
  {
  // deliberately never destroyed - tidying globals while the game process winds down is asking for trouble
  static registry_t * const instance{new registry_t{}};
  return *instance;
  }

auto ipc_client() -> overlay::client_t &
  {
  // as above, and we also do not want to join the io thread once the game is already shutting down
  static overlay::client_t * const client{new overlay::client_t{overlay::default_socket_path()}};
  return *client;
  }

auto device_data_t::family_of(VkQueue queue) -> uint32_t
  {
  std::lock_guard const lock{queues_mutex};
  auto const it{queue_family_of.find(static_cast<void *>(queue))};
  return it != queue_family_of.end() ? it->second : VK_QUEUE_FAMILY_IGNORED;
  }

namespace
  {
  VKAPI_ATTR auto VKAPI_CALL overlay_CreateInstance(
    VkInstanceCreateInfo const * create_info, VkAllocationCallbacks const * allocator, VkInstance * instance
  ) -> VkResult
    {
    VkLayerInstanceCreateInfo * const chain{instance_chain(create_info, VK_LAYER_LINK_INFO)};
    if(chain == nullptr or chain->u.pLayerInfo == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    auto const next_gipa{chain->u.pLayerInfo->pfnNextGetInstanceProcAddr};
    chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;

    auto const create{reinterpret_cast<PFN_vkCreateInstance>(next_gipa(nullptr, "vkCreateInstance"))};
    if(create == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    VkResult const result{create(create_info, allocator, instance)};
    if(result != VK_SUCCESS)
      return result;

    auto data{std::make_unique<instance_data_t>()};
    data->instance = *instance;
    data->next_gipa = next_gipa;
    data->DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(next_gipa(*instance, "vkDestroyInstance"));
    data->GetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
      next_gipa(*instance, "vkGetPhysicalDeviceQueueFamilyProperties")
    );
    data->GetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
      next_gipa(*instance, "vkGetPhysicalDeviceMemoryProperties")
    );
    data->GetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
      next_gipa(*instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR")
    );
    data->api_version = create_info->pApplicationInfo != nullptr and create_info->pApplicationInfo->apiVersion != 0u
                          ? create_info->pApplicationInfo->apiVersion
                          : VK_API_VERSION_1_0;

    log(
      "instance created, api {}.{}", VK_API_VERSION_MAJOR(data->api_version), VK_API_VERSION_MINOR(data->api_version)
    );

      {
      std::unique_lock const lock{registry().mutex};
      registry().instances[dispatch_key(*instance)] = std::move(data);
      }
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_DestroyInstance(VkInstance instance, VkAllocationCallbacks const * allocator)
    -> void
    {
    if(instance == VK_NULL_HANDLE)
      return;

    void * const key{dispatch_key(instance)};
    PFN_vkDestroyInstance destroy{};
      {
      std::unique_lock const lock{registry().mutex};
      if(auto const it{registry().instances.find(key)}; it != registry().instances.end())
        {
        destroy = it->second->DestroyInstance;
        registry().instances.erase(it);
        }
      }

    if(destroy != nullptr)
      destroy(instance, allocator);
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_CreateDevice(
    VkPhysicalDevice physical_device,
    VkDeviceCreateInfo const * create_info,
    VkAllocationCallbacks const * allocator,
    VkDevice * device
  ) -> VkResult
    {
    instance_data_t * const instance{find_instance(dispatch_key(physical_device))};
    VkLayerDeviceCreateInfo * const chain{device_chain(create_info, VK_LAYER_LINK_INFO)};
    if(instance == nullptr or chain == nullptr or chain->u.pLayerInfo == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    auto const next_gipa{chain->u.pLayerInfo->pfnNextGetInstanceProcAddr};
    auto const next_gdpa{chain->u.pLayerInfo->pfnNextGetDeviceProcAddr};
    chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;

    auto const create{reinterpret_cast<PFN_vkCreateDevice>(next_gipa(instance->instance, "vkCreateDevice"))};
    if(create == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    VkResult const result{create(physical_device, create_info, allocator, device)};
    if(result != VK_SUCCESS)
      return result;

    auto data{std::make_unique<device_data_t>()};
    data->instance = instance;
    data->physical_device = physical_device;
    data->device = *device;
    data->next_gdpa = next_gdpa;

    if(
      VkLayerDeviceCreateInfo const * const callback{device_chain(create_info, VK_LOADER_DATA_CALLBACK)};
      callback != nullptr
    )
      data->set_device_loader_data = callback->u.pfnSetDeviceLoaderData;

#define EHT_LOAD(name) data->name = reinterpret_cast<PFN_vk##name>(next_gdpa(*device, "vk" #name))
    EHT_LOAD(DestroyDevice);
    EHT_LOAD(GetDeviceQueue);
    EHT_LOAD(GetDeviceQueue2);
    EHT_LOAD(CreateSwapchainKHR);
    EHT_LOAD(DestroySwapchainKHR);
    EHT_LOAD(GetSwapchainImagesKHR);
    EHT_LOAD(QueuePresentKHR);
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
#undef EHT_LOAD

    if(instance->GetPhysicalDeviceQueueFamilyProperties != nullptr)
      {
      uint32_t count{};
      instance->GetPhysicalDeviceQueueFamilyProperties(physical_device, &count, nullptr);
      data->queue_families.resize(count);
      instance->GetPhysicalDeviceQueueFamilyProperties(physical_device, &count, data->queue_families.data());
      }

    log("device created, {} queue families", data->queue_families.size());

      {
      std::unique_lock const lock{registry().mutex};
      registry().devices[dispatch_key(*device)] = std::move(data);
      }
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_DestroyDevice(VkDevice device, VkAllocationCallbacks const * allocator) -> void
    {
    if(device == VK_NULL_HANDLE)
      return;

    void * const key{dispatch_key(device)};
    PFN_vkDestroyDevice destroy{};
    std::vector<std::unique_ptr<swapchain_data_t>> orphans;
    device_data_t * data{};

      {
      std::unique_lock const lock{registry().mutex};
      if(auto const it{registry().devices.find(key)}; it != registry().devices.end())
        {
        data = it->second.get();
        destroy = data->DestroyDevice;
        }

      for(auto it{registry().swapchains.begin()}; it != registry().swapchains.end();)
        if(it->second->device == data)
          {
          orphans.push_back(std::move(it->second));
          it = registry().swapchains.erase(it);
          }
        else
          ++it;
      }

    // the overlay's resources must go before the device does, otherwise the driver reports a leak
    if(data != nullptr and data->DeviceWaitIdle != nullptr and not orphans.empty())
      data->DeviceWaitIdle(device);
    for(auto & orphan: orphans)
      destroy_resources(*orphan);
    orphans.clear();

      {
      std::unique_lock const lock{registry().mutex};
      registry().devices.erase(key);
      }

    if(destroy != nullptr)
      destroy(device, allocator);
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_GetDeviceQueue(VkDevice device, uint32_t family, uint32_t index, VkQueue * queue)
    -> void
    {
    device_data_t * const data{find_device(dispatch_key(device))};
    if(data == nullptr or data->GetDeviceQueue == nullptr)
      return;

    data->GetDeviceQueue(device, family, index, queue);
    if(queue != nullptr and *queue != VK_NULL_HANDLE)
      {
      std::lock_guard const lock{data->queues_mutex};
      data->queue_family_of[static_cast<void *>(*queue)] = family;
      }
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_GetDeviceQueue2(VkDevice device, VkDeviceQueueInfo2 const * info, VkQueue * queue)
    -> void
    {
    device_data_t * const data{find_device(dispatch_key(device))};
    if(data == nullptr or data->GetDeviceQueue2 == nullptr)
      return;

    data->GetDeviceQueue2(device, info, queue);
    if(queue != nullptr and *queue != VK_NULL_HANDLE and info != nullptr)
      {
      std::lock_guard const lock{data->queues_mutex};
      data->queue_family_of[static_cast<void *>(*queue)] = info->queueFamilyIndex;
      }
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_CreateSwapchainKHR(
    VkDevice device,
    VkSwapchainCreateInfoKHR const * create_info,
    VkAllocationCallbacks const * allocator,
    VkSwapchainKHR * swapchain
  ) -> VkResult
    {
    device_data_t * const data{find_device(dispatch_key(device))};
    if(data == nullptr or data->CreateSwapchainKHR == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    // untouched and unregistered, so every present of it goes straight through as well
    if(process_skipped())
      return data->CreateSwapchainKHR(device, create_info, allocator, swapchain);

    // we draw into the swapchain images, so they must be usable as an attachment - and copied from, for
    // the picture of the middle of the screen
    VkSwapchainCreateInfoKHR patched{*create_info};
    patched.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // the transfer usage is not promised by the specification, so it is asked for only where the surface
    // says it has it - an invalid usage need not fail, it may simply misbehave. And only when asked for: the
    // first layer to add it met the game at 12-16 frames instead of 60 on the 9000x2160 Wine desktop, and
    // a usage like this can leave the compositor without its fast way of showing the images. The swapchain
    // is made before the tool connects, so the settings file cannot decide it - hence the one variable
    if(
      VkSurfaceCapabilitiesKHR capabilities{};
      capture_allowed()
      and
      data->instance != nullptr and data->instance->GetPhysicalDeviceSurfaceCapabilitiesKHR != nullptr
      and data->instance->GetPhysicalDeviceSurfaceCapabilitiesKHR(
            data->physical_device, create_info->surface, &capabilities
          ) == VK_SUCCESS
      and (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u
    )
      patched.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkResult result{data->CreateSwapchainKHR(device, &patched, allocator, swapchain)};
    if(
      result != VK_SUCCESS and (patched.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u
      and (create_info->imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u
    )
      {
      // the pictures are the least of it - without the copy we still draw
      log("swapchain rejected the transfer usage, trying without pictures");
      patched.imageUsage = create_info->imageUsage | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      result = data->CreateSwapchainKHR(device, &patched, allocator, swapchain);
      }
    if(result != VK_SUCCESS and patched.imageUsage != create_info->imageUsage)
      {
      // the game matters more than the overlay - if the driver refuses the added usage, we fall back to the original
      log("swapchain rejected extra usage, falling back without overlay");
      return data->CreateSwapchainKHR(device, create_info, allocator, swapchain);
      }
    if(result != VK_SUCCESS)
      return result;

    auto entry{std::make_unique<swapchain_data_t>()};
    entry->device = data;
    entry->swapchain = *swapchain;
    entry->format = create_info->imageFormat;
    entry->extent = create_info->imageExtent;
    entry->capturable = (patched.imageUsage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0u;

    if(data->GetSwapchainImagesKHR != nullptr)
      {
      uint32_t count{};
      data->GetSwapchainImagesKHR(device, *swapchain, &count, nullptr);
      entry->images.resize(count);
      if(count != 0u)
        data->GetSwapchainImagesKHR(device, *swapchain, &count, entry->images.data());
      }

    if(entry->images.empty())
      entry->broken = true;

    log(
      "swapchain {}x{} with {} images, usage {:#x} of the game's {:#x}",
      entry->extent.width,
      entry->extent.height,
      entry->images.size(),
      patched.imageUsage,
      create_info->imageUsage
    );

    // the ipc client is created only now - a process without a swapchain, a game launcher say, pays for nothing
    (void)ipc_client();

      {
      std::unique_lock const lock{registry().mutex};
      registry().swapchains[*swapchain] = std::move(entry);
      }
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL
    overlay_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, VkAllocationCallbacks const * allocator)
      -> void
    {
    device_data_t * const data{find_device(dispatch_key(device))};
    if(data == nullptr or data->DestroySwapchainKHR == nullptr)
      return;

    std::unique_ptr<swapchain_data_t> entry;
      {
      std::unique_lock const lock{registry().mutex};
      if(auto const it{registry().swapchains.find(swapchain)}; it != registry().swapchains.end())
        {
        entry = std::move(it->second);
        registry().swapchains.erase(it);
        }
      }

    if(entry and entry->ready)
      {
      if(data->DeviceWaitIdle != nullptr)
        data->DeviceWaitIdle(device);
      destroy_resources(*entry);
      }
    entry.reset();

    data->DestroySwapchainKHR(device, swapchain, allocator);
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_QueuePresentKHR(VkQueue queue, VkPresentInfoKHR const * present_info) -> VkResult
    {
    device_data_t * const data{find_device(dispatch_key(queue))};
    if(data == nullptr or data->QueuePresentKHR == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    if(present_info == nullptr or present_info->swapchainCount == 0u)
      return data->QueuePresentKHR(queue, present_info);

    std::array<VkSemaphore, max_swapchains_per_present> signalled{};
    // after a failed present we need to know whose semaphore to clean up
    std::array<swapchain_data_t *, max_swapchains_per_present> owners{};
    std::array<uint32_t, max_swapchains_per_present> owner_images{};
    uint32_t signalled_count{};

    VkSemaphore const * wait{present_info->pWaitSemaphores};
    uint32_t wait_count{present_info->waitSemaphoreCount};

    uint32_t const count{std::min(present_info->swapchainCount, max_swapchains_per_present)};
    for(uint32_t index{}; index != count; ++index)
      {
      swapchain_data_t * const entry{find_swapchain(present_info->pSwapchains[index])};
      if(entry == nullptr or entry->broken)
        continue;

      VkSemaphore const semaphore{draw_overlay(*entry, queue, present_info->pImageIndices[index], wait, wait_count)};
      if(semaphore == VK_NULL_HANDLE)
        continue;

      signalled[signalled_count] = semaphore;
      owners[signalled_count] = entry;
      owner_images[signalled_count] = present_info->pImageIndices[index];
      ++signalled_count;
      // our first submission consumes the original semaphores; the later ones have nothing left to wait on
      wait = nullptr;
      wait_count = 0u;
      }

    if(signalled_count == 0u)
      return data->QueuePresentKHR(queue, present_info);

    VkPresentInfoKHR patched{*present_info};
    patched.waitSemaphoreCount = signalled_count;
    patched.pWaitSemaphores = signalled.data();

    VkResult const result{data->QueuePresentKHR(queue, &patched)};

    // VK_SUBOPTIMAL_KHR is still a present that happened, so the semaphore was consumed;
    // any other error, OUT_OF_DATE after a window change above all, gives no such certainty
    if(result != VK_SUCCESS and result != VK_SUBOPTIMAL_KHR) [[unlikely]]
      for(uint32_t index{}; index != signalled_count; ++index)
        renew_present_semaphore(*owners[index], owner_images[index]);

    return result;
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_EnumerateInstanceLayerProperties(uint32_t * count, VkLayerProperties * properties)
    -> VkResult
    {
    if(properties == nullptr)
      {
      *count = 1u;
      return VK_SUCCESS;
      }
    if(*count == 0u)
      return VK_INCOMPLETE;

    *count = 1u;
    *properties = VkLayerProperties{};
    std::strncpy(properties->layerName, layer_name, sizeof(properties->layerName) - 1u);
    std::strncpy(properties->description, "Elite Help Tool in-game overlay", sizeof(properties->description) - 1u);
    properties->implementationVersion = 1u;
    properties->specVersion = VK_API_VERSION_1_3;
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL
    overlay_EnumerateDeviceLayerProperties(VkPhysicalDevice, uint32_t * count, VkLayerProperties * properties)
      -> VkResult
    { return overlay_EnumerateInstanceLayerProperties(count, properties); }

  VKAPI_ATTR auto VKAPI_CALL
    overlay_EnumerateInstanceExtensionProperties(char const * layer, uint32_t * count, VkExtensionProperties *)
      -> VkResult
    {
    if(layer == nullptr or std::strcmp(layer, layer_name) != 0)
      return VK_ERROR_LAYER_NOT_PRESENT;

    *count = 0u;
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_EnumerateDeviceExtensionProperties(
    VkPhysicalDevice physical_device, char const * layer, uint32_t * count, VkExtensionProperties * properties
  ) -> VkResult
    {
    // the layer contributes no extensions; a query for our own name is answered with an empty list
    if(layer != nullptr and std::strcmp(layer, layer_name) == 0)
      {
      *count = 0u;
      return VK_SUCCESS;
      }

    instance_data_t * const instance{find_instance(dispatch_key(physical_device))};
    if(instance == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    auto const next{reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
      instance->next_gipa(instance->instance, "vkEnumerateDeviceExtensionProperties")
    )};
    if(next == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    return next(physical_device, layer, count, properties);
    }
  }  // namespace

[[nodiscard]]
auto lookup_hook(char const * name) -> PFN_vkVoidFunction
  {
#define EHT_HOOK(entry)                   \
  if(std::strcmp(name, "vk" #entry) == 0) \
  return reinterpret_cast<PFN_vkVoidFunction>(&overlay_##entry)

  EHT_HOOK(CreateInstance);
  EHT_HOOK(DestroyInstance);
  EHT_HOOK(CreateDevice);
  EHT_HOOK(DestroyDevice);
  EHT_HOOK(GetDeviceQueue);
  EHT_HOOK(GetDeviceQueue2);
  EHT_HOOK(CreateSwapchainKHR);
  EHT_HOOK(DestroySwapchainKHR);
  EHT_HOOK(QueuePresentKHR);
  EHT_HOOK(EnumerateInstanceLayerProperties);
  EHT_HOOK(EnumerateDeviceLayerProperties);
  EHT_HOOK(EnumerateInstanceExtensionProperties);
  EHT_HOOK(EnumerateDeviceExtensionProperties);
#undef EHT_HOOK
  return nullptr;
  }
  }  // namespace eht_overlay

extern "C"
  {
  VK_LAYER_EXPORT VKAPI_ATTR auto VKAPI_CALL eht_overlay_GetInstanceProcAddr(VkInstance instance, char const * name)
    -> PFN_vkVoidFunction
    {
    if(std::strcmp(name, "vkGetInstanceProcAddr") == 0)
      return reinterpret_cast<PFN_vkVoidFunction>(&eht_overlay_GetInstanceProcAddr);

    if(auto const hook{eht_overlay::lookup_hook(name)}; hook != nullptr)
      return hook;

    if(instance == VK_NULL_HANDLE)
      return nullptr;

    eht_overlay::instance_data_t const * const data{
      [instance]() -> eht_overlay::instance_data_t const *
      {
        std::shared_lock const lock{eht_overlay::registry().mutex};
        auto const it{eht_overlay::registry().instances.find(eht_overlay::dispatch_key(instance))};
        return it != eht_overlay::registry().instances.end() ? it->second.get() : nullptr;
      }()
    };

    return data != nullptr ? data->next_gipa(instance, name) : nullptr;
    }

  VK_LAYER_EXPORT VKAPI_ATTR auto VKAPI_CALL eht_overlay_GetDeviceProcAddr(VkDevice device, char const * name)
    -> PFN_vkVoidFunction
    {
    if(std::strcmp(name, "vkGetDeviceProcAddr") == 0)
      return reinterpret_cast<PFN_vkVoidFunction>(&eht_overlay_GetDeviceProcAddr);

    if(auto const hook{eht_overlay::lookup_hook(name)}; hook != nullptr)
      return hook;

    if(device == VK_NULL_HANDLE)
      return nullptr;

    eht_overlay::device_data_t const * const data{
      [device]() -> eht_overlay::device_data_t const *
      {
        std::shared_lock const lock{eht_overlay::registry().mutex};
        auto const it{eht_overlay::registry().devices.find(eht_overlay::dispatch_key(device))};
        return it != eht_overlay::registry().devices.end() ? it->second.get() : nullptr;
      }()
    };

    return data != nullptr ? data->next_gdpa(device, name) : nullptr;
    }

  VK_LAYER_EXPORT VKAPI_ATTR auto VKAPI_CALL vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterface * version)
    -> VkResult
    {
    if(version == nullptr or version->sType != LAYER_NEGOTIATE_INTERFACE_STRUCT)
      return VK_ERROR_INITIALIZATION_FAILED;

    if(version->loaderLayerInterfaceVersion > 2u)
      version->loaderLayerInterfaceVersion = 2u;

    version->pfnGetInstanceProcAddr = &eht_overlay_GetInstanceProcAddr;
    version->pfnGetDeviceProcAddr = &eht_overlay_GetDeviceProcAddr;
    version->pfnGetPhysicalDeviceProcAddr = nullptr;
    return VK_SUCCESS;
    }
  }
