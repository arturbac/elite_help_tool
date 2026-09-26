///\brief checking the layer without a screen
///
/// VK_EXT_headless_surface gives a swapchain nobody looks at. the game does not notice,
/// and the layer draws exactly as it would in a real window. the image goes back out as a PPM file,
/// so it can be checked whether the overlay really made it into the frame - with no desktop and no game
#include <vulkan/vulkan.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
  {
uint32_t width{1280u};
uint32_t height{720u};
///\brief a few frames, so every swapchain image gets through a present
uint32_t frames{6u};

auto fail(char const * what, VkResult result = VK_SUCCESS) -> int
  {
  std::fprintf(stderr, "headless_check: %s (VkResult %d)\n", what, int(result));
  return 1;
  }

[[nodiscard]]
auto write_ppm(char const * path, std::vector<uint8_t> const & bgra, bool swap_red_blue) -> bool
  {
  std::FILE * const file{std::fopen(path, "wb")};
  if(file == nullptr)
    return false;

  std::fprintf(file, "P6\n%u %u\n255\n", width, height);
  std::vector<uint8_t> row(size_t{width} * 3u);
  for(uint32_t y{}; y != height; ++y)
    {
    for(uint32_t x{}; x != width; ++x)
      {
      uint8_t const * const pixel{bgra.data() + (size_t{y} * width + x) * 4u};
      row[size_t{x} * 3u + 0u] = swap_red_blue ? pixel[2] : pixel[0];
      row[size_t{x} * 3u + 1u] = pixel[1];
      row[size_t{x} * 3u + 2u] = swap_red_blue ? pixel[0] : pixel[2];
      }
    std::fwrite(row.data(), 1u, row.size(), file);
    }
  std::fclose(file);
  return true;
  }
  }  // namespace

auto main(int argc, char ** argv) -> int
  {
  char const * const output{argc > 1 ? argv[1] : "headless_check.ppm"};
  if(argc > 3)
    {
    width = static_cast<uint32_t>(std::atoi(argv[2]));
    height = static_cast<uint32_t>(std::atoi(argv[3]));
    }
  if(argc > 4)
    frames = static_cast<uint32_t>(std::atoi(argv[4]));
  // alt-tab and an in-game resolution change are exactly this - the swapchain dies and is built anew
  uint32_t const rounds{argc > 5 ? static_cast<uint32_t>(std::atoi(argv[5])) : 1u};

  VkApplicationInfo const application{
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = nullptr,
    .pApplicationName = "eht headless check",
    .applicationVersion = 1u,
    .pEngineName = "eht",
    .engineVersion = 1u,
    .apiVersion = VK_API_VERSION_1_1
  };

  char const * const instance_extensions[]{VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME};
  VkInstanceCreateInfo const instance_info{
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0u,
    .pApplicationInfo = &application,
    .enabledLayerCount = 0u,
    .ppEnabledLayerNames = nullptr,
    .enabledExtensionCount = 2u,
    .ppEnabledExtensionNames = instance_extensions
  };

  VkInstance instance{};
  if(auto const result{vkCreateInstance(&instance_info, nullptr, &instance)}; result != VK_SUCCESS)
    return fail("vkCreateInstance", result);

  auto const create_headless{
    reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(vkGetInstanceProcAddr(instance, "vkCreateHeadlessSurfaceEXT"))
  };
  if(create_headless == nullptr)
    return fail("no vkCreateHeadlessSurfaceEXT");

  VkHeadlessSurfaceCreateInfoEXT const surface_info{
    .sType = VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT, .pNext = nullptr, .flags = 0u
  };
  VkSurfaceKHR surface{};
  if(auto const result{create_headless(instance, &surface_info, nullptr, &surface)}; result != VK_SUCCESS)
    return fail("vkCreateHeadlessSurfaceEXT", result);

  uint32_t device_count{};
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

  VkPhysicalDevice physical{};
  uint32_t family{UINT32_MAX};
  for(VkPhysicalDevice candidate: devices)
    {
    uint32_t family_count{};
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());

    for(uint32_t index{}; index != family_count; ++index)
      {
      VkBool32 supported{};
      vkGetPhysicalDeviceSurfaceSupportKHR(candidate, index, surface, &supported);
      if(supported == VK_TRUE and (families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u)
        {
        physical = candidate;
        family = index;
        break;
        }
      }
    if(physical != VK_NULL_HANDLE)
      break;
    }

  if(physical == VK_NULL_HANDLE)
    return fail("no device presenting to a headless surface");

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical, &properties);
  std::printf("urzadzenie: %s, rodzina kolejek %u\n", properties.deviceName, family);

  float const priority{1.f};
  VkDeviceQueueCreateInfo const queue_info{
    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0u,
    .queueFamilyIndex = family,
    .queueCount = 1u,
    .pQueuePriorities = &priority
  };
  char const * const device_extensions[]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkDeviceCreateInfo const device_info{
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0u,
    .queueCreateInfoCount = 1u,
    .pQueueCreateInfos = &queue_info,
    .enabledLayerCount = 0u,
    .ppEnabledLayerNames = nullptr,
    .enabledExtensionCount = 1u,
    .ppEnabledExtensionNames = device_extensions,
    .pEnabledFeatures = nullptr
  };

  VkDevice device{};
  if(auto const result{vkCreateDevice(physical, &device_info, nullptr, &device)}; result != VK_SUCCESS)
    return fail("vkCreateDevice", result);

  VkQueue queue{};
  vkGetDeviceQueue(device, family, 0u, &queue);

  VkSurfaceCapabilitiesKHR capabilities{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &capabilities);

  uint32_t format_count{};
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, nullptr);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count, formats.data());
  if(formats.empty())
    return fail("surface reports no formats");

  VkSurfaceFormatKHR chosen{formats.front()};
  for(VkSurfaceFormatKHR const & candidate: formats)
    if(candidate.format == VK_FORMAT_B8G8R8A8_UNORM or candidate.format == VK_FORMAT_R8G8B8A8_UNORM)
      {
      chosen = candidate;
      break;
      }

  VkImageUsageFlags const wanted{
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
  };
  VkSwapchainCreateInfoKHR const swapchain_info{
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .pNext = nullptr,
    .flags = 0u,
    .surface = surface,
    .minImageCount = std::max(2u, capabilities.minImageCount),
    .imageFormat = chosen.format,
    .imageColorSpace = chosen.colorSpace,
    .imageExtent = VkExtent2D{width, height},
    .imageArrayLayers = 1u,
    .imageUsage = wanted & capabilities.supportedUsageFlags,
    .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0u,
    .pQueueFamilyIndices = nullptr,
    .preTransform = capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = VK_PRESENT_MODE_FIFO_KHR,
    .clipped = VK_FALSE,
    .oldSwapchain = VK_NULL_HANDLE
  };

  VkCommandPoolCreateInfo const pool_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = family
  };
  VkCommandPool pool{};
  vkCreateCommandPool(device, &pool_info, nullptr, &pool);

  VkCommandBufferAllocateInfo const allocate_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext = nullptr,
    .commandPool = pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1u
  };
  VkCommandBuffer command{};
  vkAllocateCommandBuffers(device, &allocate_info, &command);

  VkSemaphoreCreateInfo const semaphore_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0u
  };
  VkSemaphore acquired{};
  VkSemaphore rendered{};
  vkCreateSemaphore(device, &semaphore_info, nullptr, &acquired);
  vkCreateSemaphore(device, &semaphore_info, nullptr, &rendered);

  auto const barrier{[&](VkImage image, VkImageLayout from, VkImageLayout to, VkAccessFlags src, VkAccessFlags dst)
                     {
                       VkImageMemoryBarrier const change{
                         .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                         .pNext = nullptr,
                         .srcAccessMask = src,
                         .dstAccessMask = dst,
                         .oldLayout = from,
                         .newLayout = to,
                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                         .image = image,
                         .subresourceRange = {
                           .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0u,
                           .levelCount = 1u,
                           .baseArrayLayer = 0u,
                           .layerCount = 1u
                         }
                       };
                       vkCmdPipelineBarrier(
                         command,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0u,
                         0u,
                         nullptr,
                         0u,
                         nullptr,
                         1u,
                         &change
                       );
                     }};

  VkSwapchainKHR swapchain{};
  std::vector<VkImage> images;
  uint32_t last_index{};
  auto const loop_started{std::chrono::steady_clock::now()};

  for(uint32_t round{}; round != rounds; ++round)
    {
    if(round != 0u)
      {
      vkDeviceWaitIdle(device);
      vkDestroySwapchainKHR(device, swapchain, nullptr);
      }

    if(auto const result{vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain)}; result != VK_SUCCESS)
      return fail("vkCreateSwapchainKHR", result);

    uint32_t image_count{};
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    images.assign(image_count, VkImage{});
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data());
    if(round == 0u)
      std::printf("lancuch wymiany: %ux%u, %u obrazow\n", width, height, image_count);

    for(uint32_t frame{}; frame != frames; ++frame)
      {
      uint32_t index{};
      if(
        auto const result{vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index)};
        result != VK_SUCCESS and result != VK_SUBOPTIMAL_KHR
      )
        return fail("vkAcquireNextImageKHR", result);
      last_index = index;

      VkCommandBufferBeginInfo const begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
      };
      vkResetCommandBuffer(command, 0u);
      vkBeginCommandBuffer(command, &begin);

      barrier(
        images[index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0u, VK_ACCESS_TRANSFER_WRITE_BIT
      );

      // a background shade that is easy to tell apart from whatever the overlay draws
      VkClearColorValue const colour{.float32 = {0.06f, 0.10f, 0.18f, 1.f}};
      VkImageSubresourceRange const range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0u,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = 1u
      };
      vkCmdClearColorImage(command, images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &colour, 1u, &range);

      barrier(
        images[index],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        0u
      );

      vkEndCommandBuffer(command);

      VkPipelineStageFlags const stage{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
      VkSubmitInfo const submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 1u,
        .pWaitSemaphores = &acquired,
        .pWaitDstStageMask = &stage,
        .commandBufferCount = 1u,
        .pCommandBuffers = &command,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &rendered
      };
      vkQueueSubmit(queue, 1u, &submit, VK_NULL_HANDLE);

      VkPresentInfoKHR const present{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 1u,
        .pWaitSemaphores = &rendered,
        .swapchainCount = 1u,
        .pSwapchains = &swapchain,
        .pImageIndices = &index,
        .pResults = nullptr
      };
      vkQueuePresentKHR(queue, &present);
      vkQueueWaitIdle(queue);
      }
    }

  vkDeviceWaitIdle(device);

  auto const spent{std::chrono::duration<double>{std::chrono::steady_clock::now() - loop_started}.count()};
  std::printf(
    "%u rund po %u klatek w %.3f s, srednio %.3f ms na klatke\n",
    rounds,
    frames,
    spent,
    1000.0 * spent / double(uint64_t{frames} * rounds)
  );

  // reading back what actually remained in the image after the last present
  VkBufferCreateInfo const buffer_info{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0u,
    .size = VkDeviceSize{width} * height * 4u,
    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .queueFamilyIndexCount = 0u,
    .pQueueFamilyIndices = nullptr
  };
  VkBuffer readback{};
  vkCreateBuffer(device, &buffer_info, nullptr, &readback);

  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(device, readback, &requirements);

  VkPhysicalDeviceMemoryProperties memory_properties{};
  vkGetPhysicalDeviceMemoryProperties(physical, &memory_properties);

  uint32_t memory_type{UINT32_MAX};
  for(uint32_t index{}; index != memory_properties.memoryTypeCount; ++index)
    {
    constexpr VkMemoryPropertyFlags needed{VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    if(
      (requirements.memoryTypeBits & (1u << index)) != 0u
      and (memory_properties.memoryTypes[index].propertyFlags & needed) == needed
    )
      {
      memory_type = index;
      break;
      }
    }
  if(memory_type == UINT32_MAX)
    return fail("no host visible memory type");

  VkMemoryAllocateInfo const memory_info{
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .pNext = nullptr,
    .allocationSize = requirements.size,
    .memoryTypeIndex = memory_type
  };
  VkDeviceMemory memory{};
  vkAllocateMemory(device, &memory_info, nullptr, &memory);
  vkBindBufferMemory(device, readback, memory, 0u);

  VkCommandBufferBeginInfo const begin{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    .pInheritanceInfo = nullptr
  };
  vkResetCommandBuffer(command, 0u);
  vkBeginCommandBuffer(command, &begin);

  barrier(
    images[last_index],
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    0u,
    VK_ACCESS_TRANSFER_READ_BIT
  );

  VkBufferImageCopy const copy{
    .bufferOffset = 0u,
    .bufferRowLength = 0u,
    .bufferImageHeight = 0u,
    .imageSubresource
    = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0u, .baseArrayLayer = 0u, .layerCount = 1u},
    .imageOffset = {0, 0, 0},
    .imageExtent = {width, height, 1u}
  };
  vkCmdCopyImageToBuffer(command, images[last_index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback, 1u, &copy);
  vkEndCommandBuffer(command);

  VkSubmitInfo const copy_submit{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pNext = nullptr,
    .waitSemaphoreCount = 0u,
    .pWaitSemaphores = nullptr,
    .pWaitDstStageMask = nullptr,
    .commandBufferCount = 1u,
    .pCommandBuffers = &command,
    .signalSemaphoreCount = 0u,
    .pSignalSemaphores = nullptr
  };
  vkQueueSubmit(queue, 1u, &copy_submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  void * mapped{};
  vkMapMemory(device, memory, 0u, requirements.size, 0u, &mapped);
  std::vector<uint8_t> pixels(size_t{width} * height * 4u);
  std::memcpy(pixels.data(), mapped, pixels.size());
  vkUnmapMemory(device, memory);

  if(not write_ppm(output, pixels, chosen.format == VK_FORMAT_B8G8R8A8_UNORM))
    return fail("could not write output");

  std::printf("zapisano %s\n", output);

  vkDestroyBuffer(device, readback, nullptr);
  vkFreeMemory(device, memory, nullptr);
  vkDestroySemaphore(device, acquired, nullptr);
  vkDestroySemaphore(device, rendered, nullptr);
  vkDestroyCommandPool(device, pool, nullptr);
  vkDestroySwapchainKHR(device, swapchain, nullptr);
  vkDestroyDevice(device, nullptr);
  vkDestroySurfaceKHR(instance, surface, nullptr);
  vkDestroyInstance(instance, nullptr);
  return 0;
  }
