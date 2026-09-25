#include "vk_draw.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <array>
#include <chrono>
#include <cstdlib>

namespace eht_overlay
  {
namespace
  {
  constexpr float corner_margin{14.f};
  ///\brief present z wieksza liczba semaforow po prostu pomijamy - nie warto alokowac na sciezce klatki
  constexpr uint32_t max_wait_semaphores{16u};

  [[nodiscard]]
  auto env_flag(char const * name, bool fallback) noexcept -> bool
    {
    char const * const value{std::getenv(name)};
    if(value == nullptr or *value == '\0')
      return fallback;
    return *value != '0';
    }

  [[nodiscard]]
  auto stats_enabled() noexcept -> bool
    {
    static bool const enabled{env_flag("EHT_OVERLAY_STATS", true)};
    return enabled;
    }

  [[nodiscard]]
  auto scale_override() noexcept -> float
    {
    static float const scale{
      []() -> float
      {
        char const * const value{std::getenv("EHT_OVERLAY_SCALE")};
        if(value == nullptr or *value == '\0')
          return 0.f;
        return std::strtof(value, nullptr);
      }()
    };
    return scale;
    }

  [[nodiscard]]
  auto to_color(uint32_t rgb) noexcept -> ImVec4
    {
    return ImVec4{
      static_cast<float>((rgb >> 16u) & 0xffu) / 255.f,
      static_cast<float>((rgb >> 8u) & 0xffu) / 255.f,
      static_cast<float>(rgb & 0xffu) / 255.f,
      1.f
    };
    }

  [[nodiscard]]
  auto now_seconds() noexcept -> double
    { return std::chrono::duration<double>{std::chrono::steady_clock::now().time_since_epoch()}.count(); }

  ///\brief imgui nie moze wolac loadera, bo ten wpuscilby nas ponownie na gore lancucha warstw
  auto vulkan_loader(char const * name, void * user_data) -> PFN_vkVoidFunction
    {
    auto * const device{static_cast<device_data_t *>(user_data)};
    if(auto const from_device{device->next_gdpa(device->device, name)}; from_device != nullptr)
      return from_device;
    return device->instance->next_gipa(device->instance->instance, name);
    }

  [[nodiscard]]
  auto corner_position(overlay::corner_e corner, ImVec2 display) noexcept -> std::pair<ImVec2, ImVec2>
    {
    using enum overlay::corner_e;
    switch(corner)
      {
      case top_left:     return {ImVec2{corner_margin, corner_margin}, ImVec2{0.f, 0.f}};
      case top_right:    return {ImVec2{display.x - corner_margin, corner_margin}, ImVec2{1.f, 0.f}};
      case bottom_left:  return {ImVec2{corner_margin, display.y - corner_margin}, ImVec2{0.f, 1.f}};
      case bottom_right: return {ImVec2{display.x - corner_margin, display.y - corner_margin}, ImVec2{1.f, 1.f}};
      }
    return {ImVec2{corner_margin, corner_margin}, ImVec2{0.f, 0.f}};
    }

  [[nodiscard]]
  auto window_name(overlay::corner_e corner) noexcept -> char const *
    {
    using enum overlay::corner_e;
    switch(corner)
      {
      case top_left:     return "eht_top_left";
      case top_right:    return "eht_top_right";
      case bottom_left:  return "eht_bottom_left";
      case bottom_right: return "eht_bottom_right";
      }
    return "eht_unknown";
    }

  [[nodiscard]]
  auto block_visible(overlay::block_t const & block, uint64_t age_ms) noexcept -> bool
    { return not block.lines.empty() and (block.ttl_ms == 0u or age_ms <= block.ttl_ms); }

  auto build_ui(swapchain_data_t & data) -> void
    {
    auto const snapshot{ipc_client().snapshot()};
    uint64_t age_ms{};
    if(snapshot)
      age_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - snapshot->at).count()
      );

    constexpr ImGuiWindowFlags flags{
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize
      | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav
      | ImGuiWindowFlags_NoMove
    };

    ImVec2 const display{ImGui::GetIO().DisplaySize};

    for(auto const corner:
        {overlay::corner_e::top_left,
         overlay::corner_e::top_right,
         overlay::corner_e::bottom_left,
         overlay::corner_e::bottom_right})
      {
      bool const stats_here{stats_enabled() and corner == overlay::corner_e::top_right};

      bool anything{stats_here};
      if(snapshot and not anything)
        for(overlay::block_t const & block: snapshot->frame.blocks)
          anything = anything or (block.corner == corner and block_visible(block, age_ms));

      if(not anything)
        continue;

      auto const [position, pivot]{corner_position(corner, display)};
      ImGui::SetNextWindowBgAlpha(0.35f);
      ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);

      if(ImGui::Begin(window_name(corner), nullptr, flags))
        {
        if(stats_here)
          {
          ImGui::TextColored(
            to_color(0x9ad1ff),
            "EHT overlay  %.0f fps  frame %llu",
            double{data.fps},
            (unsigned long long)data.drawn_frames
          );
          auto & client{ipc_client()};
          if(client.connected())
            ImGui::TextColored(
              to_color(0x86d986), "elite_help_tool: connected, %llu frames", (unsigned long long)client.received()
            );
          else
            ImGui::TextColored(to_color(0xd9a34a), "elite_help_tool: waiting for connection");
          }

        if(snapshot)
          for(overlay::block_t const & block: snapshot->frame.blocks)
            {
            if(block.corner != corner or not block_visible(block, age_ms))
              continue;

            if(stats_here)
              ImGui::Separator();

            for(overlay::line_t const & line: block.lines)
              ImGui::TextColored(to_color(line.color), "%s", line.text.c_str());
            }
        }
      ImGui::End();
      }
    }

  auto destroy_frame(device_data_t & device, frame_resources_t & frame) -> void
    {
    if(frame.framebuffer != VK_NULL_HANDLE)
      device.DestroyFramebuffer(device.device, frame.framebuffer, nullptr);
    if(frame.view != VK_NULL_HANDLE)
      device.DestroyImageView(device.device, frame.view, nullptr);
    if(frame.fence != VK_NULL_HANDLE)
      device.DestroyFence(device.device, frame.fence, nullptr);
    if(frame.semaphore != VK_NULL_HANDLE)
      device.DestroySemaphore(device.device, frame.semaphore, nullptr);
    frame = {};
    }
  }  // namespace

auto imgui_assert_failed(char const * expression, char const * file, int line) -> void
  { log("imgui assert: {} at {}:{}", expression, file, line); }

auto destroy_resources(swapchain_data_t & data) -> void
  {
  if(data.device == nullptr)
    return;

  device_data_t & device{*data.device};

  if(data.imgui != nullptr)
    {
    ImGui::SetCurrentContext(data.imgui);
    ImGui_ImplVulkan_Shutdown();
    ImGui::DestroyContext(data.imgui);
    data.imgui = nullptr;
    }

  for(frame_resources_t & frame: data.frames)
    destroy_frame(device, frame);
  data.frames.clear();

  if(data.command_pool != VK_NULL_HANDLE)
    {
    device.DestroyCommandPool(device.device, data.command_pool, nullptr);
    data.command_pool = VK_NULL_HANDLE;
    }
  if(data.descriptor_pool != VK_NULL_HANDLE)
    {
    device.DestroyDescriptorPool(device.device, data.descriptor_pool, nullptr);
    data.descriptor_pool = VK_NULL_HANDLE;
    }
  if(data.render_pass != VK_NULL_HANDLE)
    {
    device.DestroyRenderPass(device.device, data.render_pass, nullptr);
    data.render_pass = VK_NULL_HANDLE;
    }

  data.ready = false;
  }

auto ensure_resources(swapchain_data_t & data, VkQueue queue) -> bool
  {
  if(data.ready)
    return true;

  device_data_t & device{*data.device};

  uint32_t const family{device.family_of(queue)};
  if(family == VK_QUEUE_FAMILY_IGNORED or family >= device.queue_families.size())
    {
    log("present queue of unknown family, overlay disabled for this swapchain");
    return false;
    }
  if((device.queue_families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0u)
    {
    log("present queue without graphics support, overlay disabled for this swapchain");
    return false;
    }
  data.queue_family = family;

  VkAttachmentDescription const attachment{
    .flags = 0u,
    .format = data.format,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    // obraz gry musi zostac nietkniety - dokladamy sie do niego, nie czyscimy go
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
  };
  VkAttachmentReference const reference{.attachment = 0u, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription const subpass{
    .flags = 0u,
    .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
    .inputAttachmentCount = 0u,
    .pInputAttachments = nullptr,
    .colorAttachmentCount = 1u,
    .pColorAttachments = &reference,
    .pResolveAttachments = nullptr,
    .pDepthStencilAttachment = nullptr,
    .preserveAttachmentCount = 0u,
    .pPreserveAttachments = nullptr
  };
  VkSubpassDependency const dependency{
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0u,
    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = 0u,
    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    .dependencyFlags = 0u
  };
  VkRenderPassCreateInfo const render_pass_info{
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0u,
    .attachmentCount = 1u,
    .pAttachments = &attachment,
    .subpassCount = 1u,
    .pSubpasses = &subpass,
    .dependencyCount = 1u,
    .pDependencies = &dependency
  };
  if(device.CreateRenderPass(device.device, &render_pass_info, nullptr, &data.render_pass) != VK_SUCCESS)
    {
    log("render pass creation failed");
    return false;
    }

  VkDescriptorPoolSize const pool_size{.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 16u};
  VkDescriptorPoolCreateInfo const pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
    .maxSets = 16u,
    .poolSizeCount = 1u,
    .pPoolSizes = &pool_size
  };
  if(device.CreateDescriptorPool(device.device, &pool_info, nullptr, &data.descriptor_pool) != VK_SUCCESS)
    {
    log("descriptor pool creation failed");
    destroy_resources(data);
    return false;
    }

  VkCommandPoolCreateInfo const command_pool_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = family
  };
  if(device.CreateCommandPool(device.device, &command_pool_info, nullptr, &data.command_pool) != VK_SUCCESS)
    {
    log("command pool creation failed");
    destroy_resources(data);
    return false;
    }

  data.frames.resize(data.images.size());

  for(size_t index{}; index != data.images.size(); ++index)
    {
    frame_resources_t & frame{data.frames[index]};

    VkCommandBufferAllocateInfo const allocate_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = data.command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1u
    };
    if(device.AllocateCommandBuffers(device.device, &allocate_info, &frame.command_buffer) != VK_SUCCESS)
      {
      log("command buffer allocation failed");
      destroy_resources(data);
      return false;
      }
    // loader wymaga aby kazdy nowy uchwyt dispatchable dostal od nas tablice dyspozycji
    if(device.set_device_loader_data != nullptr)
      device.set_device_loader_data(device.device, frame.command_buffer);

    VkImageViewCreateInfo const view_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .image = data.images[index],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = data.format,
      .components = {},
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0u,
        .levelCount = 1u,
        .baseArrayLayer = 0u,
        .layerCount = 1u
      }
    };
    if(device.CreateImageView(device.device, &view_info, nullptr, &frame.view) != VK_SUCCESS)
      {
      log("image view creation failed");
      destroy_resources(data);
      return false;
      }

    VkFramebufferCreateInfo const framebuffer_info{
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0u,
      .renderPass = data.render_pass,
      .attachmentCount = 1u,
      .pAttachments = &frame.view,
      .width = data.extent.width,
      .height = data.extent.height,
      .layers = 1u
    };
    if(device.CreateFramebuffer(device.device, &framebuffer_info, nullptr, &frame.framebuffer) != VK_SUCCESS)
      {
      log("framebuffer creation failed");
      destroy_resources(data);
      return false;
      }

    VkFenceCreateInfo const fence_info{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0u};
    VkSemaphoreCreateInfo const semaphore_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0u
    };
    if(
      device.CreateFence(device.device, &fence_info, nullptr, &frame.fence) != VK_SUCCESS
      or device.CreateSemaphore(device.device, &semaphore_info, nullptr, &frame.semaphore) != VK_SUCCESS
    )
      {
      log("synchronisation object creation failed");
      destroy_resources(data);
      return false;
      }
    }

  IMGUI_CHECKVERSION();
  data.imgui = ImGui::CreateContext();
  if(data.imgui == nullptr)
    {
    destroy_resources(data);
    return false;
    }
  ImGui::SetCurrentContext(data.imgui);

  ImGuiIO & io{ImGui::GetIO()};
  // overlay nigdy nie tworzy plikow w katalogu gry
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.DisplaySize = ImVec2{static_cast<float>(data.extent.width), static_cast<float>(data.extent.height)};
  io.FontGlobalScale
    = scale_override() > 0.f ? scale_override() : std::max(1.f, static_cast<float>(data.extent.height) / 1080.f);

  ImGui::StyleColorsDark();

  if(not ImGui_ImplVulkan_LoadFunctions(device.instance->api_version, &vulkan_loader, static_cast<void *>(data.device)))
    {
    log("imgui could not resolve vulkan functions");
    destroy_resources(data);
    return false;
    }

  ImGui_ImplVulkan_InitInfo init_info{};
  init_info.ApiVersion = device.instance->api_version;
  init_info.Instance = device.instance->instance;
  init_info.PhysicalDevice = device.physical_device;
  init_info.Device = device.device;
  init_info.QueueFamily = family;
  init_info.Queue = queue;
  init_info.DescriptorPool = data.descriptor_pool;
  init_info.RenderPass = data.render_pass;
  init_info.MinImageCount = static_cast<uint32_t>(std::max<size_t>(2u, data.images.size()));
  init_info.ImageCount = static_cast<uint32_t>(data.images.size());
  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  if(not ImGui_ImplVulkan_Init(&init_info))
    {
    log("imgui vulkan backend init failed");
    destroy_resources(data);
    return false;
    }

  data.last_draw_seconds = now_seconds();
  data.ready = true;
  log("overlay ready for swapchain {}x{}, {} images", data.extent.width, data.extent.height, data.images.size());
  return true;
  }

auto draw_overlay(
  swapchain_data_t & data, VkQueue queue, uint32_t image_index, VkSemaphore const * wait, uint32_t wait_count
) noexcept -> VkSemaphore
  {
  if(data.broken or data.device == nullptr)
    return VK_NULL_HANDLE;

  if(wait_count > max_wait_semaphores)
    return VK_NULL_HANDLE;

  try
    {
    if(not data.ready and not ensure_resources(data, queue))
      {
      // jedna nieudana proba wystarczy - dalej gra ma isc swoim torem bez nas
      data.broken = true;
      return VK_NULL_HANDLE;
      }

    if(image_index >= data.frames.size())
      return VK_NULL_HANDLE;

    device_data_t & device{*data.device};
    frame_resources_t & frame{data.frames[image_index]};

    if(frame.submitted)
      {
      if(device.WaitForFences(device.device, 1u, &frame.fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        {
        data.broken = true;
        return VK_NULL_HANDLE;
        }
      device.ResetFences(device.device, 1u, &frame.fence);
      frame.submitted = false;
      }

    auto const now{now_seconds()};
    auto const delta{std::max(1.0 / 10000.0, now - data.last_draw_seconds)};
    data.last_draw_seconds = now;
    data.fps = static_cast<float>(0.9 * double{data.fps} + 0.1 / delta);
    ++data.drawn_frames;

    ImGui::SetCurrentContext(data.imgui);
    ImGuiIO & io{ImGui::GetIO()};
    io.DisplaySize = ImVec2{static_cast<float>(data.extent.width), static_cast<float>(data.extent.height)};
    io.DeltaTime = static_cast<float>(delta);

    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    build_ui(data);
    ImGui::Render();

    ImDrawData * const draw_data{ImGui::GetDrawData()};
    if(draw_data == nullptr or draw_data->CmdListsCount == 0)
      return VK_NULL_HANDLE;

    device.ResetCommandBuffer(frame.command_buffer, 0u);

    VkCommandBufferBeginInfo const begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr
    };
    if(device.BeginCommandBuffer(frame.command_buffer, &begin_info) != VK_SUCCESS)
      {
      data.broken = true;
      return VK_NULL_HANDLE;
      }

    VkRenderPassBeginInfo const pass_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = nullptr,
      .renderPass = data.render_pass,
      .framebuffer = frame.framebuffer,
      .renderArea = VkRect2D{.offset = {0, 0}, .extent = data.extent},
      .clearValueCount = 0u,
      .pClearValues = nullptr
    };
    device.CmdBeginRenderPass(frame.command_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(draw_data, frame.command_buffer);
    device.CmdEndRenderPass(frame.command_buffer);

    if(device.EndCommandBuffer(frame.command_buffer) != VK_SUCCESS)
      {
      data.broken = true;
      return VK_NULL_HANDLE;
      }

    std::array<VkPipelineStageFlags, max_wait_semaphores> stages{};
    stages.fill(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    VkSubmitInfo const submit{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreCount = wait_count,
      .pWaitSemaphores = wait_count != 0u ? wait : nullptr,
      .pWaitDstStageMask = wait_count != 0u ? stages.data() : nullptr,
      .commandBufferCount = 1u,
      .pCommandBuffers = &frame.command_buffer,
      .signalSemaphoreCount = 1u,
      .pSignalSemaphores = &frame.semaphore
    };
    if(device.QueueSubmit(queue, 1u, &submit, frame.fence) != VK_SUCCESS)
      {
      data.broken = true;
      return VK_NULL_HANDLE;
      }

    frame.submitted = true;
    return frame.semaphore;
    }
  catch(...)
    {
    data.broken = true;
    return VK_NULL_HANDLE;
    }
  }
  }  // namespace eht_overlay
