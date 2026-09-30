#include "vk_faces.h"
#include "vk_service.h"

#include <backends/imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include <vector>

namespace eht_overlay
  {
namespace
  {
  constexpr uint32_t atlas_side{face_side * faces_across};
  constexpr uint32_t tile_count{faces_across * faces_across};
  constexpr VkDeviceSize tile_bytes{VkDeviceSize{face_side} * face_side * 4u};
  ///\brief how many faces one frame copies at most - a system map comes in over a few frames
  constexpr uint32_t uploads_per_frame{4u};

  ///\brief reads the faces off the game's frame: the frame asks for a name, a later frame takes the pixels
  class loader_t final
    {
  public:
    loader_t() : thread_{[this] { run(); }} {}

    loader_t(loader_t const &) = delete;
    auto operator=(loader_t const &) -> loader_t & = delete;

    ~loader_t()
      {
        {
        std::scoped_lock const lock{mutex_};
        stopping_ = true;
        }
      wake_.notify_one();
      thread_.join();
      }

    auto ask(std::string const & name) -> void
      {
      std::scoped_lock const lock{mutex_};
      if(not asked_.insert(name).second)
        return;
      wanted_.push_back(name);
      wake_.notify_one();
      }

    ///\brief the faces read so far, taken out - a failed one comes with no pixels
    [[nodiscard]]
    auto take(size_t most) -> std::vector<std::pair<std::string, std::vector<uint8_t>>>
      {
      std::scoped_lock const lock{mutex_};
      std::vector<std::pair<std::string, std::vector<uint8_t>>> taken;
      while(not loaded_.empty() and taken.size() != most)
        {
        taken.push_back(std::move(loaded_.front()));
        loaded_.pop_front();
        }
      return taken;
      }

    ///\brief a name whose tile was given to another face may be asked for again
    auto forget(std::string const & name) -> void
      {
      std::scoped_lock const lock{mutex_};
      asked_.erase(name);
      }

  private:
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::string> wanted_;
    std::set<std::string> asked_;
    std::deque<std::pair<std::string, std::vector<uint8_t>>> loaded_;
    bool stopping_{};
    // last: it runs over everything above
    std::thread thread_;

    auto run() -> void
      {
      for(;;)
        {
        std::string name;
          {
          std::unique_lock lock{mutex_};
          wake_.wait(lock, [this] { return stopping_ or not wanted_.empty(); });
          if(stopping_)
            return;
          name = std::move(wanted_.front());
          wanted_.pop_front();
          }
        std::vector<uint8_t> rgba{read(name)};
        if(rgba.empty())
          log("face {} could not be read", name);
        std::scoped_lock const lock{mutex_};
        loaded_.emplace_back(std::move(name), std::move(rgba));
        }
      }

    ///\brief a binary PPM of exactly the face's size, as RGBA
    [[nodiscard]]
    static auto read(std::string const & path) -> std::vector<uint8_t>
      {
      std::ifstream in{path, std::ios::binary};
      std::string magic;
      uint32_t width{};
      uint32_t height{};
      uint32_t maximum{};
      in >> magic >> width >> height >> maximum;
      in.get();
      if(not in or magic != "P6" or maximum != 255u or width != face_side or height != face_side)
        return {};
      std::vector<uint8_t> rgb(size_t{face_side} * face_side * 3u);
      in.read(reinterpret_cast<char *>(rgb.data()), std::streamsize(rgb.size()));
      if(not in)
        return {};
      std::vector<uint8_t> rgba(size_t{face_side} * face_side * 4u);
      for(size_t at{}; at != size_t{face_side} * face_side; ++at)
        {
        std::memcpy(rgba.data() + at * 4u, rgb.data() + at * 3u, 3u);
        rgba[at * 4u + 3u] = 255u;
        }
      return rgba;
      }
    };

  service_t<loader_t> loaders;

  [[nodiscard]]
  auto loader() -> loader_t &
    { return loaders.get([] { return std::make_unique<loader_t>(); }); }

  [[nodiscard]]
  auto memory_type(device_data_t const & device, uint32_t type_bits, VkMemoryPropertyFlags wanted)
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

  ///\brief the atlas, its sampler and its staging buffer, made when the first face is ready
  [[nodiscard]]
  auto make_atlas(swapchain_data_t & data) -> bool
    {
    face_atlas_t & atlas{data.faces};
    device_data_t & device{*data.device};
    if(
      device.CreateImage == nullptr or device.GetImageMemoryRequirements == nullptr or device.BindImageMemory == nullptr
      or device.CreateSampler == nullptr or device.CmdCopyBufferToImage == nullptr
    )
      return false;

    VkImageCreateInfo const image_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .extent = VkExtent3D{.width = atlas_side, .height = atlas_side, .depth = 1u},
      .mipLevels = 1u,
      .arrayLayers = 1u,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0u,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    if(device.CreateImage(device.device, &image_info, nullptr, &atlas.image) != VK_SUCCESS)
      return false;
    VkMemoryRequirements image_needs{};
    device.GetImageMemoryRequirements(device.device, atlas.image, &image_needs);
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
      device.AllocateMemory(device.device, &image_allocate, nullptr, &atlas.memory) != VK_SUCCESS
      or device.BindImageMemory(device.device, atlas.image, atlas.memory, 0u) != VK_SUCCESS
    )
      return false;

    VkImageViewCreateInfo const view_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .image = atlas.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = VK_FORMAT_R8G8B8A8_UNORM,
      .components = {},
      .subresourceRange = VkImageSubresourceRange{
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0u,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = 1u
      }
    };
    if(device.CreateImageView(device.device, &view_info, nullptr, &atlas.view) != VK_SUCCESS)
      return false;

    VkSamplerCreateInfo const sampler_info{
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 1.f,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_ALWAYS,
      .minLod = 0.f,
      .maxLod = 0.f,
      .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
      .unnormalizedCoordinates = VK_FALSE
    };
    if(device.CreateSampler(device.device, &sampler_info, nullptr, &atlas.sampler) != VK_SUCCESS)
      return false;

    // each swapchain image its own part, so a frame never writes where one in flight still reads
    VkDeviceSize const staging_size{tile_bytes * uploads_per_frame * std::max<VkDeviceSize>(1u, data.frames.size())};
    VkBufferCreateInfo const buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .size = staging_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0u,
      .pQueueFamilyIndices = nullptr
    };
    if(device.CreateBuffer(device.device, &buffer_info, nullptr, &atlas.staging) != VK_SUCCESS)
      return false;
    VkMemoryRequirements buffer_needs{};
    device.GetBufferMemoryRequirements(device.device, atlas.staging, &buffer_needs);
    auto const buffer_type{memory_type(
      device, buffer_needs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    )};
    if(not buffer_type)
      return false;
    VkMemoryAllocateInfo const buffer_allocate{
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = nullptr,
      .allocationSize = buffer_needs.size,
      .memoryTypeIndex = *buffer_type
    };
    if(
      device.AllocateMemory(device.device, &buffer_allocate, nullptr, &atlas.staging_memory) != VK_SUCCESS
      or device.BindBufferMemory(device.device, atlas.staging, atlas.staging_memory, 0u) != VK_SUCCESS
      or device.MapMemory(device.device, atlas.staging_memory, 0u, VK_WHOLE_SIZE, 0u, &atlas.mapped) != VK_SUCCESS
    )
      return false;
    atlas.staging_frames = std::max<size_t>(1u, data.frames.size());

    atlas.set = ImGui_ImplVulkan_AddTexture(atlas.sampler, atlas.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if(atlas.set == VK_NULL_HANDLE)
      return false;
    atlas.names.assign(tile_count, {});
    atlas.used.assign(tile_count, 0u);
    atlas.ready.assign(tile_count, false);
    atlas.fresh = true;
    log("face atlas ready, {} faces of {} px", tile_count, face_side);
    return true;
    }

  ///\brief the tile for a face coming in: its own, a free one, or the one drawn longest ago and not this frame
  [[nodiscard]]
  auto tile_for(face_atlas_t & atlas, std::string const & name, uint64_t frame) -> std::optional<uint32_t>
    {
    if(auto const it{std::ranges::find(atlas.names, name)}; it != atlas.names.end())
      return uint32_t(it - atlas.names.begin());
    std::optional<uint32_t> oldest;
    for(uint32_t tile{}; tile != tile_count; ++tile)
      {
      if(atlas.names[tile].empty())
        return tile;
      if(atlas.used[tile] < frame and (not oldest or atlas.used[tile] < atlas.used[*oldest]))
        oldest = tile;
      }
    return oldest;
    }
  }  // namespace

auto face_tile(swapchain_data_t & data, std::string const & name) -> std::optional<uint32_t>
  {
  face_atlas_t & atlas{data.faces};
  if(atlas.broken or name.empty())
    return std::nullopt;
  if(auto const it{std::ranges::find(atlas.names, name)}; it != atlas.names.end())
    {
    auto const tile{uint32_t(it - atlas.names.begin())};
    atlas.used[tile] = data.drawn_frames;
    if(atlas.ready[tile])
      return tile;
    return std::nullopt;
    }
  loader().ask(name);
  return std::nullopt;
  }

auto face_texture(swapchain_data_t const & data) noexcept -> VkDescriptorSet { return data.faces.set; }

auto record_face_uploads(swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index) noexcept -> void
  {
  try
    {
    face_atlas_t & atlas{data.faces};
    if(atlas.broken)
      return;
    auto faces{loader().take(uploads_per_frame)};
    if(faces.empty())
      return;
    if(atlas.set == VK_NULL_HANDLE and not make_atlas(data))
      {
      log("face atlas could not be made, the balls keep their colours");
      atlas.broken = true;
      return;
      }
    if(image_index >= atlas.staging_frames)
      return;

    device_data_t & device{*data.device};
    std::vector<VkBufferImageCopy> copies;
    std::vector<uint32_t> tiles;
    VkDeviceSize const base{tile_bytes * uploads_per_frame * image_index};
    for(auto & [name, rgba]: faces)
      {
      if(rgba.size() != tile_bytes)
        continue;
      auto const tile{tile_for(atlas, name, data.drawn_frames)};
      if(not tile)
        {
        // every tile drawn this frame - asked for again when next wanted
        loader().forget(name);
        continue;
        }
      if(not atlas.names[*tile].empty() and atlas.names[*tile] != name)
        loader().forget(atlas.names[*tile]);
      atlas.names[*tile] = name;
      atlas.ready[*tile] = false;
      atlas.used[*tile] = data.drawn_frames;
      VkDeviceSize const offset{base + tile_bytes * copies.size()};
      std::memcpy(static_cast<uint8_t *>(atlas.mapped) + offset, rgba.data(), rgba.size());
      copies.push_back(
        VkBufferImageCopy{
          .bufferOffset = offset,
          .bufferRowLength = 0u,
          .bufferImageHeight = 0u,
          .imageSubresource
          = VkImageSubresourceLayers{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0u, .baseArrayLayer = 0u, .layerCount = 1u},
          .imageOffset
          = VkOffset3D{.x = int32_t((*tile % faces_across) * face_side), .y = int32_t((*tile / faces_across) * face_side), .z = 0},
          .imageExtent = VkExtent3D{.width = face_side, .height = face_side, .depth = 1u}
        }
      );
      tiles.push_back(*tile);
      }
    if(copies.empty())
      return;

    VkImageSubresourceRange const whole{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0u,
      .levelCount = 1u,
      .baseArrayLayer = 0u,
      .layerCount = 1u
    };
    // the earlier frames' drawing reads the atlas; the copy waits for it, the drawing after waits for the copy
    VkImageMemoryBarrier const to_copy{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = atlas.fresh ? VkAccessFlags{0u} : VkAccessFlags{VK_ACCESS_SHADER_READ_BIT},
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = atlas.fresh ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = atlas.image,
      .subresourceRange = whole
    };
    device.CmdPipelineBarrier(
      frame.command_buffer,
      atlas.fresh ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      0u,
      0u,
      nullptr,
      0u,
      nullptr,
      1u,
      &to_copy
    );
    device.CmdCopyBufferToImage(
      frame.command_buffer,
      atlas.staging,
      atlas.image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      uint32_t(copies.size()),
      copies.data()
    );
    VkImageMemoryBarrier const to_read{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = atlas.image,
      .subresourceRange = whole
    };
    device.CmdPipelineBarrier(
      frame.command_buffer,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0u,
      0u,
      nullptr,
      0u,
      nullptr,
      1u,
      &to_read
    );
    atlas.fresh = false;
    // drawn from the next frame on - this one was laid out before the pixels came
    for(uint32_t const tile: tiles)
      atlas.ready[tile] = true;
    }
  catch(...)
    {
    data.faces.broken = true;
    }
  }

auto destroy_faces(swapchain_data_t & data) noexcept -> void
  {
  face_atlas_t & atlas{data.faces};
  if(data.device == nullptr)
    return;
  device_data_t & device{*data.device};
  if(atlas.sampler != VK_NULL_HANDLE)
    device.DestroySampler(device.device, atlas.sampler, nullptr);
  if(atlas.view != VK_NULL_HANDLE)
    device.DestroyImageView(device.device, atlas.view, nullptr);
  if(atlas.image != VK_NULL_HANDLE)
    device.DestroyImage(device.device, atlas.image, nullptr);
  if(atlas.memory != VK_NULL_HANDLE)
    device.FreeMemory(device.device, atlas.memory, nullptr);
  if(atlas.staging != VK_NULL_HANDLE)
    device.DestroyBuffer(device.device, atlas.staging, nullptr);
  if(atlas.staging_memory != VK_NULL_HANDLE)
    device.FreeMemory(device.device, atlas.staging_memory, nullptr);
  // the names go back to the loader, so a new swapchain reads its faces again - a loader already stopped
  // is not started for that
  if(loader_t * const running{loaders.peek()}; running != nullptr)
    for(std::string const & name: atlas.names)
      if(not name.empty())
        running->forget(name);
  atlas = {};
  }

auto stop_face_loader() noexcept -> void
  { loaders.stop(); }
  }  // namespace eht_overlay
