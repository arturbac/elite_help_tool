#include "vk_capture.h"

#include <algorithm>
#include <chrono>
#include <format>
#include <utility>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace eht_overlay
  {
namespace
  {
  ///\brief how a pixel of the swapchain's format is laid out, as far as a picture of it cares
  enum struct layout_e : uint8_t
    {
    unsupported,
    rgba8,
    bgra8,
    ///\brief A2B10G10R10 - red in the low bits
    abgr10,
    ///\brief A2R10G10B10 - blue in the low bits
    argb10
    };

  [[nodiscard]]
  auto pixel_layout(VkFormat format) noexcept -> layout_e
    {
    switch(format)
      {
      case VK_FORMAT_R8G8B8A8_UNORM:
      case VK_FORMAT_R8G8B8A8_SRGB:            return layout_e::rgba8;
      case VK_FORMAT_B8G8R8A8_UNORM:
      case VK_FORMAT_B8G8R8A8_SRGB:            return layout_e::bgra8;
      case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return layout_e::abgr10;
      case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return layout_e::argb10;
      default:                                 return layout_e::unsupported;
      }
    }

  [[nodiscard]]
  auto find_memory_type(device_data_t const & device, uint32_t type_bits, VkMemoryPropertyFlags wanted)
    -> std::optional<uint32_t>
    {
    if(device.instance == nullptr or device.instance->GetPhysicalDeviceMemoryProperties == nullptr)
      return std::nullopt;
    VkPhysicalDeviceMemoryProperties properties{};
    device.instance->GetPhysicalDeviceMemoryProperties(device.physical_device, &properties);
    for(uint32_t index{}; index != properties.memoryTypeCount; ++index)
      if((type_bits & (1u << index)) != 0u and (properties.memoryTypes[index].propertyFlags & wanted) == wanted)
        return index;
    return std::nullopt;
    }

  [[nodiscard]]
  auto ensure_buffer(swapchain_data_t & data, VkDeviceSize size) -> bool
    {
    if(data.capture.buffer != VK_NULL_HANDLE and data.capture.size >= size)
      return true;
    destroy_capture(data);

    device_data_t & device{*data.device};
    VkBufferCreateInfo const info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .size = size,
      .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0u,
      .pQueueFamilyIndices = nullptr
    };
    if(device.CreateBuffer(device.device, &info, nullptr, &data.capture.buffer) != VK_SUCCESS)
      return false;

    VkMemoryRequirements requirements{};
    device.GetBufferMemoryRequirements(device.device, data.capture.buffer, &requirements);

    // coherent memory spares the invalidate; cached makes reading it back fast - either will do
    auto type{find_memory_type(
      device,
      requirements.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    )};
    if(not type)
      type = find_memory_type(
        device, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      );
    data.capture.coherent = type.has_value();
    if(not type)
      type = find_memory_type(device, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if(not type)
      {
      destroy_capture(data);
      return false;
      }

    VkMemoryAllocateInfo const allocate{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = requirements.size,
      .memoryTypeIndex = *type
    };
    if(
      device.AllocateMemory(device.device, &allocate, nullptr, &data.capture.memory) != VK_SUCCESS
      or device.BindBufferMemory(device.device, data.capture.buffer, data.capture.memory, 0u) != VK_SUCCESS
      or device.MapMemory(device.device, data.capture.memory, 0u, VK_WHOLE_SIZE, 0u, &data.capture.mapped) != VK_SUCCESS
    )
      {
      destroy_capture(data);
      return false;
      }
    data.capture.size = size;
    return true;
    }

  ///\brief turns the copied pixels into RGB and writes a PPM, under its final name only when complete
  auto write_picture(std::vector<uint8_t> pixels, layout_e layout, uint32_t width, uint32_t height, std::string path)
    -> void
    {
    std::vector<uint8_t> rgb(size_t{width} * height * 3u);
    for(size_t index{}; index != size_t{width} * height; ++index)
      {
      uint8_t const * const px{pixels.data() + index * 4u};
      uint8_t * const out{rgb.data() + index * 3u};
      switch(layout)
        {
        case layout_e::rgba8:
          out[0] = px[0];
          out[1] = px[1];
          out[2] = px[2];
          break;
        case layout_e::bgra8:
          out[0] = px[2];
          out[1] = px[1];
          out[2] = px[0];
          break;
        case layout_e::abgr10:
        case layout_e::argb10:
            {
            uint32_t word{};
            std::memcpy(&word, px, sizeof(word));
            auto const channel = [word](unsigned shift) -> uint8_t
            { return static_cast<uint8_t>(((word >> shift) & 0x3ffu) * 255u / 1023u); };
            bool const red_low{layout == layout_e::abgr10};
            out[0] = channel(red_low ? 0u : 20u);
            out[1] = channel(10u);
            out[2] = channel(red_low ? 20u : 0u);
            break;
            }
        case layout_e::unsupported: break;
        }
      }

    std::string const partial{path + ".part"};
    std::FILE * const file{std::fopen(partial.c_str(), "wb")};
    if(file == nullptr)
      {
      log("picture not written, cannot open {}", partial);
      return;
      }
    std::fprintf(file, "P6\n%u %u\n255\n", width, height);
    bool const written{std::fwrite(rgb.data(), 1u, rgb.size(), file) == rgb.size()};
    bool const closed{std::fclose(file) == 0};
    if(written and closed)
      {
      std::rename(partial.c_str(), path.c_str());
      log("picture {}x{} written to {}", width, height, path);
      }
    else
      {
      std::remove(partial.c_str());
      log("picture not written to {}", path);
      }
    }
  }  // namespace

auto take_capture_request() -> std::optional<overlay::capture_t>
  {
  static std::mutex mutex;
  static bool primed{};
  static uint64_t served{};

  auto const snapshot{ipc_client().snapshot()};
  if(not snapshot)
    return std::nullopt;

  overlay::capture_t const & request{snapshot->frame.capture};
  std::lock_guard const lock{mutex};
  if(not primed)
    {
    primed = true;
    served = request.id;
    return std::nullopt;
    }
  if(request.id == 0u or request.id == served or request.path.empty())
    return std::nullopt;
  served = request.id;
  return request;
  }

namespace
  {
  ///\brief whether a copy recorded earlier still waits to be read out of the one buffer all frames share
  [[nodiscard]]
  auto copy_in_flight(swapchain_data_t const & data) noexcept -> bool
    {
    return std::ranges::any_of(data.frames, [](frame_resources_t const & f) { return f.capture_pending; });
    }

  ///\brief records the copy of a rectangle of the image into the buffer, into the frame's command buffer
  auto record_copy(
    swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index, VkRect2D area, std::string path
  ) noexcept -> bool
    {
    try
      {
      // the buffer is one for all frames, so a copy waiting to be read out forbids another - and would be
      // freed under it, were the new one larger
      if(not data.capturable or data.capture_broken or copy_in_flight(data) or image_index >= data.images.size())
        return false;

      device_data_t & device{*data.device};
      if(
        device.CmdPipelineBarrier == nullptr or device.CmdCopyImageToBuffer == nullptr or device.CreateBuffer == nullptr
        or device.AllocateMemory == nullptr or device.MapMemory == nullptr
      )
        {
        data.capture_broken = true;
        return false;
        }

      if(pixel_layout(data.format) == layout_e::unsupported)
        {
        log("picture not taken, swapchain format {} is not one we can read", static_cast<int>(data.format));
        data.capture_broken = true;
        return false;
        }

      uint32_t const width{area.extent.width};
      uint32_t const height{area.extent.height};
      if(width == 0u or height == 0u)
        return false;

      if(not ensure_buffer(data, VkDeviceSize{width} * height * 4u))
        {
        log("picture not taken, no buffer for it");
        data.capture_broken = true;
        return false;
        }

      VkImageSubresourceRange const range{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0u,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = 1u
      };
      VkImageMemoryBarrier const to_transfer{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = data.images[image_index],
        .subresourceRange = range
      };
      device.CmdPipelineBarrier(
        frame.command_buffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0u,
        0u,
        nullptr,
        0u,
        nullptr,
        1u,
        &to_transfer
      );

      VkBufferImageCopy const region{
        .bufferOffset = 0u,
        .bufferRowLength = 0u,
        .bufferImageHeight = 0u,
        .imageSubresource
        = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0u, .baseArrayLayer = 0u, .layerCount = 1u},
        .imageOffset = {area.offset.x, area.offset.y, 0},
        .imageExtent = {width, height, 1u}
      };
      device.CmdCopyImageToBuffer(
        frame.command_buffer,
        data.images[image_index],
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        data.capture.buffer,
        1u,
        &region
      );

      // back to where the render pass expects it, and the copy made visible to the host
      VkImageMemoryBarrier const to_present{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = data.images[image_index],
        .subresourceRange = range
      };
      VkBufferMemoryBarrier const to_host{
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = data.capture.buffer,
        .offset = 0u,
        .size = VK_WHOLE_SIZE
      };
      device.CmdPipelineBarrier(
        frame.command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_HOST_BIT,
        0u,
        0u,
        nullptr,
        1u,
        &to_host,
        1u,
        &to_present
      );

      frame.capture_pending = true;
      frame.capture_width = width;
      frame.capture_height = height;
      frame.capture_path = std::move(path);
      return true;
      }
    catch(...)
      {
      data.capture_broken = true;
      return false;
      }
    }
  }  // namespace

auto record_capture(
  swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index, overlay::capture_t const & request
) noexcept -> bool
  {
  // round the very middle of the whole surface - on a triple screen the middle of the centre one; a square
  // unless a wider shape is asked for
  float const share{std::clamp(request.size, 0.05f, 1.f)};
  uint32_t const side{std::min(
    {static_cast<uint32_t>(std::lround(float(data.extent.height) * share)), data.extent.height, data.extent.width}
  )};
  float const aspect{std::clamp(request.aspect, 0.25f, 4.f)};
  uint32_t const width{std::min(static_cast<uint32_t>(std::lround(float(side) * aspect)), data.extent.width)};
  VkRect2D area{
    .offset
    = {static_cast<int32_t>((data.extent.width - width) / 2u), static_cast<int32_t>((data.extent.height - side) / 2u)},
    .extent = {width, side}
  };
  // or the rectangle the tool names, kept within the surface
  if(request.region_width > 0.f and request.region_height > 0.f)
    {
    auto const to_pixels = [](float share, uint32_t whole)
    { return static_cast<uint32_t>(std::lround(std::clamp(share, 0.f, 1.f) * float(whole))); };
    uint32_t const left{to_pixels(request.region_left, data.extent.width)};
    uint32_t const top{to_pixels(request.region_top, data.extent.height)};
    uint32_t const right{std::max(left + 1u, to_pixels(request.region_left + request.region_width, data.extent.width))};
    uint32_t const bottom{std::max(top + 1u, to_pixels(request.region_top + request.region_height, data.extent.height))};
    area = VkRect2D{
      .offset = {static_cast<int32_t>(left), static_cast<int32_t>(top)},
      .extent = {std::min(right, data.extent.width) - left, std::min(bottom, data.extent.height) - top}
    };
    if(left >= data.extent.width or top >= data.extent.height)
      return false;
    }
  try
    {
    return record_copy(data, frame, image_index, area, request.path);
    }
  catch(...)
    {
    return false;
    }
  }

auto record_screenshot(swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index) noexcept -> bool
  {
  try
    {
    uint64_t const moment{uint64_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    )};
    std::string path{std::format("{}/{}{}.ppm", overlay::default_spool_path(), overlay::screenshot_prefix, moment)};
    if(not record_copy(data, frame, image_index, VkRect2D{.offset = {0, 0}, .extent = data.extent}, std::move(path)))
      return false;
    // the whole screen needs a buffer many times the size of a picture; it goes once the copy is read
    frame.capture_release = true;
    return true;
    }
  catch(...)
    {
    return false;
    }
  }

auto collect_capture(swapchain_data_t & data, frame_resources_t & frame) noexcept -> void
  {
  if(not frame.capture_pending)
    return;
  frame.capture_pending = false;

  try
    {
    device_data_t & device{*data.device};
    if(data.capture.mapped == nullptr)
      return;

    VkDeviceSize const bytes{VkDeviceSize{frame.capture_width} * frame.capture_height * 4u};
    if(not data.capture.coherent and device.InvalidateMappedMemoryRanges != nullptr)
      {
      VkMappedMemoryRange const whole{
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .pNext = nullptr,
        .memory = data.capture.memory,
        .offset = 0u,
        .size = VK_WHOLE_SIZE
      };
      device.InvalidateMappedMemoryRanges(device.device, 1u, &whole);
      }

    // one copy on the frame path, a few megabytes; the conversion and the disk go to a thread of their own
    std::vector<uint8_t> pixels(static_cast<size_t>(bytes));
    std::memcpy(pixels.data(), data.capture.mapped, pixels.size());
    std::thread{
      write_picture,
      std::move(pixels),
      pixel_layout(data.format),
      frame.capture_width,
      frame.capture_height,
      std::move(frame.capture_path)
    }
      .detach();
    if(std::exchange(frame.capture_release, false) and not copy_in_flight(data))
      destroy_capture(data);
    }
  catch(...)
    {
    data.capture_broken = true;
    }
  }

auto destroy_capture(swapchain_data_t & data) noexcept -> void
  {
  if(data.device == nullptr)
    return;
  device_data_t & device{*data.device};
  if(data.capture.mapped != nullptr and device.UnmapMemory != nullptr)
    device.UnmapMemory(device.device, data.capture.memory);
  if(data.capture.buffer != VK_NULL_HANDLE and device.DestroyBuffer != nullptr)
    device.DestroyBuffer(device.device, data.capture.buffer, nullptr);
  if(data.capture.memory != VK_NULL_HANDLE and device.FreeMemory != nullptr)
    device.FreeMemory(device.device, data.capture.memory, nullptr);
  data.capture = {};
  }
  }  // namespace eht_overlay
