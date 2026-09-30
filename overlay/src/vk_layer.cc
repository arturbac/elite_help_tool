// the layer the Vulkan loader knows: it tracks the game's objects and hands them to the overlay plugin
//
// Nothing is drawn here. The plugin (libeht_overlay_plugin.so, beside this library) does all of it, and
// is loaded again when its file is replaced - the old one is detached from everything, shut down and
// unloaded first, so neither a thread nor a byte of it is left behind
#include "plugin_api.h"
#include "vk_log.h"

// the Vulkan headers do not carry this macro, and the layer's symbols must break through hidden visibility
#ifndef VK_LAYER_EXPORT
#define VK_LAYER_EXPORT __attribute__((visibility("default")))
#endif

#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace eht_overlay
  {
namespace
  {
  constexpr char layer_name[]{"VK_LAYER_EHT_overlay"};
  constexpr char plugin_file[]{"libeht_overlay_plugin.so"};
  ///\brief how often the plugin's file is looked at for a new one
  constexpr std::chrono::milliseconds reload_check{1000};

  ///\brief the loader keeps the dispatch table in the first word of every dispatchable handle
  [[nodiscard]]
  auto dispatch_key(void * handle) noexcept -> void *
    { return *static_cast<void **>(handle); }

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

  struct instance_data_t
    {
    VkInstance instance{};
    PFN_vkGetInstanceProcAddr next_gipa{};
    PFN_vkDestroyInstance DestroyInstance{};
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
    PFN_vkQueueWaitIdle QueueWaitIdle{};
    PFN_vkDeviceWaitIdle DeviceWaitIdle{};

    ///\brief the queue family is only learned from vkGetDeviceQueue, and the plugin needs it at present time
    std::mutex queues_mutex;
    std::unordered_map<void *, uint32_t> queue_family_of;

    ///\brief what the plugin keeps of it, once attached - the plugin's lock guards both
    void * plugin{};
    bool attached{};
    };

  struct swapchain_data_t
    {
    device_data_t * device{};
    VkSwapchainKHR swapchain{};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
    bool capturable{};
    std::vector<VkImage> images;
    void * plugin{};
    bool attached{};
    };

  ///\brief the file a plugin was loaded from: another file under the same name is a new plugin
  struct identity_t
    {
    dev_t device{};
    ino_t inode{};
    timespec modified{};
    off_t size{};

    [[nodiscard]]
    auto operator==(identity_t const & other) const noexcept -> bool
      {
      return device == other.device and inode == other.inode and modified.tv_sec == other.modified.tv_sec
             and modified.tv_nsec == other.modified.tv_nsec and size == other.size;
      }
    };

  [[nodiscard]]
  auto identity_of(struct stat const & status) noexcept -> identity_t
    {
    return identity_t{
      .device = status.st_dev, .inode = status.st_ino, .modified = status.st_mtim, .size = status.st_size
    };
    }

  ///\brief one plugin loaded: its code, and the copy of its file it was loaded from
  struct plugin_t
    {
    void * library{};
    ///\brief the file copied into memory - the loader knows a library by its name, and a new file under
    /// the old name would only be given the old library back. Kept open while the library is loaded
    int memory_fd{-1};
    identity_t identity;
    eht_plugin_t api{};

    plugin_t() = default;
    plugin_t(plugin_t const &) = delete;
    auto operator=(plugin_t const &) -> plugin_t & = delete;

    ~plugin_t()
      {
      if(library != nullptr)
        ::dlclose(library);
      if(memory_fd >= 0)
        ::close(memory_fd);
      }
    };

  ///\brief the plugin's file, beside this library unless EHT_OVERLAY_PLUGIN names another
  [[nodiscard]]
  auto plugin_path() -> std::string
    {
    if(char const * const path{std::getenv("EHT_OVERLAY_PLUGIN")}; path != nullptr and *path != '\0')
      return path;
    Dl_info info{};
    if(::dladdr(reinterpret_cast<void *>(&process_skipped), &info) == 0 or info.dli_fname == nullptr)
      return plugin_file;
    std::string own{info.dli_fname};
    size_t const slash{own.rfind('/')};
    return slash == std::string::npos ? std::string{plugin_file} : own.substr(0u, slash + 1u) + plugin_file;
    }

  ///\brief the plugin in the file now, or nothing when it cannot be loaded - the reason goes to the log
  [[nodiscard]]
  auto load_plugin(std::string const & path) -> std::unique_ptr<plugin_t>
    {
    int const file{::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
    if(file < 0)
      {
      log("no plugin at {}", path);
      return nullptr;
      }
    auto plugin{std::make_unique<plugin_t>()};
    struct stat status{};
    bool copied{::fstat(file, &status) == 0};
    plugin->identity = identity_of(status);
    if(copied)
      {
      plugin->memory_fd = ::memfd_create("eht_overlay_plugin", MFD_CLOEXEC);
      copied = plugin->memory_fd >= 0;
      }
    std::array<char, 1u << 16u> chunk{};
    for(ssize_t read; copied and (read = ::read(file, chunk.data(), chunk.size())) != 0;)
      copied = read > 0 and ::write(plugin->memory_fd, chunk.data(), size_t(read)) == read;
    ::close(file);
    if(not copied)
      {
      log("plugin {} could not be copied into memory", path);
      return nullptr;
      }

    std::string const name{std::format("/proc/self/fd/{}", plugin->memory_fd)};
    plugin->library = ::dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
    if(plugin->library == nullptr)
      {
      char const * const error{::dlerror()};
      log("plugin {} not loaded: {}", path, error != nullptr ? error : "?");
      return nullptr;
      }
    auto const entry{reinterpret_cast<eht_overlay_plugin_entry_t>(::dlsym(plugin->library, EHT_OVERLAY_PLUGIN_ENTRY))};
    plugin->api.abi = EHT_OVERLAY_PLUGIN_ABI;
    plugin->api.size = sizeof(eht_plugin_t);
    if(entry == nullptr or entry(&plugin->api) == 0)
      {
      log("plugin {} refused: not of this layer's interface {}", path, EHT_OVERLAY_PLUGIN_ABI);
      return nullptr;
      }
    log("plugin loaded from {}", path);
    return plugin;
    }

  ///\brief everything the layer holds, destroyed only at the end of the process
  struct layer_t
    {
    ///\brief guards the maps below
    std::shared_mutex registry_mutex;
    std::unordered_map<void *, std::unique_ptr<instance_data_t>> instances;
    std::unordered_map<void *, std::unique_ptr<device_data_t>> devices;
    std::unordered_map<VkSwapchainKHR, std::unique_ptr<swapchain_data_t>> swapchains;

    ///\brief guards the plugin and every device's and swapchain's part of it: shared by the presents,
    /// held alone to attach, detach and load. Taken before registry_mutex, never after
    std::shared_mutex plugin_mutex;
    std::unique_ptr<plugin_t> plugin;
    ///\brief loaded once, at the first swapchain; a file that failed is not tried again until it changes
    bool plugin_tried{};
    std::optional<identity_t> refused;
    std::string path;
    std::atomic<int64_t> next_check_ms{};

    layer_t() = default;
    layer_t(layer_t const &) = delete;
    auto operator=(layer_t const &) -> layer_t & = delete;

    ///\brief a process ending without destroying its instance still gets its plugin shut down
    ~layer_t()
      {
      std::unique_lock const lock{plugin_mutex};
      for(auto & [key, device]: devices)
        if(device->plugin != nullptr and device->DeviceWaitIdle != nullptr)
          device->DeviceWaitIdle(device->device);
      unload_locked();
      }

    [[nodiscard]]
    auto find_instance(void * key) -> instance_data_t *
      {
      std::shared_lock const lock{registry_mutex};
      auto const it{instances.find(key)};
      return it != instances.end() ? it->second.get() : nullptr;
      }

    [[nodiscard]]
    auto find_device(void * key) -> device_data_t *
      {
      std::shared_lock const lock{registry_mutex};
      auto const it{devices.find(key)};
      return it != devices.end() ? it->second.get() : nullptr;
      }

    [[nodiscard]]
    auto find_swapchain(VkSwapchainKHR handle) -> swapchain_data_t *
      {
      std::shared_lock const lock{registry_mutex};
      auto const it{swapchains.find(handle)};
      return it != swapchains.end() ? it->second.get() : nullptr;
      }

    // --- everything below runs with plugin_mutex held alone

    auto attach_locked(device_data_t & device) -> void
      {
      if(device.attached or not plugin)
        return;
      device.attached = true;
      eht_plugin_device_t const host{
        .instance = device.instance->instance,
        .physical_device = device.physical_device,
        .device = device.device,
        .next_gipa = device.instance->next_gipa,
        .next_gdpa = device.next_gdpa,
        .set_device_loader_data = device.set_device_loader_data,
        .api_version = device.instance->api_version,
        .queue_family = &queue_family,
        .host = &device
      };
      device.plugin = plugin->api.attach_device(&host);
      }

    auto attach_locked(swapchain_data_t & swapchain) -> void
      {
      if(swapchain.attached or not plugin)
        return;
      attach_locked(*swapchain.device);
      swapchain.attached = true;
      eht_plugin_swapchain_t const host{
        .swapchain = swapchain.swapchain,
        .format = swapchain.format,
        .extent = swapchain.extent,
        .image_count = uint32_t(swapchain.images.size()),
        .images = swapchain.images.data(),
        .capturable = swapchain.capturable ? 1u : 0u
      };
      swapchain.plugin = plugin->api.attach_swapchain(swapchain.device->plugin, &host);
      }

    auto detach_locked(swapchain_data_t & swapchain) -> void
      {
      if(swapchain.plugin != nullptr and plugin)
        plugin->api.detach_swapchain(swapchain.plugin);
      swapchain.plugin = nullptr;
      swapchain.attached = false;
      }

    ///\brief the device's swapchains must have been detached already
    auto detach_locked(device_data_t & device) -> void
      {
      if(device.plugin != nullptr and plugin)
        plugin->api.detach_device(device.plugin);
      device.plugin = nullptr;
      device.attached = false;
      }

    ///\brief everything detached, the plugin shut down and unloaded; the device must be idle
    auto unload_locked() -> void
      {
      if(not plugin)
        return;
        {
        std::shared_lock const lock{registry_mutex};
        for(auto & [handle, swapchain]: swapchains)
          detach_locked(*swapchain);
        for(auto & [key, device]: devices)
          detach_locked(*device);
        }
      plugin->api.shutdown();
      plugin.reset();
      log("plugin unloaded");
      }

    auto attach_all_locked() -> void
      {
      std::shared_lock const lock{registry_mutex};
      for(auto & [handle, swapchain]: swapchains)
        attach_locked(*swapchain);
      }

    ///\brief the first plugin, loaded at the first swapchain rather than in every process using Vulkan
    auto ensure_plugin_locked() -> void
      {
      if(plugin_tried)
        return;
      plugin_tried = true;
      path = plugin_path();
      plugin = load_plugin(path);
      if(not plugin)
        if(struct stat status{}; ::stat(path.c_str(), &status) == 0)
          refused = identity_of(status);
      }

    ///\brief called at every present: at most once a check period, a look at the file; when it holds
    /// another plugin, the one running is replaced by it
    ///\detail the queue given is the one presenting, which the game synchronises for us: waiting for it to
    /// go idle is the wait for every present that may still read the overlay's semaphores. The overlay's
    /// own submissions are waited for by the plugin as it detaches
    auto check_reload(VkQueue queue, device_data_t & presenting) -> void
      {
      int64_t const now{std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now().time_since_epoch()
      )
                          .count()};
      int64_t due{next_check_ms.load(std::memory_order_relaxed)};
      if(now < due or not next_check_ms.compare_exchange_strong(due, now + reload_check.count()))
        return;

      identity_t current{};
        {
        std::shared_lock const lock{plugin_mutex};
        if(not plugin_tried)
          return;
        struct stat status{};
        if(::stat(path.c_str(), &status) != 0)
          return;
        current = identity_of(status);
        if((plugin and plugin->identity == current) or (refused and *refused == current))
          return;
        }

      std::unique_lock const lock{plugin_mutex};
      auto next{load_plugin(path)};
      if(not next)
        {
        // the one running goes on
        refused = current;
        return;
        }
      refused.reset();
      if(presenting.QueueWaitIdle != nullptr)
        presenting.QueueWaitIdle(queue);
      unload_locked();
      plugin = std::move(next);
      attach_all_locked();
      log("plugin reloaded");
      }

    [[nodiscard]]
    static auto queue_family(void * host, VkQueue queue) -> uint32_t
      {
      auto & device{*static_cast<device_data_t *>(host)};
      std::lock_guard const lock{device.queues_mutex};
      auto const it{device.queue_family_of.find(static_cast<void *>(queue))};
      return it != device.queue_family_of.end() ? it->second : VK_QUEUE_FAMILY_IGNORED;
      }
    };

  [[nodiscard]]
  auto layer() -> layer_t &
    {
    // destroyed at the end of the process, after the game destroyed its instances or not
    static layer_t instance;
    return instance;
    }

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
      std::unique_lock const lock{layer().registry_mutex};
      layer().instances[dispatch_key(*instance)] = std::move(data);
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
    bool last{};
      {
      std::unique_lock const lock{layer().registry_mutex};
      if(auto const it{layer().instances.find(key)}; it != layer().instances.end())
        {
        destroy = it->second->DestroyInstance;
        layer().instances.erase(it);
        }
      last = layer().instances.empty() and layer().devices.empty();
      }

    // the game is done with Vulkan: the plugin goes, threads and all, rather than at the end of the process
    if(last)
      {
      std::unique_lock const lock{layer().plugin_mutex};
      layer().unload_locked();
      layer().plugin_tried = false;
      layer().refused.reset();
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
    instance_data_t * const instance{layer().find_instance(dispatch_key(physical_device))};
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
    EHT_LOAD(QueueWaitIdle);
    EHT_LOAD(DeviceWaitIdle);
#undef EHT_LOAD

    log("device created");

      {
      std::unique_lock const lock{layer().registry_mutex};
      layer().devices[dispatch_key(*device)] = std::move(data);
      }
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_DestroyDevice(VkDevice device, VkAllocationCallbacks const * allocator) -> void
    {
    if(device == VK_NULL_HANDLE)
      return;

    void * const key{dispatch_key(device)};
    std::unique_ptr<device_data_t> data;
    std::vector<std::unique_ptr<swapchain_data_t>> orphans;

    std::unique_lock const plugin_lock{layer().plugin_mutex};
      {
      std::unique_lock const lock{layer().registry_mutex};
      if(auto const it{layer().devices.find(key)}; it != layer().devices.end())
        {
        data = std::move(it->second);
        layer().devices.erase(it);
        }
      for(auto it{layer().swapchains.begin()}; it != layer().swapchains.end();)
        if(it->second->device == data.get())
          {
          orphans.push_back(std::move(it->second));
          it = layer().swapchains.erase(it);
          }
        else
          ++it;
      }
    if(not data)
      return;

    // the overlay's resources must go before the device does, otherwise the driver reports a leak
    if(data->DeviceWaitIdle != nullptr and data->plugin != nullptr)
      data->DeviceWaitIdle(device);
    for(auto & orphan: orphans)
      layer().detach_locked(*orphan);
    layer().detach_locked(*data);

    if(data->DestroyDevice != nullptr)
      data->DestroyDevice(device, allocator);
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_GetDeviceQueue(VkDevice device, uint32_t family, uint32_t index, VkQueue * queue)
    -> void
    {
    device_data_t * const data{layer().find_device(dispatch_key(device))};
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
    device_data_t * const data{layer().find_device(dispatch_key(device))};
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
    device_data_t * const data{layer().find_device(dispatch_key(device))};
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

    log(
      "swapchain {}x{} with {} images, usage {:#x} of the game's {:#x}",
      entry->extent.width,
      entry->extent.height,
      entry->images.size(),
      patched.imageUsage,
      create_info->imageUsage
    );

    std::unique_lock const plugin_lock{layer().plugin_mutex};
    layer().ensure_plugin_locked();
    swapchain_data_t & added{*entry};
      {
      std::unique_lock const lock{layer().registry_mutex};
      layer().swapchains[*swapchain] = std::move(entry);
      }
    layer().attach_locked(added);
    return VK_SUCCESS;
    }

  VKAPI_ATTR auto VKAPI_CALL
    overlay_DestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, VkAllocationCallbacks const * allocator)
      -> void
    {
    device_data_t * const data{layer().find_device(dispatch_key(device))};
    if(data == nullptr or data->DestroySwapchainKHR == nullptr)
      return;

      {
      std::unique_lock const plugin_lock{layer().plugin_mutex};
      std::unique_ptr<swapchain_data_t> entry;
        {
        std::unique_lock const lock{layer().registry_mutex};
        if(auto const it{layer().swapchains.find(swapchain)}; it != layer().swapchains.end())
          {
          entry = std::move(it->second);
          layer().swapchains.erase(it);
          }
        }
      if(entry and entry->plugin != nullptr)
        {
        if(data->DeviceWaitIdle != nullptr)
          data->DeviceWaitIdle(device);
        layer().detach_locked(*entry);
        }
      }

    data->DestroySwapchainKHR(device, swapchain, allocator);
    }

  VKAPI_ATTR auto VKAPI_CALL overlay_QueuePresentKHR(VkQueue queue, VkPresentInfoKHR const * present_info) -> VkResult
    {
    device_data_t * const data{layer().find_device(dispatch_key(queue))};
    if(data == nullptr or data->QueuePresentKHR == nullptr)
      return VK_ERROR_INITIALIZATION_FAILED;

    if(present_info == nullptr or present_info->swapchainCount == 0u)
      return data->QueuePresentKHR(queue, present_info);

    layer().check_reload(queue, *data);

    std::shared_lock const plugin_lock{layer().plugin_mutex};
    if(not layer().plugin)
      return data->QueuePresentKHR(queue, present_info);
    eht_plugin_t const & plugin{layer().plugin->api};

    std::array<VkSemaphore, max_swapchains_per_present> signalled{};
    // after a failed present we need to know whose semaphore to clean up
    std::array<void *, max_swapchains_per_present> owners{};
    std::array<uint32_t, max_swapchains_per_present> owner_images{};
    uint32_t signalled_count{};

    VkSemaphore const * wait{present_info->pWaitSemaphores};
    uint32_t wait_count{present_info->waitSemaphoreCount};

    uint32_t const count{std::min(present_info->swapchainCount, max_swapchains_per_present)};
    for(uint32_t index{}; index != count; ++index)
      {
      swapchain_data_t * const entry{layer().find_swapchain(present_info->pSwapchains[index])};
      if(entry == nullptr or entry->plugin == nullptr)
        continue;

      VkSemaphore const semaphore{plugin.present(entry->plugin, queue, present_info->pImageIndices[index], wait, wait_count)};
      if(semaphore == VK_NULL_HANDLE)
        continue;

      signalled[signalled_count] = semaphore;
      owners[signalled_count] = entry->plugin;
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
        plugin.present_failed(owners[index], owner_images[index]);

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

    instance_data_t * const instance{eht_overlay::layer().find_instance(dispatch_key(physical_device))};
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

[[nodiscard]]
auto next_instance_proc(VkInstance instance, char const * name) -> PFN_vkVoidFunction
  {
  instance_data_t const * const data{layer().find_instance(dispatch_key(instance))};
  return data != nullptr ? data->next_gipa(instance, name) : nullptr;
  }

[[nodiscard]]
auto next_device_proc(VkDevice device, char const * name) -> PFN_vkVoidFunction
  {
  device_data_t const * const data{layer().find_device(dispatch_key(device))};
  return data != nullptr ? data->next_gdpa(device, name) : nullptr;
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

    return eht_overlay::next_instance_proc(instance, name);
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

    return eht_overlay::next_device_proc(device, name);
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
