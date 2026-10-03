#include "vk_sample.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace eht_overlay
  {
namespace
  {
  ///\brief a sample asked for by a tool gone quiet this long is no longer made
  constexpr std::chrono::seconds stale_request{3};
  constexpr VkFormat chain_format{VK_FORMAT_R8G8B8A8_UNORM};

  [[nodiscard]]
  auto now_ms() -> uint64_t
    {
    return uint64_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    );
    }

  ///\brief what the tool asks for now, if it asks at all and is still there
  [[nodiscard]]
  auto wanted() -> std::optional<overlay::sample_t>
    {
    auto const snapshot{ipc_client().snapshot()};
    if(not snapshot or snapshot->frame.sample.every_ms == 0u)
      return std::nullopt;
    if(std::chrono::steady_clock::now() - snapshot->at > stale_request)
      return std::nullopt;
    return snapshot->frame.sample;
    }

  [[nodiscard]]
  auto memory_type(device_data_t const & device, uint32_t type_bits, VkMemoryPropertyFlags wanted_flags)
    -> std::optional<uint32_t>
    {
    if(device.instance == nullptr or device.instance->GetPhysicalDeviceMemoryProperties == nullptr)
      return std::nullopt;
    VkPhysicalDeviceMemoryProperties properties{};
    device.instance->GetPhysicalDeviceMemoryProperties(device.physical_device, &properties);
    for(uint32_t index{}; index != properties.memoryTypeCount; ++index)
      if(
        (type_bits & (1u << index)) != 0u
        and (properties.memoryTypes[index].propertyFlags & wanted_flags) == wanted_flags
      )
        return index;
    return std::nullopt;
    }

  ///\brief both formats can be blitted with a linear filter - the swapchain's read, ours written and read
  [[nodiscard]]
  auto blits_supported(swapchain_data_t const & data) -> bool
    {
    device_data_t const & device{*data.device};
    if(device.instance == nullptr or device.instance->GetPhysicalDeviceFormatProperties == nullptr)
      return false;
    VkFormatProperties source{};
    device.instance->GetPhysicalDeviceFormatProperties(device.physical_device, data.format, &source);
    VkFormatProperties chain{};
    device.instance->GetPhysicalDeviceFormatProperties(device.physical_device, chain_format, &chain);
    VkFormatFeatureFlags const needed{
      VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT
      | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
    };
    return (source.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) != 0u
           and (chain.optimalTilingFeatures & needed) == needed;
    }

  auto free_chain(swapchain_data_t & data) -> void
    {
    sample_state_t & sample{data.sample};
    device_data_t & device{*data.device};
    if(sample.image != VK_NULL_HANDLE)
      device.DestroyImage(device.device, sample.image, nullptr);
    if(sample.memory != VK_NULL_HANDLE)
      device.FreeMemory(device.device, sample.memory, nullptr);
    if(sample.buffer != VK_NULL_HANDLE)
      device.DestroyBuffer(device.device, sample.buffer, nullptr);
    if(sample.buffer_memory != VK_NULL_HANDLE)
      device.FreeMemory(device.device, sample.buffer_memory, nullptr);
    sample.image = VK_NULL_HANDLE;
    sample.memory = VK_NULL_HANDLE;
    sample.buffer = VK_NULL_HANDLE;
    sample.buffer_memory = VK_NULL_HANDLE;
    sample.mapped = nullptr;
    sample.levels = 0u;
    }

  ///\brief the image of halvings and the buffer its last level is read into, made for this part and width
  [[nodiscard]]
  auto make_chain(swapchain_data_t & data, VkRect2D area, uint32_t levels) -> bool
    {
    sample_state_t & sample{data.sample};
    device_data_t & device{*data.device};
    free_chain(data);
    uint32_t const width{std::max(1u, area.extent.width / 2u)};
    uint32_t const height{std::max(1u, area.extent.height / 2u)};
    VkImageCreateInfo const image_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = chain_format,
      .extent = VkExtent3D{.width = width, .height = height, .depth = 1u},
      .mipLevels = levels,
      .arrayLayers = 1u,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0u,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if(device.CreateImage(device.device, &image_info, nullptr, &sample.image) != VK_SUCCESS)
      return false;
    VkMemoryRequirements image_needs{};
    device.GetImageMemoryRequirements(device.device, sample.image, &image_needs);
    auto const image_type{memory_type(device, image_needs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    if(not image_type)
      return false;
    VkMemoryAllocateInfo const image_allocate{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = image_needs.size,
      .memoryTypeIndex = *image_type
    };
    if(
      device.AllocateMemory(device.device, &image_allocate, nullptr, &sample.memory) != VK_SUCCESS
      or device.BindImageMemory(device.device, sample.image, sample.memory, 0u) != VK_SUCCESS
    )
      return false;

    sample.levels = levels;
    sample.final_width = std::max(1u, width >> (levels - 1u));
    sample.final_height = std::max(1u, height >> (levels - 1u));
    VkBufferCreateInfo const buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .size = VkDeviceSize{sample.final_width} * sample.final_height * 4u,
      .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0u,
      .pQueueFamilyIndices = nullptr
    };
    if(device.CreateBuffer(device.device, &buffer_info, nullptr, &sample.buffer) != VK_SUCCESS)
      return false;
    VkMemoryRequirements buffer_needs{};
    device.GetBufferMemoryRequirements(device.device, sample.buffer, &buffer_needs);
    // cached makes reading it back fast, coherent spares the invalidate - either will do
    auto type{memory_type(
      device,
      buffer_needs.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT
    )};
    if(not type)
      type = memory_type(
        device, buffer_needs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
      );
    sample.coherent = type.has_value();
    if(not type)
      type = memory_type(device, buffer_needs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if(not type)
      return false;
    VkMemoryAllocateInfo const buffer_allocate{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = buffer_needs.size,
      .memoryTypeIndex = *type
    };
    if(
      device.AllocateMemory(device.device, &buffer_allocate, nullptr, &sample.buffer_memory) != VK_SUCCESS
      or device.BindBufferMemory(device.device, sample.buffer, sample.buffer_memory, 0u) != VK_SUCCESS
      or device.MapMemory(device.device, sample.buffer_memory, 0u, VK_WHOLE_SIZE, 0u, &sample.mapped) != VK_SUCCESS
    )
      return false;
    sample.area = area;
    log(
      "sample chain ready: {}x{} of the screen halved {} times to {}x{}",
      area.extent.width,
      area.extent.height,
      levels,
      sample.final_width,
      sample.final_height
    );
    return true;
    }

  ///\brief the shared file, mapped once and kept
  [[nodiscard]]
  auto map_file(sample_state_t & sample) -> bool
    {
    if(sample.file != nullptr)
      return true;
    std::string const path{overlay::sample_file_path()};
    int const fd{::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0644)};
    if(fd < 0)
      return false;
    if(::ftruncate(fd, off_t(overlay::sample_file_size)) != 0)
      {
      ::close(fd);
      return false;
      }
    void * const mapped{::mmap(nullptr, overlay::sample_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)};
    ::close(fd);
    if(mapped == MAP_FAILED)
      return false;
    sample.file = mapped;
    return true;
    }

  [[nodiscard]]
  auto level_barrier(
    VkImage image, uint32_t level, VkImageLayout from, VkImageLayout to, VkAccessFlags src, VkAccessFlags dst
  ) -> VkImageMemoryBarrier
    {
    return VkImageMemoryBarrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = src,
      .dstAccessMask = dst,
      .oldLayout = from,
      .newLayout = to,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = level,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = 1u
      }
    };
    }
  }  // namespace

auto record_sample(swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index) noexcept -> bool
  {
  try
    {
    sample_state_t & sample{data.sample};
    if(sample.broken or sample.in_flight or not data.capturable or image_index >= data.images.size())
      return false;
    auto const request{wanted()};
    if(not request)
      return false;
    uint64_t const now{now_ms()};
    if(now - sample.last_ms < request->every_ms)
      return false;

    device_data_t & device{*data.device};
    if(
      device.CmdBlitImage == nullptr or device.CreateImage == nullptr or device.CmdCopyImageToBuffer == nullptr
      or not blits_supported(data)
    )
      {
      report("overlay: no sample, the image cannot be blitted here");
      sample.broken = true;
      return false;
      }

    // the part of the screen, as a picture of the middle takes it
    float const share{std::clamp(request->size, 0.05f, 1.f)};
    uint32_t const side{std::min(
      {static_cast<uint32_t>(std::lround(float(data.extent.height) * share)), data.extent.height, data.extent.width}
    )};
    float const aspect{std::clamp(request->aspect, 0.25f, 4.f)};
    uint32_t const width{std::min(static_cast<uint32_t>(std::lround(float(side) * aspect)), data.extent.width)};
    VkRect2D const area{
      .offset
      = {static_cast<int32_t>((data.extent.width - width) / 2u), static_cast<int32_t>((data.extent.height - side) / 2u)},
      .extent = {width, side}
    };
    if(width < 4u or side < 4u)
      return false;
    // halved until the width is not above twice the one asked for, and never above what the file holds
    uint32_t const target{std::clamp(request->width, 32u, overlay::sample_max_side / 2u)};
    uint32_t levels{1u};
    while((std::max(1u, width / 2u) >> (levels - 1u)) > 2u * target and levels < 12u)
      ++levels;
    while(((std::max(1u, width / 2u) >> (levels - 1u)) > overlay::sample_max_side
           or (std::max(1u, side / 2u) >> (levels - 1u)) > overlay::sample_max_side)
          and levels < 12u)
      ++levels;
    bool const same{
      sample.image != VK_NULL_HANDLE and sample.levels == levels and sample.area.extent.width == area.extent.width
      and sample.area.extent.height == area.extent.height and sample.area.offset.x == area.offset.x
      and sample.area.offset.y == area.offset.y
    };
    if(not same and not make_chain(data, area, levels))
      {
      report("overlay: no sample, its image could not be made");
      free_chain(data);
      sample.broken = true;
      return false;
      }

    VkCommandBuffer const cb{frame.command_buffer};
    VkImage const game{data.images[image_index]};
    VkImageMemoryBarrier const game_to_read{level_barrier(
      game,
      0u,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT
    )};
    // every level made anew; the earlier frame's reading of them is waited for
    VkImageMemoryBarrier chain_to_write{level_barrier(
      sample.image,
      0u,
      VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      0u,
      VK_ACCESS_TRANSFER_WRITE_BIT
    )};
    chain_to_write.subresourceRange.levelCount = levels;
    std::array const first{game_to_read, chain_to_write};
    device.CmdPipelineBarrier(
      cb,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0u,
      0u,
      nullptr,
      0u,
      nullptr,
      uint32_t(first.size()),
      first.data()
    );

    auto const size_of = [&](uint32_t level) -> std::array<int32_t, 2>
    {
      return {
        int32_t(std::max(1u, (std::max(1u, area.extent.width / 2u)) >> level)),
        int32_t(std::max(1u, (std::max(1u, area.extent.height / 2u)) >> level))
      };
    };
    VkImageSubresourceLayers const colour{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0u, .baseArrayLayer = 0u, .layerCount = 1u
    };
    auto const [w0, h0]{size_of(0u)};
    VkImageBlit const from_game{
      .srcSubresource = colour,
      .srcOffsets
      = {VkOffset3D{area.offset.x, area.offset.y, 0},
         VkOffset3D{area.offset.x + int32_t(area.extent.width), area.offset.y + int32_t(area.extent.height), 1}},
      .dstSubresource = colour,
      .dstOffsets = {VkOffset3D{0, 0, 0}, VkOffset3D{w0, h0, 1}}
    };
    device.CmdBlitImage(
      cb,
      game,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      sample.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1u,
      &from_game,
      VK_FILTER_LINEAR
    );
    for(uint32_t level{1u}; level != levels; ++level)
      {
      VkImageMemoryBarrier const ready{level_barrier(
        sample.image,
        level - 1u,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT
      )};
      device.CmdPipelineBarrier(
        cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &ready
      );
      auto const [sw, sh]{size_of(level - 1u)};
      auto const [dw, dh]{size_of(level)};
      VkImageSubresourceLayers src{colour};
      src.mipLevel = level - 1u;
      VkImageSubresourceLayers dst{colour};
      dst.mipLevel = level;
      VkImageBlit const halving{
        .srcSubresource = src,
        .srcOffsets = {VkOffset3D{0, 0, 0}, VkOffset3D{sw, sh, 1}},
        .dstSubresource = dst,
        .dstOffsets = {VkOffset3D{0, 0, 0}, VkOffset3D{dw, dh, 1}}
      };
      device.CmdBlitImage(
        cb,
        sample.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        sample.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1u,
        &halving,
        VK_FILTER_LINEAR
      );
      }
    VkImageMemoryBarrier const last_ready{level_barrier(
      sample.image,
      levels - 1u,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT
    )};
    device.CmdPipelineBarrier(
      cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0u, 0u, nullptr, 0u, nullptr, 1u, &last_ready
    );
    VkImageSubresourceLayers last{colour};
    last.mipLevel = levels - 1u;
    VkBufferImageCopy const out{
      .bufferOffset = 0u,
      .bufferRowLength = 0u,
      .bufferImageHeight = 0u,
      .imageSubresource = last,
      .imageOffset = {0, 0, 0},
      .imageExtent = {sample.final_width, sample.final_height, 1u}
    };
    device.CmdCopyImageToBuffer(cb, sample.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, sample.buffer, 1u, &out);

    // the game's image back where the render pass expects it, the copy made visible to the host
    VkImageMemoryBarrier const game_back{level_barrier(
      game,
      0u,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    )};
    VkBufferMemoryBarrier const to_host{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .buffer = sample.buffer,
      .offset = 0u,
      .size = VK_WHOLE_SIZE
    };
    device.CmdPipelineBarrier(
      cb,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_HOST_BIT,
      0u,
      0u,
      nullptr,
      1u,
      &to_host,
      1u,
      &game_back
    );

    sample.in_flight = true;
    sample.last_ms = now;
    sample.taken_ms = now;
    frame.sample_pending = true;
    return true;
    }
  catch(...)
    {
    report("overlay: sampling threw ({}), no samples for this swapchain", exception_text());
    data.sample.broken = true;
    return false;
    }
  }

auto collect_sample(swapchain_data_t & data, frame_resources_t & frame) noexcept -> void
  {
  if(not frame.sample_pending)
    return;
  frame.sample_pending = false;
  sample_state_t & sample{data.sample};
  sample.in_flight = false;
  if(sample.mapped == nullptr or not map_file(sample))
    return;
  device_data_t & device{*data.device};
  size_t const bytes{size_t{sample.final_width} * sample.final_height * 4u};
  if(bytes > overlay::sample_file_size - sizeof(overlay::sample_header_t))
    return;
  if(not sample.coherent and device.InvalidateMappedMemoryRanges != nullptr)
    {
    VkMappedMemoryRange const whole{
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      .pNext = nullptr,
      .memory = sample.buffer_memory,
      .offset = 0u,
      .size = VK_WHOLE_SIZE
    };
    device.InvalidateMappedMemoryRanges(device.device, 1u, &whole);
    }

  auto * const header{static_cast<overlay::sample_header_t *>(sample.file)};
  std::atomic_ref<uint64_t> seq{header->seq};
  // odd while written - the tool leaves a copy made meanwhile
  uint64_t const begun{(seq.load(std::memory_order_relaxed) | 1u) + 2u};
  seq.store(begun, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  std::memcpy(static_cast<uint8_t *>(sample.file) + sizeof(overlay::sample_header_t), sample.mapped, bytes);
  header->magic = overlay::sample_header_t{}.magic;
  header->version = overlay::sample_header_t{}.version;
  header->width = sample.final_width;
  header->height = sample.final_height;
  header->taken_ms = sample.taken_ms;
  header->left = float(sample.area.offset.x) / float(data.extent.width);
  header->top = float(sample.area.offset.y) / float(data.extent.height);
  header->region_width = float(sample.area.extent.width) / float(data.extent.width);
  header->region_height = float(sample.area.extent.height) / float(data.extent.height);
  header->surface_width = data.extent.width;
  header->surface_height = data.extent.height;
  std::atomic_thread_fence(std::memory_order_release);
  seq.store(begun + 1u, std::memory_order_release);
  }

auto destroy_sample(swapchain_data_t & data) noexcept -> void
  {
  if(data.device == nullptr)
    return;
  free_chain(data);
  if(data.sample.file != nullptr)
    ::munmap(data.sample.file, overlay::sample_file_size);
  data.sample = {};
  }
  }  // namespace eht_overlay
