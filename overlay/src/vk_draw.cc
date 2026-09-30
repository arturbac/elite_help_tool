#include "vk_draw.h"
#include "vk_capture.h"
#include "vk_keyboard.h"
#include "vk_faces.h"
#include "overlay_font.h"
#include "ground_shaders.h"
#include "sphere_shaders.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <overlay_emblems.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace eht_overlay
  {
namespace
  {
  ///\brief a present with more semaphores is simply skipped - not worth allocating on the frame path
  constexpr uint32_t max_wait_semaphores{16u};
  ///\brief we never wait for ever - a hung overlay has no right to hang the game
  constexpr uint64_t fence_timeout_ns{1000000000ull};
  ///\brief above this the drawing costs a frame, which is worth knowing about
  constexpr double slow_draw_seconds{0.004};

  ///\brief the layout the tool sent with its newest frame, or the defaults before the first one
  ///\detail the layer keeps no settings of its own - they live in the tool's settings file, and a saved
  /// change arrives here with the next frame
  [[nodiscard]]
  auto current_layout() -> overlay::layout_t
    {
    auto const snapshot{ipc_client().snapshot()};
    return snapshot ? snapshot->frame.layout : overlay::layout_t{};
    }

  ///\brief width of the screen in the middle, which is the only one the player looks at
  ///
  /// The layer sees one surface spanning every monitor and cannot see where one ends - but it does not
  /// have to. The game draws its own interface on the middle screen at the ordinary shape of a monitor
  /// however wide the whole surface is, so that screen's width follows from its height: 2160 tall makes
  /// it 3840 across, whether the surface beside it is 8000 or twice that. Only a middle screen of an
  /// unusual shape needs telling, which the layout's centre_width does.
  [[nodiscard]]
  auto centre_screen_width(ImVec2 display, overlay::layout_t const & layout) noexcept -> float
    {
    if(layout.centre_width > 0.f and layout.centre_width <= display.x)
      return layout.centre_width;

    constexpr float ordinary_shape{16.f / 9.f};
    return std::min(display.y * ordinary_shape, display.x);
    }

  ///\brief how much smaller the small text is than the ordinary text
  [[nodiscard]]
  auto small_text_ratio(overlay::layout_t const & layout) noexcept -> float
    { return std::clamp(layout.small_text, 0.4f, 1.f); }

  ///\brief the size the text is rasterised at, as a multiple of 13 pixels
  ///\detail the goal is a constant fraction of screen height, so the text reads the same on 1080 and on
  /// 4k - unless the layout names a scale of its own
  [[nodiscard]]
  auto text_scale(overlay::layout_t const & layout, VkExtent2D extent) noexcept -> float
    { return layout.scale > 0.f ? layout.scale : std::max(1.f, static_cast<float>(extent.height) / 780.f); }

  ///\brief the line the head-up readouts stand on, which they grow upwards from
  ///\detail the game's own weapon panels begin about a third of the way down, and the space above
  /// them is empty - so the readouts are hung from a line just above where those panels start, and
  /// rise as far as they need. Anchoring them by the bottom is what keeps them there whether they
  /// carry two lines or eight
  [[nodiscard]]
  auto hud_bottom(ImVec2 display, overlay::layout_t const & layout) noexcept -> float
    { return display.y * std::clamp(layout.hud_bottom, 0.f, 1.f); }

  ///\brief half the corridor left clear down the middle, where the fighting happens
  ///\detail measured off the game's interface rather than chosen: SECONDARY and PRIMARY sit about a
  /// quarter of the middle screen's width either side of its centre, so the readouts stand over them
  [[nodiscard]]
  auto hud_gap(ImVec2 display, overlay::layout_t const & layout) noexcept -> float
    { return centre_screen_width(display, layout) * std::clamp(layout.hud_gap, 0.f, 0.5f); }

  ///\brief width of the band at the screen edge we are allowed to draw in
  [[nodiscard]]
  auto side_band_width(ImVec2 display, overlay::layout_t const & layout) noexcept -> float
    {
    if(layout.side_width > 0.f)
      return layout.side_width;

    // The band is not a guess: it is exactly the screen beside the middle one, edge to edge, and
    // every pixel of it lies outside where the player looks
    if(float const centre{centre_screen_width(display, layout)}; centre < display.x)
      return std::max(240.f, (display.x - centre) * 0.5f);

    // on a single screen the middle one is the whole surface, so the band goes back to a fifth of it
    return std::clamp(display.x * 0.2f, 240.f, 1600.f);
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

  ///\brief imgui must not call the loader, which would let us back in at the top of the layer chain
  auto vulkan_loader(char const * name, void * user_data) -> PFN_vkVoidFunction
    {
    auto * const device{static_cast<device_data_t *>(user_data)};
    if(auto const from_device{device->next_gdpa(device->device, name)}; from_device != nullptr)
      return from_device;
    return device->instance->next_gipa(device->instance->instance, name);
    }

  [[nodiscard]]
  auto corner_position(overlay::corner_e corner, ImVec2 display, overlay::layout_t const & layout) noexcept
    -> std::pair<ImVec2, ImVec2>
    {
    float const corner_margin{layout.corner_margin};
    using enum overlay::corner_e;
    switch(corner)
      {
      case top_left:     return {ImVec2{corner_margin, corner_margin}, ImVec2{0.f, 0.f}};
      case top_right:    return {ImVec2{display.x - corner_margin, corner_margin}, ImVec2{1.f, 0.f}};
      case bottom_left:  return {ImVec2{corner_margin, display.y - corner_margin}, ImVec2{0.f, 1.f}};
      case bottom_right: return {ImVec2{display.x - corner_margin, display.y - corner_margin}, ImVec2{1.f, 1.f}};

      // both stand on the same line and grow upwards and outwards from it, so the corridor between
      // them keeps its width and the weapon panels below keep their room however much text arrives
      case centre_top_left:
        return {ImVec2{display.x * 0.5f - hud_gap(display, layout), hud_bottom(display, layout)}, ImVec2{1.f, 1.f}};
      case centre_top_right:
        return {ImVec2{display.x * 0.5f + hud_gap(display, layout), hud_bottom(display, layout)}, ImVec2{0.f, 1.f}};
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
      case centre_top_left:  return "eht_centre_top_left";
      case centre_top_right: return "eht_centre_top_right";
      }
    return "eht_unknown";
    }

  ///\brief TextColored does not wrap, and in a side band wrapping is essential
  template<typename... args_t>
  auto coloured_text(uint32_t rgb, char const * format, args_t... args) -> void
    {
    ImGui::PushStyleColor(ImGuiCol_Text, to_color(rgb));
    ImGui::TextWrapped(format, args...);
    ImGui::PopStyleColor();
    }

  ///\brief the shape standing in front of a name, and again at the head of that name's line
  ///\detail three independent factions share one colour, so the shape is what tells their lines apart
  auto draw_marker(ImDrawList * draw, ImVec2 centre, float radius, ImU32 colour, overlay::marker_e marker) -> void
    {
    using enum overlay::marker_e;
    switch(marker)
      {
      case circle: draw->AddCircleFilled(centre, radius, colour, 12); break;

      case diamond:
        {
        ImVec2 const points[]{
          ImVec2{centre.x, centre.y - radius},
          ImVec2{centre.x + radius, centre.y},
          ImVec2{centre.x, centre.y + radius},
          ImVec2{centre.x - radius, centre.y}
        };
        draw->AddConvexPolyFilled(points, 4, colour);
        }
        break;

      case triangle:
        draw->AddTriangleFilled(
          ImVec2{centre.x, centre.y - radius},
          ImVec2{centre.x + radius, centre.y + radius},
          ImVec2{centre.x - radius, centre.y + radius},
          colour
        );
        break;

      case square:
        draw->AddRectFilled(
          ImVec2{centre.x - radius, centre.y - radius}, ImVec2{centre.x + radius, centre.y + radius}, colour
        );
        break;

      case cross:
        draw->AddLine(
          ImVec2{centre.x - radius, centre.y - radius}, ImVec2{centre.x + radius, centre.y + radius}, colour, radius * 0.6f
        );
        draw->AddLine(
          ImVec2{centre.x - radius, centre.y + radius}, ImVec2{centre.x + radius, centre.y - radius}, colour, radius * 0.6f
        );
        break;

      case none: break;
      }
    }

  [[nodiscard]]
  auto emblem_mask(overlay::emblem_e which) noexcept -> emblems::mask_t const *
    {
    using enum overlay::emblem_e;
    switch(which)
      {
      case federation: return &emblems::federation;
      case empire:     return &emblems::empire;
      case alliance:   return &emblems::alliance;
      case none:       break;
      }
    return nullptr;
    }

  ///\brief puts the emblems into the font atlas instead of giving them textures of their own
  ///\detail imgui hands out space in the atlas it already owns, so the emblems cost no VkImage, no
  /// sampler and no descriptor set, and nothing has to be freed when the swapchain is rebuilt.
  /// Must run before the atlas is built, which is why it sits next to the font
  auto reserve_emblems(swapchain_data_t & data) -> void
    {
    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};
    for(auto const which: {overlay::emblem_e::federation, overlay::emblem_e::empire, overlay::emblem_e::alliance})
      if(emblems::mask_t const * const mask{emblem_mask(which)}; mask != nullptr)
        data.emblem_rects[static_cast<size_t>(which)] = atlas.AddCustomRectRegular(mask->width, mask->height);
    }

  ///\brief writes the coverage masks into the atlas pixels, white throughout so the tint decides the colour
  auto blit_emblems(swapchain_data_t & data) -> void
    {
    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};

    unsigned char * pixels{};
    int width{};
    int height{};
    // this builds the atlas, which is what fixes where the reserved rectangles landed
    atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
    if(pixels == nullptr)
      return;

    constexpr auto digit = [](char c) noexcept -> uint32_t
    { return static_cast<uint32_t>(c <= '9' ? c - '0' : (c | 0x20) - 'a' + 10); };

    for(auto const which: {overlay::emblem_e::federation, overlay::emblem_e::empire, overlay::emblem_e::alliance})
      {
      int const index{data.emblem_rects[static_cast<size_t>(which)]};
      emblems::mask_t const * const mask{emblem_mask(which)};
      if(index < 0 or mask == nullptr)
        continue;

      ImFontAtlasCustomRect const * const rect{atlas.GetCustomRectByIndex(index)};
      auto * const target{reinterpret_cast<uint32_t *>(pixels)};

      for(int y{}; y != mask->height; ++y)
        for(int x{}; x != mask->width; ++x)
          {
          char const * const at{mask->hex + 2 * (y * mask->width + x)};
          uint32_t const coverage{digit(at[0]) << 4u | digit(at[1])};
          target[(rect->Y + y) * width + (rect->X + x)] = IM_COL32(255, 255, 255, coverage);
          }
      }
    }

  ///\brief a picture read from a binary PPM, as the tool writes them
  struct ppm_t
    {
    int width{};
    int height{};
    std::vector<uint8_t> rgb;
    };

  ///\brief the largest picture taken into the atlas - the tool sends thumbnails, this only guards the atlas
  constexpr int max_picture_side{640};
  ///\brief the most pictures the atlas takes at once
  constexpr size_t max_pictures{16u};

  [[nodiscard]]
  auto read_ppm(std::string const & path) -> std::optional<ppm_t>
    {
    std::ifstream in{path, std::ios::binary};
    if(not in)
      return std::nullopt;
    std::string magic;
    int width{};
    int height{};
    int maximum{};
    in >> magic >> width >> height >> maximum;
    in.get();
    if(
      not in or magic != "P6" or maximum != 255 or width <= 0 or height <= 0 or width > max_picture_side
      or height > max_picture_side
    )
      return std::nullopt;
    ppm_t picture{.width = width, .height = height, .rgb = std::vector<uint8_t>(size_t(width) * size_t(height) * 3u)};
    in.read(reinterpret_cast<char *>(picture.rgb.data()), std::streamsize(picture.rgb.size()));
    if(not in)
      return std::nullopt;
    return picture;
    }

  ///\brief the pictures the tool wants drawn now, each once, in the order it sends them
  [[nodiscard]]
  auto wanted_pictures() -> std::vector<std::string>
    {
    std::vector<std::string> paths;
    auto const snapshot{ipc_client().snapshot()};
    if(not snapshot)
      return paths;
    for(overlay::block_t const & block: snapshot->frame.blocks)
      for(overlay::picture_t const & picture: block.pictures)
        if(paths.size() < max_pictures and std::ranges::find(paths, picture.path) == paths.end())
          paths.push_back(picture.path);
    return paths;
    }

  ///\brief the pictures go into the font atlas like the emblems, and for the same reason: no textures of
  /// our own to create, bind and free. Reserves their room and hands back the pixels to write afterwards
  [[nodiscard]]
  auto reserve_pictures(swapchain_data_t & data) -> std::vector<std::optional<ppm_t>>
    {
    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};
    std::vector<std::optional<ppm_t>> pixels;
    data.picture_rects.assign(data.picture_paths.size(), -1);
    for(size_t ix{}; ix != data.picture_paths.size(); ++ix)
      {
      pixels.push_back(read_ppm(data.picture_paths[ix]));
      if(pixels.back())
        data.picture_rects[ix] = atlas.AddCustomRectRegular(pixels.back()->width, pixels.back()->height);
      else
        log("picture {} could not be read", data.picture_paths[ix]);
      }
    return pixels;
    }

  auto blit_pictures(swapchain_data_t const & data, std::vector<std::optional<ppm_t>> const & pictures) -> void
    {
    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};
    unsigned char * pixels{};
    int width{};
    int height{};
    atlas.GetTexDataAsRGBA32(&pixels, &width, &height);
    if(pixels == nullptr)
      return;
    auto * const target{reinterpret_cast<uint32_t *>(pixels)};
    for(size_t ix{}; ix != pictures.size(); ++ix)
      {
      if(not pictures[ix] or data.picture_rects[ix] < 0)
        continue;
      ppm_t const & picture{*pictures[ix]};
      ImFontAtlasCustomRect const * const rect{atlas.GetCustomRectByIndex(data.picture_rects[ix])};
      for(int y{}; y != picture.height; ++y)
        for(int x{}; x != picture.width; ++x)
          {
          uint8_t const * const rgb{picture.rgb.data() + 3u * (size_t(y) * size_t(picture.width) + size_t(x))};
          target[(rect->Y + y) * width + (rect->X + x)] = IM_COL32(rgb[0], rgb[1], rgb[2], 255);
          }
      }
    }

  ///\brief the pictures of a block in a grid as wide as the band
  auto draw_pictures(swapchain_data_t const & data, overlay::block_t const & block, float available) -> void
    {
    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};
    uint32_t const columns{std::clamp(block.picture_columns, 1u, 8u)};
    float const gap{ImGui::GetStyle().ItemSpacing.x};
    float const cell{(available - gap * float(columns - 1u)) / float(columns)};
    if(cell <= 0.f)
      return;

    ImDrawList * const draw{ImGui::GetWindowDrawList()};
    uint32_t column{};
    float row_height{};
    ImVec2 row_origin{ImGui::GetCursorScreenPos()};
    for(overlay::picture_t const & picture: block.pictures)
      {
      auto const it{std::ranges::find(data.picture_paths, picture.path)};
      if(it == data.picture_paths.end())
        continue;
      int const index{data.picture_rects[size_t(it - data.picture_paths.begin())]};
      if(index < 0)
        continue;
      ImFontAtlasCustomRect const * const rect{atlas.GetCustomRectByIndex(index)};
      if(rect->Width == 0)
        continue;

      ImVec2 uv_min{};
      ImVec2 uv_max{};
      atlas.CalcCustomRectUV(rect, &uv_min, &uv_max);
      float const height{cell * float(rect->Height) / float(rect->Width)};
      ImVec2 const at{row_origin.x + float(column) * (cell + gap), row_origin.y};
      draw->AddImage(atlas.TexID, at, ImVec2{at.x + cell, at.y + height}, uv_min, uv_max);
      row_height = std::max(row_height, height);

      if(++column == columns)
        {
        ImGui::Dummy(ImVec2{available, row_height});
        row_origin = ImGui::GetCursorScreenPos();
        column = 0u;
        row_height = 0.f;
        }
      }
    if(column != 0u)
      ImGui::Dummy(ImVec2{available, row_height});
    }

  ///\brief draws one emblem at the cursor, and says how wide it turned out
  [[nodiscard]]
  auto draw_emblem(swapchain_data_t const & data, overlay::emblem_e which, ImVec2 at, float height, ImU32 colour)
    -> float
    {
    int const index{data.emblem_rects[static_cast<size_t>(which)]};
    if(which == overlay::emblem_e::none or index < 0)
      return 0.f;

    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};
    ImFontAtlasCustomRect const * const rect{atlas.GetCustomRectByIndex(index)};
    if(rect->Height == 0)
      return 0.f;

    ImVec2 uv_min{};
    ImVec2 uv_max{};
    atlas.CalcCustomRectUV(rect, &uv_min, &uv_max);

    // the emblems differ in proportion - the Empire's is nearly twice as wide as it is tall - so
    // the height is what is fixed and the width follows from it
    float const width{height * static_cast<float>(rect->Width) / static_cast<float>(rect->Height)};
    ImGui::GetWindowDrawList()
      ->AddImage(atlas.TexID, at, ImVec2{at.x + width, at.y + height}, uv_min, uv_max, colour);
    return width;
    }

  ///\brief which way the faction went at the last recalculation
  auto draw_trend(ImDrawList * draw, ImVec2 centre, float radius, ImU32 colour, overlay::trend_e trend) -> void
    {
    using enum overlay::trend_e;
    switch(trend)
      {
      case up:
        draw->AddTriangleFilled(
          ImVec2{centre.x, centre.y - radius},
          ImVec2{centre.x + radius, centre.y + radius * 0.7f},
          ImVec2{centre.x - radius, centre.y + radius * 0.7f},
          colour
        );
        break;

      case down:
        draw->AddTriangleFilled(
          ImVec2{centre.x, centre.y + radius},
          ImVec2{centre.x - radius, centre.y - radius * 0.7f},
          ImVec2{centre.x + radius, centre.y - radius * 0.7f},
          colour
        );
        break;

      case flat:
        draw->AddLine(
          ImVec2{centre.x - radius, centre.y},
          ImVec2{centre.x + radius, centre.y},
          colour,
          std::max(1.f, radius * 0.45f)
        );
        break;

      case unknown: break;
      }
    }

  ///\brief where the text of the window being drawn wraps, in the window's own coordinates
  ///\detail the windows size themselves to their content, so what is left of the current width is nothing
  /// once the widest line is reached - a mark measured against it would go to a line of its own even
  /// with the whole band free beside it
  thread_local float wrap_right{};

  [[nodiscard]]
  auto room_left() -> float
    { return wrap_right - ImGui::GetCursorPosX(); }

  ///\brief the trend mark and whatever follows it, both optional
  auto draw_trailing(overlay::line_t const & line, float box, float spacing, ImU32 colour) -> void
    {
    if(line.trend != overlay::trend_e::unknown)
      {
      ImGui::SameLine(0.f, spacing);
      // a mark that does not fit beside the value would be wrapped to a line of its own, where it
      // would stand next to nothing
      if(room_left() < box)
        ImGui::NewLine();

      ImVec2 const at{ImGui::GetCursorScreenPos()};
      ImGui::Dummy(ImVec2{box * 0.7f, box});
      draw_trend(
        ImGui::GetWindowDrawList(),
        ImVec2{at.x + box * 0.35f, at.y + box * 0.5f},
        box * 0.28f,
        colour,
        line.trend
      );
      }

    if(not line.suffix.empty())
      {
      ImGui::SameLine(0.f, spacing);

      // What is left of the line after the value is usually a few pixels, and text wrapped into a
      // few pixels comes out one letter per row. Starting the states on a line of their own costs a
      // row and reads; squeezing them into the remainder costs a column of single letters
      if(room_left() < ImGui::CalcTextSize(line.suffix.c_str()).x)
        ImGui::NewLine();

      coloured_text(line.color, "%s", line.suffix.c_str());
      }
    }

  ///\brief a line of text, preceded by its marker and its emblem when it carries them
  ///\brief a small square in the given colours cut along the diagonal, a dot in its corner when asked
  auto draw_swatch(ImDrawList * draw, ImVec2 corner, float side, std::vector<uint32_t> const & colours, bool dot) -> void
    {
    ImVec2 const far{corner.x + side, corner.y + side};
    draw->PushClipRect(corner, far, true);
    float const band{2.f * side / static_cast<float>(colours.size())};
    for(size_t i{}; i != colours.size(); ++i)
      {
      float const from{corner.x + band * static_cast<float>(i)};
      float const to{corner.x + band * static_cast<float>(i + 1u)};
      std::array<ImVec2, 4> const shape{
        ImVec2{from, corner.y}, ImVec2{to, corner.y}, ImVec2{to - side, far.y}, ImVec2{from - side, far.y}
      };
      draw->AddConvexPolyFilled(shape.data(), static_cast<int>(shape.size()), ImGui::GetColorU32(to_color(colours[i])));
      }
    draw->PopClipRect();
    if(dot)
      draw->AddCircleFilled(ImVec2{far.x - side * 0.25f, far.y - side * 0.25f}, side * 0.2f, IM_COL32(255, 255, 255, 230));
    }

  ///\brief an arrow round the centre, turned clockwise from straight up - a head and a stem, each convex
  auto draw_pointer(ImDrawList * draw, ImVec2 centre, float radius, float degrees, ImU32 colour) -> void
    {
    float const angle{degrees * std::numbers::pi_v<float> / 180.f};
    float const c{std::cos(angle)};
    float const s{std::sin(angle)};
    // the shape drawn pointing up, in shares of the radius; screen y grows downwards, so this turn is clockwise
    auto const at = [&](float x, float y) { return ImVec2{centre.x + radius * (x * c - y * s), centre.y + radius * (x * s + y * c)}; };
    draw->AddTriangleFilled(at(0.f, -1.f), at(0.8f, 0.05f), at(-0.8f, 0.05f), colour);
    ImVec2 const stem[]{at(-0.28f, 0.f), at(0.28f, 0.f), at(0.28f, 1.f), at(-0.28f, 1.f)};
    draw->AddConvexPolyFilled(stem, 4, colour);
    }

  auto draw_line(swapchain_data_t const & data, overlay::line_t const & line) -> void
    {
    float const box{ImGui::GetFontSize()};

    // the way to go stands first, in a square slot of its own - it is what the eye looks for on the line
    if(line.pointer)
      {
      ImVec2 const at{ImGui::GetCursorScreenPos()};
      draw_pointer(
        ImGui::GetWindowDrawList(),
        ImVec2{at.x + box * 0.5f, at.y + box * 0.5f},
        box * 0.45f,
        *line.pointer,
        ImGui::GetColorU32(to_color(line.color))
      );
      ImGui::Dummy(ImVec2{box, box});
      ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x * 0.5f);
      }

    // the economies' square stands before everything else on the line, in a slot of its own, set in by
    // half its width so the commodities read as a list under their type's heading
    if(not line.swatch.empty() or line.swatch_space)
      {
      ImVec2 const at{ImGui::GetCursorScreenPos()};
      float const side{box * 0.7f};
      float const indent{side * 0.5f};
      if(not line.swatch.empty())
        draw_swatch(
          ImGui::GetWindowDrawList(), ImVec2{at.x + indent, at.y + (box - side) * 0.5f}, side, line.swatch, line.swatch_dot
        );
      ImGui::Dummy(ImVec2{indent + side, box});
      ImGui::SameLine(0.f, ImGui::GetStyle().ItemSpacing.x * 0.5f);
      }

    if(line.marker == overlay::marker_e::none and not line.emblem_column and line.emblem == overlay::emblem_e::none
       and not line.spans.empty())
      {
      // the text as one piece of room, painted part by part - each part where the text before it ends
      ImVec2 const origin{ImGui::GetCursorScreenPos()};
      ImGui::Dummy(ImGui::CalcTextSize(line.text.c_str()));
      ImDrawList * const draw{ImGui::GetWindowDrawList()};
      char const * const text{line.text.c_str()};
      size_t const size{line.text.size()};
      auto const paint = [&](size_t from, size_t to, uint32_t rgb)
      {
        if(from >= to)
          return;
        float const x{ImGui::CalcTextSize(text, text + from).x};
        draw->AddText(ImVec2{origin.x + x, origin.y}, ImGui::GetColorU32(to_color(rgb)), text + from, text + to);
      };
      size_t at{};
      for(overlay::span_t const & span: line.spans)
        {
        size_t const from{std::min<size_t>(span.from, size)};
        size_t const to{std::min<size_t>(size_t{span.from} + span.length, size)};
        if(from < at)
          continue;
        paint(at, from, line.color);
        paint(from, to, span.color);
        at = to;
        }
      paint(at, size, line.color);
      draw_trailing(line, box, ImGui::GetStyle().ItemSpacing.x * 0.5f, ImGui::GetColorU32(to_color(line.color)));
      return;
      }

    if(line.marker == overlay::marker_e::none and not line.emblem_column and line.emblem == overlay::emblem_e::none)
      {
      coloured_text(line.color, "%s", line.text.c_str());
      draw_trailing(line, box, ImGui::GetStyle().ItemSpacing.x * 0.5f, ImGui::GetColorU32(to_color(line.color)));
      return;
      }

    float const spacing{ImGui::GetStyle().ItemSpacing.x * 0.5f};
    ImU32 const colour{ImGui::GetColorU32(to_color(line.color))};

    // Every line that carries a marker reserves the same emblem slot, whether it has an emblem or
    // not. The emblems differ in width and independents have none at all, so without a fixed slot
    // the names would start at a different place on every line and stop reading as a column.
    constexpr float widest_emblem{
      std::max({
        static_cast<float>(emblems::federation.width) / static_cast<float>(emblems::federation.height),
        static_cast<float>(emblems::empire.width) / static_cast<float>(emblems::empire.height),
        static_cast<float>(emblems::alliance.width) / static_cast<float>(emblems::alliance.height)
      })
    };

    emblems::mask_t const * const mask{emblem_mask(line.emblem)};
    float const emblem_height{box * 0.85f};
    float const emblem_slot{
      line.marker != overlay::marker_e::none or line.emblem_column ? emblem_height * widest_emblem : 0.f
    };
    float const emblem_width{
      mask != nullptr ? emblem_height * static_cast<float>(mask->width) / static_cast<float>(mask->height) : 0.f
    };

    float const prefix{
      (line.marker != overlay::marker_e::none ? box : 0.f) + (emblem_slot > 0.f ? emblem_slot + spacing : 0.f)
    };

    ImVec2 const origin{ImGui::GetCursorScreenPos()};
    ImGui::Dummy(ImVec2{prefix, box});
    ImGui::SameLine(0.f, spacing);

    float cursor{origin.x};
    if(line.marker != overlay::marker_e::none)
      {
      draw_marker(
        ImGui::GetWindowDrawList(),
        ImVec2{cursor + box * 0.5f, origin.y + box * 0.5f},
        box * 0.28f,
        colour,
        line.marker
      );
      cursor += box;
      }

    if(emblem_slot > 0.f)
      {
      // centred in its slot, because the emblems are of different widths and a left edge shared by
      // a wide one and a narrow one looks like a mistake
      if(emblem_width > 0.f)
        {
        ImVec2 const at{cursor + (emblem_slot - emblem_width) * 0.5f, origin.y + (box - emblem_height) * 0.5f};
        [[maybe_unused]] float const drawn{draw_emblem(data, line.emblem, at, emblem_height, colour)};
        }
      cursor += emblem_slot + spacing;
      }

    coloured_text(line.color, "%s", line.text.c_str());
    draw_trailing(line, box, spacing, colour);
    }

  ///\brief draws a chart out of numbers the tool has already scaled to 0..1
  ///\detail no logarithm and no units here - whatever axis the tool chose, the layer only stretches
  /// the numbers over the rectangle it was given
  auto draw_chart(overlay::chart_t const & chart, float width) -> void
    {
    if(chart.series.empty() or width <= 0.f)
      return;

    // the font is rasterised at 13 * scale, so its size is the way back to the scale the rest uses
    float const scale{ImGui::GetFontSize() / 13.f};
    float const height{static_cast<float>(chart.height) * scale};

    if(not chart.caption.empty())
      coloured_text(0x9a9a9au, "%s", chart.caption.c_str());

    ImDrawList * const draw{ImGui::GetWindowDrawList()};
    ImVec2 const origin{ImGui::GetCursorScreenPos()};
    ImGui::Dummy(ImVec2{width, height});

    ImVec2 const far_corner{origin.x + width, origin.y + height};
    draw->AddRectFilled(origin, far_corner, IM_COL32(0, 0, 0, 70));

    for(overlay::grid_line_t const & guide: chart.grid)
      {
      float const y{far_corner.y - std::clamp(guide.y, 0.f, 1.f) * height};
      draw->AddLine(ImVec2{origin.x, y}, ImVec2{far_corner.x, y}, IM_COL32(255, 255, 255, 38));
      }

    // the buffer keeps its capacity between frames; this sits on the game's frame path, where an
    // allocation per series per frame would be paid for by the player
    static std::vector<ImVec2> screen_points;

    // the marker sits at the head of the line, so the plot gives it room instead of letting it
    // hang over the edge
    float const head{2.8f * scale};
    float const plot_width{std::max(1.f, width - head)};

    for(overlay::series_t const & series: chart.series)
      {
      if(series.points.size() < 2u)
        continue;

      screen_points.clear();
      for(overlay::point_t const & point: series.points)
        screen_points.push_back(
          ImVec2{
            origin.x + std::clamp(point.x, 0.f, 1.f) * plot_width,
            far_corner.y - std::clamp(point.y, 0.f, 1.f) * height
          }
        );

      ImU32 const colour{ImGui::GetColorU32(to_color(series.color))};
      draw->AddPolyline(
        screen_points.data(), static_cast<int>(screen_points.size()), colour, ImDrawFlags_None, 1.6f * scale
      );
      // the head of the line says which faction it is without a legend of its own
      draw_marker(draw, screen_points.back(), head, colour, series.marker);
      }

    // the labels go last, over the curves and on a ground of their own - a line crossing its own
    // decade was leaving the number unreadable exactly where it mattered
    float const font{ImGui::GetFontSize()};
    for(overlay::grid_line_t const & guide: chart.grid)
      {
      if(guide.label.empty())
        continue;

      float const y{far_corner.y - std::clamp(guide.y, 0.f, 1.f) * height};
      // above its own line, except at the top edge, where there is no room above
      float text_y{y - font};
      if(text_y < origin.y)
        text_y = y;
      text_y = std::min(text_y, far_corner.y - font);

      ImVec2 const at{origin.x + 2.f * scale, text_y};
      ImVec2 const size{ImGui::CalcTextSize(guide.label.c_str())};
      draw->AddRectFilled(
        ImVec2{at.x - 2.f * scale, at.y}, ImVec2{at.x + size.x + 2.f * scale, at.y + size.y}, IM_COL32(0, 0, 0, 150)
      );
      draw->AddText(at, IM_COL32(200, 200, 200, 190), guide.label.c_str());
      }
    }

  [[nodiscard]]
  auto make_pipeline(
    swapchain_data_t const & data,
    VkPipelineLayout layout,
    std::span<uint32_t const> vert_code,
    std::span<uint32_t const> frag_code,
    VkPipelineColorBlendAttachmentState const & blend
  ) -> VkPipeline;

  ///\brief draw callbacks for the lit balls, as for the ground: the pipeline is made from ImGui's layout
  /// while it records, and bound for the balls that follow. Blended as ImGui blends its own shapes
  auto make_spheres(ImDrawList const *, ImDrawCmd const * cmd) -> void
    {
    auto & data{*static_cast<swapchain_data_t *>(cmd->UserCallbackData)};
    auto const * state{static_cast<ImGui_ImplVulkan_RenderState const *>(ImGui::GetPlatformIO().Renderer_RenderState)};
    if(data.sphere_pipeline != VK_NULL_HANDLE or data.sphere_broken or state == nullptr)
      return;
    VkPipelineColorBlendAttachmentState const blend{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                      | VK_COLOR_COMPONENT_A_BIT
    };
    data.sphere_pipeline = make_pipeline(data, state->PipelineLayout, sphere_vert_spv, sphere_frag_spv, blend);
    data.sphere_broken = data.sphere_pipeline == VK_NULL_HANDLE;
    if(data.sphere_broken)
      log("sphere pipeline failed, the bodies stay flat discs");
    else
      log("sphere pipeline ready");
    }

  auto bind_spheres(ImDrawList const *, ImDrawCmd const * cmd) -> void
    {
    auto & data{*static_cast<swapchain_data_t *>(cmd->UserCallbackData)};
    auto const * state{static_cast<ImGui_ImplVulkan_RenderState const *>(ImGui::GetPlatformIO().Renderer_RenderState)};
    if(state != nullptr)
      data.device->CmdBindPipeline(state->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.sphere_pipeline);
    }

  ///\brief binds the ball pipeline unless it is bound already; false while there is none yet - the first
  /// frame only asks for it to be made - or when it could not be made, and the disc is then drawn flat
  [[nodiscard]]
  auto begin_spheres(swapchain_data_t & data, ImDrawList * draw, bool bound) -> bool
    {
    if(data.sphere_pipeline == VK_NULL_HANDLE)
      {
      if(not data.sphere_broken)
        draw->AddCallback(&make_spheres, &data);
      return false;
      }
    if(not bound)
      draw->AddCallback(&bind_spheres, &data);
    return true;
    }

  ///\brief a ball as a quad whose uv is the ball's own frame - -1..1 over the disc - turned so the light
  /// the shader always takes from -u comes from where the tool says; the vertex alpha carries the shine
  ///\param tile 0 for a ball of the disc's colour, else 1 + the atlas tile of its face - it travels in the
  /// uv as a whole multiple of 8 added to u, which the shader takes back off; the ball's own frame never
  /// reaches 4 from its middle, so the two cannot mix
  auto add_sphere(
    ImDrawList * draw, ImVec2 centre, float radius, ImVec2 light, overlay::disc_t const & disc, uint32_t tile
  ) -> void
    {
    if(radius <= 0.f)
      return;
    // a pixel and a half beyond the rim, for the edge the shader smooths - never so far as to reach 4
    float const reach{std::min((radius + 1.5f) / radius, 2.5f)};
    float const angle{light.x == 0.f and light.y == 0.f ? std::numbers::pi_v<float> : std::atan2(light.y, light.x)};
    float const turn{std::numbers::pi_v<float> - angle};
    float const c{std::cos(turn) * reach};
    float const s{std::sin(turn) * reach};
    float const shift{8.f * float(tile)};
    auto const uv = [&](float x, float y) { return ImVec2{x * c - y * s + shift, x * s + y * c}; };
    float const half{radius * reach};
    auto const alpha{
      disc.glows ? 255u : static_cast<uint32_t>(std::clamp(disc.gloss, 0.f, 1.f) * 127.f + 0.5f)
    };
    ImU32 const colour{(alpha << IM_COL32_A_SHIFT) | (ImGui::ColorConvertFloat4ToU32(to_color(disc.color)) & ~IM_COL32_A_MASK)};
    draw->PrimReserve(6, 4);
    draw->PrimQuadUV(
      ImVec2{centre.x - half, centre.y - half},
      ImVec2{centre.x + half, centre.y - half},
      ImVec2{centre.x + half, centre.y + half},
      ImVec2{centre.x - half, centre.y + half},
      uv(-1.f, -1.f),
      uv(1.f, -1.f),
      uv(1.f, 1.f),
      uv(-1.f, 1.f),
      colour
    );
    }

  ///\brief draws a picture the tool laid out, shrunk to the room there is when it would not fit
  ///\param room how tall the picture may be at most, 0 when there is no limit
  auto draw_diagram(swapchain_data_t & data, overlay::diagram_t const & diagram, float available, float room) -> void
    {
    if(diagram.width <= 0.f or diagram.height <= 0.f or available <= 0.f)
      return;

    float const scale{ImGui::GetFontSize() / 13.f};
    // across: the whole band; upright: the zoom asked for, but never so much taller than wide that
    // neighbouring discs run into each other
    available *= std::clamp(diagram.share, 0.1f, 1.f);
    float kx{available / diagram.width};
    float k{std::min(scale * std::max(diagram.zoom, 0.1f), kx * 1.3f)};
    // too tall for the room left - the whole picture shrinks, both ways, so the circles stay round and
    // the block does not climb onto the one above it
    if(room > 0.f and diagram.height * k > room)
      {
      float const shrink{std::max(room / (diagram.height * k), 0.2f)};
      kx *= shrink;
      k *= shrink;
      available *= shrink;
      }

    ImDrawList * const draw{ImGui::GetWindowDrawList()};
    ImVec2 const origin{ImGui::GetCursorScreenPos()};
    ImGui::Dummy(ImVec2{available, diagram.height * k});

    auto const at = [&](float x, float y) { return ImVec2{origin.x + x * kx, origin.y + y * k}; };

    for(overlay::segment_t const & segment: diagram.segments)
      {
      ImVec2 const a{
        segment.relative ? ImVec2{at(segment.ax, segment.ay).x + segment.x0 * k, at(segment.ax, segment.ay).y + segment.y0 * k}
                         : at(segment.x0, segment.y0)
      };
      ImVec2 const b{
        segment.relative ? ImVec2{at(segment.ax, segment.ay).x + segment.x1 * k, at(segment.ax, segment.ay).y + segment.y1 * k}
                         : at(segment.x1, segment.y1)
      };
      draw->AddLine(a, b, ImGui::GetColorU32(to_color(segment.color)), 1.2f * k);
      }

    // the balls go through a pipeline of their own, bound for each run of them and let go after, so the
    // discs keep the order the tool gave them - a marker ring still lies over its body
    bool balls{};
    for(overlay::disc_t const & disc: diagram.discs)
      {
      bool const ball{disc.sphere and not disc.outline and begin_spheres(data, draw, balls)};
      if(balls and not ball)
        draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
      balls = ball;
      if(ball)
        {
        // the light's direction on the screen, where the picture is stretched across more than upright
        ImVec2 const light{disc.light_x * kx, disc.light_y * k};
        if(auto const tile{face_tile(data, disc.face)}; tile)
          {
          draw->PushTextureID(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(face_texture(data))));
          add_sphere(draw, at(disc.x, disc.y), disc.radius * k, light, disc, *tile + 1u);
          draw->PopTextureID();
          }
        else
          add_sphere(draw, at(disc.x, disc.y), disc.radius * k, light, disc, 0u);
        continue;
        }
      ImU32 const colour{ImGui::GetColorU32(to_color(disc.color))};
      if(disc.outline)
        draw->AddCircle(at(disc.x, disc.y), disc.radius * k, colour, 24, 1.6f * k);
      else
        draw->AddCircleFilled(at(disc.x, disc.y), disc.radius * k, colour, 24);
      }
    if(balls)
      draw->AddCallback(ImDrawCallback_ResetRenderState, nullptr);

    // the labels follow the picture's own scale, so a shrunk picture keeps its numbers beside their discs
    ImFont * const font{data.small_font != nullptr ? data.small_font : ImGui::GetFont()};
    float const size{font->FontSize * k / scale};
    for(overlay::label_t const & label: diagram.labels)
      {
      if(label.text.empty())
        continue;
      ImVec2 const extent{font->CalcTextSizeA(size, FLT_MAX, 0.f, label.text.c_str())};
      ImVec2 const anchor{at(label.x, label.y)};
      draw->AddText(
        font,
        size,
        ImVec2{anchor.x - extent.x * std::clamp(label.align, 0.f, 1.f), anchor.y - extent.y * 0.5f},
        ImGui::GetColorU32(to_color(label.color)),
        label.text.c_str()
      );
      }
    }

  ///\brief the characters the overlay writes: Latin with its extended letters for names, and the few
  /// typographic ones the lines use
  constexpr std::array<ImWchar, 9> glyph_ranges{
    0x0020, 0x00ff,  // Latin-1
    0x0100, 0x017f,  // Latin Extended-A - Polish and the rest of Central Europe
    0x2013, 0x2026,  // dashes, quotes, bullet, ellipsis
    0x2190, 0x2193,  // arrows
    0
  };

  ///\brief adds the built-in face at a size - antialiased TrueType rasterised at that size, where the
  /// default bitmap face of imgui only grew its pixels
  auto add_overlay_font(ImFontConfig & config) -> ImFont *
    {
    // the data is ours and static - the atlas must neither free it nor write into it
    config.FontDataOwnedByAtlas = false;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
      const_cast<unsigned char *>(overlay_font_data),
      static_cast<int>(overlay_font_size),
      config.SizePixels,
      &config,
      glyph_ranges.data()
    );
    }

  ///\brief rasterises the text at the size the layout asks for, the emblems with it, and sizes the style
  ///\detail the font is rasterised at the target size, because stretching a finished bitmap turns to mush
  auto build_fonts(swapchain_data_t & data, overlay::layout_t const & layout) -> void
    {
    ImGuiIO & io{ImGui::GetIO()};
    float const scale{text_scale(layout, data.extent)};
    float const small{small_text_ratio(layout)};

    io.Fonts->Clear();
    ImFontConfig font_config{};
    font_config.SizePixels = std::round(13.f * scale);
    add_overlay_font(font_config);

    // the same face rasterised a second time rather than one bitmap stretched: a list of missions is
    // read line by line, not glanced at, and at the size that suits a glance it eats the band
    ImFontConfig small_config{};
    small_config.SizePixels = std::max(8.f, std::round(font_config.SizePixels * small));
    data.small_font = add_overlay_font(small_config);

    // the emblems take their place in the atlas before it is built, and are written into it right after
    data.emblem_rects = {-1, -1, -1, -1};
    reserve_emblems(data);
    auto const pictures{reserve_pictures(data)};
    blit_emblems(data);
    blit_pictures(data, pictures);

    // a fresh style each time, or a second scaling would multiply the first
    ImGui::GetStyle() = ImGuiStyle{};
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(scale);

    data.font_scale = scale;
    data.font_small = small;
    }

  ///\brief builds the fonts again when the layout asks for another size than they were made at
  ///\detail only the atlas and its texture go: our own frames in flight are waited for first, since the
  /// texture is still bound in them, and the backend uploads the new one at the start of the next frame
  auto follow_font_layout(swapchain_data_t & data) -> void
    {
    overlay::layout_t const layout{current_layout()};
    std::vector<std::string> pictures{wanted_pictures()};
    if(
      text_scale(layout, data.extent) == data.font_scale and small_text_ratio(layout) == data.font_small
      and pictures == data.picture_paths
    )
      return;
    data.picture_paths = std::move(pictures);

    device_data_t & device{*data.device};
    // only a frame actually submitted has a fence that will ever signal
    for(frame_resources_t const & frame: data.frames)
      if(frame.submitted and frame.fence != VK_NULL_HANDLE)
        device.WaitForFences(device.device, 1u, &frame.fence, VK_TRUE, fence_timeout_ns);

    ImGui_ImplVulkan_DestroyFontsTexture();
    build_fonts(data, layout);
    log(
      "fonts rebuilt at scale {:.2f}, small text {:.2f}, {} pictures",
      data.font_scale,
      data.font_small,
      data.picture_paths.size()
    );
    }

  [[nodiscard]]
  auto block_visible(overlay::block_t const & block, uint64_t age_ms) noexcept -> bool
    {
    return (not block.lines.empty() or not block.charts.empty() or not block.diagrams.empty()
            or not block.pictures.empty())
           and (block.ttl_ms == 0u or age_ms <= block.ttl_ms);
    }

  ///\brief shows where the picture will be and when, then that it was taken
  ///\detail The picture is copied out before anything of ours is drawn, so the frame never gets into it.
  /// It stands a little outside the square, so what it marks is the edge and not a strip of the picture
  auto draw_capture_guide(swapchain_data_t & data, ImVec2 display) -> void
    {
    constexpr double snap_s{0.15};
    constexpr double taken_s{0.5};
    double const now{now_seconds()};
    bool const waiting{data.armed_capture.has_value() and not data.armed_capture->quiet};
    bool const taken{not waiting and data.shutter_at >= 0.0 and now - data.shutter_at < taken_s};
    if(not waiting and not taken)
      return;

    float const scale{ImGui::GetFontSize() / 13.f};
    float const size{std::clamp(waiting ? data.armed_capture->size : data.shutter_size, 0.05f, 1.f)};
    float const side{std::min(std::round(display.y * size), display.x)};
    ImVec2 const a{(display.x - side) / 2.f, (display.y - side) / 2.f};
    ImVec2 const b{a.x + side, a.y + side};
    ImDrawList * const draw{ImGui::GetForegroundDrawList()};

    // corners of a viewfinder rather than a box - the plant stays in view between them. The shutter is
    // the corners snapping onto the edge and easing back, never a flash: Artur finds bright flashes tiring
    float margin{side * 0.02f};
    if(taken and now - data.shutter_at < snap_s)
      margin *= float((now - data.shutter_at) / snap_s);
    ImVec2 const fa{a.x - margin, a.y - margin};
    ImVec2 const fb{b.x + margin, b.y + margin};
    float const arm{side * 0.12f};
    float const thick{2.f * scale};
    ImU32 const frame_colour{taken ? IM_COL32(134, 217, 134, 230) : IM_COL32(255, 255, 255, 190)};
    for(auto const [x, y, dx, dy]: {std::array{fa.x, fa.y, 1.f, 1.f},
                                    std::array{fb.x, fa.y, -1.f, 1.f},
                                    std::array{fa.x, fb.y, 1.f, -1.f},
                                    std::array{fb.x, fb.y, -1.f, -1.f}})
      {
      draw->AddLine(ImVec2{x, y}, ImVec2{x + dx * arm, y}, frame_colour, thick);
      draw->AddLine(ImVec2{x, y}, ImVec2{x, y + dy * arm}, frame_colour, thick);
      }

    std::string const text{
      waiting ? std::format("hold still for the picture  {:.1f} s", std::max(0.0, data.capture_due - now))
              : std::string{"picture taken"}
    };
    ImVec2 const text_size{ImGui::CalcTextSize(text.c_str())};
    ImVec2 const at{(display.x - text_size.x) / 2.f, fa.y - text_size.y - 8.f * scale};
    draw->AddRectFilled(
      ImVec2{at.x - 6.f * scale, at.y - 3.f * scale},
      ImVec2{at.x + text_size.x + 6.f * scale, at.y + text_size.y + 3.f * scale},
      IM_COL32(0, 0, 0, 150)
    );
    draw->AddText(at, frame_colour, text.c_str());
    }

  ///\brief patches over the game's own interface, beneath everything else of ours
  ///\detail measured on the middle screen from its centre in shares of its height, so the patch lands on
  /// the same spot of the game's interface on any screen - the game draws that interface at 16:9 there
  auto draw_covers(swapchain_data_t const & data, std::vector<overlay::cover_t> const & covers, ImVec2 display,
                   overlay::layout_t const & layout) -> void
    {
    if(covers.empty())
      return;

    float const unit{centre_screen_width(display, layout) * 9.f / 16.f};
    ImVec2 const centre{display.x / 2.f, display.y / 2.f};
    ImDrawList * const draw{ImGui::GetBackgroundDrawList()};
    ImFontAtlas & atlas{*ImGui::GetIO().Fonts};
    for(overlay::cover_t const & cover: covers)
      {
      ImVec2 const middle{centre.x + cover.x * unit, centre.y + cover.y * unit};
      ImVec2 const half{cover.width * unit / 2.f, cover.height * unit / 2.f};
      draw->AddRectFilled(
        ImVec2{std::round(middle.x - half.x), std::round(middle.y - half.y)},
        ImVec2{std::round(middle.x + half.x), std::round(middle.y + half.y)},
        ImGui::ColorConvertFloat4ToU32(to_color(cover.ground))
      );

      if(cover.emblem == overlay::emblem_e::none or cover.emblem_height <= 0.f)
        continue;
      int const index{data.emblem_rects[static_cast<size_t>(cover.emblem)]};
      if(index < 0)
        continue;
      ImFontAtlasCustomRect const * const rect{atlas.GetCustomRectByIndex(index)};
      if(rect->Height == 0)
        continue;

      ImVec2 uv_min{};
      ImVec2 uv_max{};
      atlas.CalcCustomRectUV(rect, &uv_min, &uv_max);
      float const height{cover.emblem_height * unit};
      float const width{height * static_cast<float>(rect->Width) / static_cast<float>(rect->Height)};
      ImVec2 const at{std::round(middle.x - width / 2.f), std::round(middle.y - height / 2.f)};
      draw->AddImage(
        atlas.TexID, at, ImVec2{at.x + width, at.y + height}, uv_min, uv_max, ImGui::ColorConvertFloat4ToU32(to_color(cover.emblem_color))
      );
      }
    }

  ///\brief a word at the top of the middle screen that the screenshot was taken - small and green, no flash
  auto draw_screenshot_notice(swapchain_data_t & data, ImVec2 display) -> void
    {
    constexpr double shown_s{1.5};
    double const now{now_seconds()};
    if(data.screenshot_at < 0.0 or now - data.screenshot_at >= shown_s)
      return;

    float const scale{ImGui::GetFontSize() / 13.f};
    char const * const text{"screenshot"};
    ImVec2 const text_size{ImGui::CalcTextSize(text)};
    ImVec2 const at{(display.x - text_size.x) / 2.f, display.y * 0.06f};
    ImDrawList * const draw{ImGui::GetForegroundDrawList()};
    draw->AddRectFilled(
      ImVec2{at.x - 6.f * scale, at.y - 3.f * scale},
      ImVec2{at.x + text_size.x + 6.f * scale, at.y + text_size.y + 3.f * scale},
      IM_COL32(0, 0, 0, 150)
    );
    draw->AddText(at, IM_COL32(134, 217, 134, 230), text);
    }

  ///\brief a pipeline fed like ImGui's own - its vertex layout and pipeline layout, so the draw list's
  /// buffers serve it - with shaders and a blend of its own; null when it could not be made
  [[nodiscard]]
  auto make_pipeline(
    swapchain_data_t const & data,
    VkPipelineLayout layout,
    std::span<uint32_t const> vert_code,
    std::span<uint32_t const> frag_code,
    VkPipelineColorBlendAttachmentState const & blend
  ) -> VkPipeline
    {
    device_data_t & device{*data.device};
    if(
      device.CreateShaderModule == nullptr or device.DestroyShaderModule == nullptr
      or device.CreateGraphicsPipelines == nullptr or device.CmdBindPipeline == nullptr
    )
      return VK_NULL_HANDLE;

    auto const module = [&](std::span<uint32_t const> code) -> VkShaderModule
    {
      VkShaderModuleCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .codeSize = code.size() * sizeof(uint32_t),
        .pCode = code.data()
      };
      VkShaderModule made{};
      return device.CreateShaderModule(device.device, &info, nullptr, &made) == VK_SUCCESS ? made : VK_NULL_HANDLE;
    };
    VkShaderModule const vert{module(vert_code)};
    VkShaderModule const frag{module(frag_code)};
    VkPipeline pipeline{};

    if(vert != VK_NULL_HANDLE and frag != VK_NULL_HANDLE)
      {
      std::array<VkPipelineShaderStageCreateInfo, 2> const stages{
        VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0u,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = vert,
          .pName = "main",
          .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .pNext = nullptr,
          .flags = 0u,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = frag,
          .pName = "main",
          .pSpecializationInfo = nullptr
        }
      };
      VkVertexInputBindingDescription const binding{
        .binding = 0u, .stride = sizeof(ImDrawVert), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
      };
      std::array<VkVertexInputAttributeDescription, 3> const attributes{
        VkVertexInputAttributeDescription{
          .location = 0u, .binding = 0u, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ImDrawVert, pos)
        },
        VkVertexInputAttributeDescription{
          .location = 1u, .binding = 0u, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(ImDrawVert, uv)
        },
        VkVertexInputAttributeDescription{
          .location = 2u, .binding = 0u, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(ImDrawVert, col)
        }
      };
      VkPipelineVertexInputStateCreateInfo const vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .vertexBindingDescriptionCount = 1u,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
        .pVertexAttributeDescriptions = attributes.data()
      };
      VkPipelineInputAssemblyStateCreateInfo const assembly{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
      };
      VkPipelineViewportStateCreateInfo const viewport{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .viewportCount = 1u,
        .pViewports = nullptr,
        .scissorCount = 1u,
        .pScissors = nullptr
      };
      VkPipelineRasterizationStateCreateInfo const raster{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.f,
        .depthBiasClamp = 0.f,
        .depthBiasSlopeFactor = 0.f,
        .lineWidth = 1.f
      };
      VkPipelineMultisampleStateCreateInfo const multisample{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
      };
      VkPipelineColorBlendStateCreateInfo const blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_CLEAR,
        .attachmentCount = 1u,
        .pAttachments = &blend,
        .blendConstants = {0.f, 0.f, 0.f, 0.f}
      };
      std::array<VkDynamicState, 2> const dynamic{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
      VkPipelineDynamicStateCreateInfo const dynamic_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .dynamicStateCount = static_cast<uint32_t>(dynamic.size()),
        .pDynamicStates = dynamic.data()
      };
      VkGraphicsPipelineCreateInfo const info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0u,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertex_input,
        .pInputAssemblyState = &assembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport,
        .pRasterizationState = &raster,
        .pMultisampleState = &multisample,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &blend_state,
        .pDynamicState = &dynamic_state,
        .layout = layout,
        .renderPass = data.render_pass,
        .subpass = 0u,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
      };
      if(device.CreateGraphicsPipelines(device.device, VK_NULL_HANDLE, 1u, &info, nullptr, &pipeline) != VK_SUCCESS)
        pipeline = VK_NULL_HANDLE;
      }

    if(vert != VK_NULL_HANDLE)
      device.DestroyShaderModule(device.device, vert, nullptr);
    if(frag != VK_NULL_HANDLE)
      device.DestroyShaderModule(device.device, frag, nullptr);
    return pipeline;
    }

  ///\brief the pipeline the ground under the blocks is drawn with
  ///\detail ImGui's own vertex stage and layout, so the draw list feeds it as it feeds ImGui, but a blend
  /// that reads the game beneath: colour = src*dst + dst*(1-dst). With the ground's grey k as src that is
  /// g*(1+k) - g*g - dark space kept as it is, a bright ice body pulled down to dark grey. Nothing of
  /// the game is read back to decide it: the blend does it for every pixel, at once, with no flicker
  auto make_ground_pipeline(swapchain_data_t & data, VkPipelineLayout layout) -> void
    {
    // the game's alpha is left as it is - only the colour is pulled down
    VkPipelineColorBlendAttachmentState const blend{
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .alphaBlendOp = VK_BLEND_OP_ADD,
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT
                      | VK_COLOR_COMPONENT_A_BIT
    };
    data.ground_pipeline = make_pipeline(data, layout, ground_vert_spv, ground_frag_spv, blend);
    data.ground_broken = data.ground_pipeline == VK_NULL_HANDLE;
    if(data.ground_broken)
      log("ground pipeline failed, the blocks keep a plain ground");
    else
      log("ground pipeline ready");
    }

  ///\brief draw callbacks: the pipeline can only be made while ImGui records - its layout is not
  /// shown anywhere else. One makes it, the other binds it for the ground that follows
  auto make_ground(ImDrawList const *, ImDrawCmd const * cmd) -> void
    {
    auto & data{*static_cast<swapchain_data_t *>(cmd->UserCallbackData)};
    auto const * state{static_cast<ImGui_ImplVulkan_RenderState const *>(ImGui::GetPlatformIO().Renderer_RenderState)};
    if(data.ground_pipeline == VK_NULL_HANDLE and not data.ground_broken and state != nullptr)
      make_ground_pipeline(data, state->PipelineLayout);
    }

  auto bind_ground(ImDrawList const *, ImDrawCmd const * cmd) -> void
    {
    auto & data{*static_cast<swapchain_data_t *>(cmd->UserCallbackData)};
    auto const * state{static_cast<ImGui_ImplVulkan_RenderState const *>(ImGui::GetPlatformIO().Renderer_RenderState)};
    if(state != nullptr)
      data.device->CmdBindPipeline(state->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, data.ground_pipeline);
    }

  ///\brief the ground under the block just begun: pulled down by how bright the game is, then darkened
  /// as before; without the pipeline, the plain half-transparent ground alone
  auto draw_ground(swapchain_data_t & data, overlay::layout_t const & layout) -> void
    {
    ImDrawList * const list{ImGui::GetWindowDrawList()};
    ImVec2 const from{ImGui::GetWindowPos()};
    ImVec2 const to{from.x + ImGui::GetWindowSize().x, from.y + ImGui::GetWindowSize().y};
    float const rounding{ImGui::GetStyle().WindowRounding};
    ImVec4 plain{ImGui::GetStyle().Colors[ImGuiCol_WindowBg]};
    plain.w = std::clamp(layout.window_alpha, 0.f, 1.f);

    if(layout.bright_ground >= 0.f and data.ground_pipeline != VK_NULL_HANDLE)
      {
      auto const k{static_cast<ImU32>(std::clamp(layout.bright_ground, 0.f, 1.f) * 255.f + 0.5f)};
      list->AddCallback(&bind_ground, &data);
      list->AddRectFilled(from, to, IM_COL32(k, k, k, 255), rounding);
      list->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
      }
    else if(layout.bright_ground >= 0.f and not data.ground_broken)
      list->AddCallback(&make_ground, &data);
    list->AddRectFilled(from, to, ImGui::GetColorU32(plain), rounding);
    }

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
    overlay::layout_t const layout{snapshot ? snapshot->frame.layout : overlay::layout_t{}};
    float const band{side_band_width(display, layout)};

    // one window of blocks; its size is what the next window beside it has to keep clear of
    auto const draw_window = [&](
                               char const * name,
                               ImVec2 position,
                               ImVec2 pivot,
                               float width,
                               bool stats_here,
                               auto const & wanted,
                               float height_limit
                             ) -> ImVec2
    {
      // the ground is drawn by hand, see draw_ground
      ImGui::SetNextWindowBgAlpha(0.f);
      ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
      // long text should wrap inside the band rather than run into the player's field of view
      ImGui::SetNextWindowSizeConstraints(ImVec2{0.f, 0.f}, ImVec2{width, display.y});

      ImVec2 size{};
      if(ImGui::Begin(name, nullptr, flags))
        {
        draw_ground(data, layout);
        wrap_right = width - 2.f * ImGui::GetStyle().WindowPadding.x;
        ImGui::PushTextWrapPos(wrap_right);
        if(stats_here)
          {
          coloured_text(
            0x9ad1ffu, "EHT overlay  %.0f fps  frame %llu", double{data.fps}, (unsigned long long)data.drawn_frames
          );
          auto & client{ipc_client()};
          if(client.connected())
            coloured_text(0x86d986u, "elite_help_tool: connected, %llu frames", (unsigned long long)client.received());
          else
            coloured_text(0xd9a34au, "elite_help_tool: waiting for connection");

          // a frame that cannot be read is almost always a layer older than the tool feeding it,
          // and without saying so the overlay simply looks dead
          if(auto const lost{client.rejected()}; lost != 0u)
            coloured_text(
              0xd9534fu, "%llu frames rejected - install this layer, it is older than the tool",
              (unsigned long long)lost
            );
          }

        if(snapshot)
          for(overlay::block_t const & block: snapshot->frame.blocks)
            {
            if(not wanted(block) or not block_visible(block, age_ms))
              continue;

            if(stats_here)
              ImGui::Separator();

            bool const small{block.text == overlay::text_e::small and data.small_font != nullptr};
            if(small)
              ImGui::PushFont(data.small_font);

            for(overlay::line_t const & line: block.lines)
              draw_line(data, line);

            // a chart wider than this is no more readable, only more of the view taken away
            float const chart_width{
              std::min(width - 2.f * ImGui::GetStyle().WindowPadding.x, layout.chart_width * ImGui::GetFontSize() / 13.f)
            };
            for(overlay::chart_t const & chart: block.charts)
              draw_chart(chart, chart_width);

            for(overlay::diagram_t const & diagram: block.diagrams)
              draw_diagram(
                data,
                diagram,
                width - 2.f * ImGui::GetStyle().WindowPadding.x,
                height_limit > 0.f ? std::max(height_limit - ImGui::GetCursorPosY() - ImGui::GetStyle().WindowPadding.y, 1.f)
                                   : 0.f
              );

            if(not block.pictures.empty())
              draw_pictures(data, block, width - 2.f * ImGui::GetStyle().WindowPadding.x);

            if(small)
              ImGui::PopFont();
            }

        ImGui::PopTextWrapPos();
        size = ImGui::GetWindowSize();
        }
      ImGui::End();
      return size;
    };

    auto const any_visible = [&](auto const & wanted) -> bool
    {
      if(snapshot)
        for(overlay::block_t const & block: snapshot->frame.blocks)
          if(wanted(block) and block_visible(block, age_ms))
            return true;
      return false;
    };

    constexpr std::array corners{
      overlay::corner_e::top_left,
      overlay::corner_e::top_right,
      overlay::corner_e::bottom_left,
      overlay::corner_e::bottom_right,
      overlay::corner_e::centre_top_left,
      overlay::corner_e::centre_top_right
    };
    std::array<ImVec2, corners.size()> stack_size{};

    for(size_t index{}; index != corners.size(); ++index)
      {
      overlay::corner_e const corner{corners[index]};
      bool const stats_here{layout.stats and corner == overlay::corner_e::top_right};
      auto const in_stack = [corner](overlay::block_t const & block) -> bool
      { return block.corner == corner and not block.beside and not block.middle; };

      if(not stats_here and not any_visible(in_stack))
        continue;

      bool const head_up{
        corner == overlay::corner_e::centre_top_left or corner == overlay::corner_e::centre_top_right
      };
      // a head-up readout is glanced at, not read - past this width it stops being a glance
      float const width{head_up ? std::min(band, centre_screen_width(display, layout) * layout.hud_width) : band};

      // a bottom stack may reach no higher than where the stack above it ends - drawn first, so its
      // size is known by now; only a picture can give way, text is never cut
      float height_limit{};
      if(corner == overlay::corner_e::bottom_left or corner == overlay::corner_e::bottom_right)
        {
        size_t const above{corner == overlay::corner_e::bottom_left ? 0u : 1u};
        if(stack_size[above].y > 0.f)
          height_limit = display.y - 2.f * layout.corner_margin - stack_size[above].y - layout.corner_margin;
        }

      auto const [position, pivot]{corner_position(corner, display, layout)};
      stack_size[index] = draw_window(window_name(corner), position, pivot, width, stats_here, in_stack, height_limit);
      }

    // the blocks asked to stand beside a stack take what is left of the band next to it, on the same edge;
    // when the stack leaves too little of the band they go on top of it after all
    for(size_t index{}; index != 4u; ++index)
      {
      overlay::corner_e const corner{corners[index]};
      auto const beside = [corner](overlay::block_t const & block) -> bool
      { return block.corner == corner and block.beside; };
      if(not any_visible(beside))
        continue;

      bool const left{corner == overlay::corner_e::top_left or corner == overlay::corner_e::bottom_left};
      bool const bottom{corner == overlay::corner_e::bottom_left or corner == overlay::corner_e::bottom_right};
      ImVec2 const used{stack_size[index]};
      float const gap{layout.corner_margin};
      auto [position, pivot]{corner_position(corner, display, layout)};

      float width{band - used.x - gap};
      if(used.x == 0.f)
        width = band;
      else if(width >= band * 0.25f)
        position.x += left ? used.x + gap : -(used.x + gap);
      else
        {
        width = band;
        position.y += bottom ? -(used.y + gap) : used.y + gap;
        }

      static constexpr std::array names{
        "eht_top_left_beside", "eht_top_right_beside", "eht_bottom_left_beside", "eht_bottom_right_beside"
      };
      draw_window(names[index], position, pivot, width, false, beside, 0.f);
      }

    // the blocks that stand in the middle screen, each a window of its own, centred across it
    if(snapshot)
      {
      float const unit{centre_screen_width(display, layout) * 9.f / 16.f};
      size_t count{};
      for(overlay::block_t const & block: snapshot->frame.blocks)
        {
        if(not block.middle or not block_visible(block, age_ms))
          continue;
        std::string const name{std::format("eht_middle_{}", count++)};
        draw_window(
          name.c_str(),
          ImVec2{display.x / 2.f, display.y / 2.f + block.middle_y * unit},
          ImVec2{0.5f, 0.f},
          std::max(block.middle_width * unit, 1.f),
          false,
          [&block](overlay::block_t const & other) -> bool { return &other == &block; },
          0.f
        );
        }
      }

    // a patch outliving the tool would hide the game's own interface for good, so a silent tool takes it away
    // - one that lives sends a frame at least every few seconds
    constexpr uint64_t cover_ttl_ms{10000u};
    if(snapshot and age_ms < cover_ttl_ms)
      draw_covers(data, snapshot->frame.covers, display, layout);
    draw_capture_guide(data, display);
    draw_screenshot_notice(data, display);
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

  auto const started{now_seconds()};
  bool const was_ready{data.ready};
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
  destroy_capture(data);
  destroy_faces(data);

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
  if(data.ground_pipeline != VK_NULL_HANDLE)
    {
    device.DestroyPipeline(device.device, data.ground_pipeline, nullptr);
    data.ground_pipeline = VK_NULL_HANDLE;
    }
  data.ground_broken = false;
  if(data.sphere_pipeline != VK_NULL_HANDLE)
    {
    device.DestroyPipeline(device.device, data.sphere_pipeline, nullptr);
    data.sphere_pipeline = VK_NULL_HANDLE;
    }
  data.sphere_broken = false;
  if(data.render_pass != VK_NULL_HANDLE)
    {
    device.DestroyRenderPass(device.device, data.render_pass, nullptr);
    data.render_pass = VK_NULL_HANDLE;
    }

  data.ready = false;

  if(was_ready)
    log("overlay resources released in {:.1f} ms", (now_seconds() - started) * 1000.0);
  }

auto ensure_resources(swapchain_data_t & data, VkQueue queue) -> bool
  {
  if(data.ready)
    return true;

  auto const started{now_seconds()};
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
    // the game's image must stay untouched - we add to it, we do not clear it
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
    // the loader requires every new dispatchable handle to receive a dispatch table from us
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
  // the overlay never creates files in the game's directory
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.DisplaySize = ImVec2{static_cast<float>(data.extent.width), static_cast<float>(data.extent.height)};
  build_fonts(data, current_layout());

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
  log(
    "overlay ready for swapchain {}x{}, {} images, setup took {:.1f} ms",
    data.extent.width,
    data.extent.height,
    data.images.size(),
    (now_seconds() - started) * 1000.0
  );
  return true;
  }

auto renew_present_semaphore(swapchain_data_t & data, uint32_t image_index) noexcept -> void
  {
  if(data.device == nullptr or image_index >= data.frames.size())
    return;

  device_data_t & device{*data.device};
  frame_resources_t & frame{data.frames[image_index]};

  // our own submission may still be running, so we wait for it to finish first
  if(frame.submitted)
    {
    if(device.WaitForFences(device.device, 1u, &frame.fence, VK_TRUE, fence_timeout_ns) != VK_SUCCESS)
      {
      data.broken = true;
      return;
      }
    device.ResetFences(device.device, 1u, &frame.fence);
    frame.submitted = false;
    }

  VkSemaphoreCreateInfo const semaphore_info{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0u
  };
  VkSemaphore replacement{};
  if(device.CreateSemaphore(device.device, &semaphore_info, nullptr, &replacement) != VK_SUCCESS)
    {
    data.broken = true;
    return;
    }

  device.DestroySemaphore(device.device, frame.semaphore, nullptr);
  frame.semaphore = replacement;
  log("present failed, presentation semaphore replaced for image {}", image_index);
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
      // one failed attempt is enough - from there the game goes its own way without us
      data.broken = true;
      return VK_NULL_HANDLE;
      }

    if(image_index >= data.frames.size())
      return VK_NULL_HANDLE;

    device_data_t & device{*data.device};
    frame_resources_t & frame{data.frames[image_index]};

    double const entered{now_seconds()};
    if(frame.submitted)
      {
      // a whole second is a failure; we would rather lose the overlay than stall the game's frames
      if(device.WaitForFences(device.device, 1u, &frame.fence, VK_TRUE, fence_timeout_ns) != VK_SUCCESS)
        {
        log("overlay command buffer did not finish in time, disabling for this swapchain");
        data.broken = true;
        return VK_NULL_HANDLE;
        }
      device.ResetFences(device.device, 1u, &frame.fence);
      frame.submitted = false;
      }
    // the fence is behind us, so a picture copied out by this frame's last submission is complete
    collect_capture(data, frame);

    auto const now{now_seconds()};
    auto const delta{std::max(1.0 / 10000.0, now - data.last_draw_seconds)};
    data.last_draw_seconds = now;
    if(debug_enabled()) [[unlikely]]
      {
      auto & r{data.report};
      if(r.started == 0.0)
        r.started = now;
      ++r.frames;
      r.worst_gap = std::max(r.worst_gap, delta);
      r.waiting += now - entered;
      if(now - r.started >= 10.0)
        {
        log(
          "{}x{}: {:.1f} fps, worst frame {:.0f} ms, ours per frame {:.2f} ms drawing + {:.2f} ms waiting",
          data.extent.width,
          data.extent.height,
          double(r.frames) / (now - r.started),
          r.worst_gap * 1000.0,
          r.drawing * 1000.0 / double(r.frames),
          r.waiting * 1000.0 / double(r.frames)
        );
        r = {};
        }
      }
    data.fps = static_cast<float>(0.9 * double{data.fps} + 0.1 / delta);
    ++data.drawn_frames;

    ImGui::SetCurrentContext(data.imgui);
    ImGuiIO & io{ImGui::GetIO()};
    io.DisplaySize = ImVec2{static_cast<float>(data.extent.width), static_cast<float>(data.extent.height)};
    io.DeltaTime = static_cast<float>(delta);

    // a new request waits for its moment first, and the frame is shown meanwhile
    // Without the copy usage on the images there will be no picture, and a frame promising one would lie.
    // A picture the player is holding still for is not given up for one nobody is waiting for
    if(
      auto fresh{take_capture_request()};
      fresh and data.capturable and not data.capture_broken
      and not(fresh->quiet and data.armed_capture and not data.armed_capture->quiet)
    )
      {
      data.capture_due = now + double(fresh->delay_ms) / 1000.0;
      data.armed_capture = std::move(fresh);
      }

    // the key is heard on a thread of its own; a press waits here until some frame can take it
    if(take_screenshot_press() and data.capturable and not data.capture_broken)
      data.screenshot_wanted = true;

    follow_font_layout(data);
    ImGui_ImplVulkan_NewFrame();
    ImGui::NewFrame();
    build_ui(data);
    ImGui::Render();

    ImDrawData * const draw_data{ImGui::GetDrawData()};
    std::optional<overlay::capture_t> request;
    if(data.armed_capture and now >= data.capture_due)
      {
      request = std::move(data.armed_capture);
      data.armed_capture.reset();
      }
    bool const nothing_drawn{draw_data == nullptr or draw_data->CmdListsCount == 0};
    if(nothing_drawn and not request and not data.screenshot_wanted)
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
    // the faces read since the last frame go into their atlas before anything is drawn with it
    record_face_uploads(data, frame, image_index);

    // the picture comes before the overlay is drawn over the image - it is of the game, not of us
    bool const capturing{request and record_capture(data, frame, image_index, *request)};
    if(capturing and not request->quiet)
      {
      data.shutter_at = now;
      data.shutter_size = request->size;
      }

    device.CmdBeginRenderPass(frame.command_buffer, &pass_info, VK_SUBPASS_CONTENTS_INLINE);
    if(not nothing_drawn)
      ImGui_ImplVulkan_RenderDrawData(draw_data, frame.command_buffer);
    device.CmdEndRenderPass(frame.command_buffer);

    // the screenshot comes after the overlay - it is the screen as the player sees it. A picture of the
    // middle taken this same frame has the buffer, so the screenshot waits for the next one
    bool const screenshot{
      data.screenshot_wanted and not capturing and record_screenshot(data, frame, image_index)
    };
    if(screenshot)
      {
      data.screenshot_wanted = false;
      data.screenshot_at = now;
      }

    if(device.EndCommandBuffer(frame.command_buffer) != VK_SUCCESS)
      {
      data.broken = true;
      return VK_NULL_HANDLE;
      }

    std::array<VkPipelineStageFlags, max_wait_semaphores> stages{};
    // the copy reads the game's image too, so it waits for the game like the drawing does
    stages.fill(
      capturing or screenshot ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT
                : VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    );

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

    data.report.drawing += now_seconds() - now;
    if(auto const spent{now_seconds() - now}; spent > slow_draw_seconds) [[unlikely]]
      log("overlay draw took {:.1f} ms", spent * 1000.0);

    return frame.semaphore;
    }
  catch(...)
    {
    data.broken = true;
    return VK_NULL_HANDLE;
    }
  }
  }  // namespace eht_overlay
