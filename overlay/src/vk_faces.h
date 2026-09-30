#pragma once

#include "vk_dispatch.h"

#include <cstdint>
#include <optional>
#include <string>

namespace eht_overlay
  {
///\brief the side of a face in the atlas - the tool makes them this size, another is not taken
inline constexpr uint32_t face_side{128u};
///\brief the atlas is faces_across x faces_across faces
inline constexpr uint32_t faces_across{8u};

///\brief the atlas tile the face of that name is in, once its pixels are there; asks for it to be read
/// otherwise. A tile given out is kept for this frame - a tile is only reused for another face when no
/// frame drew it last
[[nodiscard]]
auto face_tile(swapchain_data_t & data, std::string const & name) -> std::optional<uint32_t>;

///\brief the atlas as ImGui knows a texture, for the draw commands of the textured balls
[[nodiscard]]
auto face_texture(swapchain_data_t const & data) noexcept -> VkDescriptorSet;

///\brief copies the faces read since the last frame into the atlas, before the render pass
///\detail the files are read on a thread of their own; the frame only copies what is ready, a few at a
/// time, through its own part of a staging buffer - its fence has signalled, so that part is free
auto record_face_uploads(swapchain_data_t & data, frame_resources_t & frame, uint32_t image_index) noexcept -> void;

///\brief frees the atlas; the device is idle by then, and its descriptor set goes with ImGui's pool
auto destroy_faces(swapchain_data_t & data) noexcept -> void;
  }  // namespace eht_overlay
