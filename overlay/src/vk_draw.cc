#include "vk_draw.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <overlay_emblems.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdlib>
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

  ///\brief the trend mark and whatever follows it, both optional
  auto draw_trailing(overlay::line_t const & line, float box, float spacing, ImU32 colour) -> void
    {
    if(line.trend != overlay::trend_e::unknown)
      {
      ImGui::SameLine(0.f, spacing);
      // a mark that does not fit beside the value would be wrapped to a line of its own, where it
      // would stand next to nothing
      if(ImGui::GetContentRegionAvail().x < box)
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
      if(ImGui::GetContentRegionAvail().x < ImGui::CalcTextSize(line.suffix.c_str()).x)
        ImGui::NewLine();

      coloured_text(line.color, "%s", line.suffix.c_str());
      }
    }

  ///\brief a line of text, preceded by its marker and its emblem when it carries them
  auto draw_line(swapchain_data_t const & data, overlay::line_t const & line) -> void
    {
    float const box{ImGui::GetFontSize()};

    if(line.marker == overlay::marker_e::none and line.emblem == overlay::emblem_e::none)
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
    float const emblem_slot{line.marker != overlay::marker_e::none ? emblem_height * widest_emblem : 0.f};
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

  ///\brief draws a picture the tool laid out, shrunk to the room there is when it would not fit
  auto draw_diagram(swapchain_data_t & data, overlay::diagram_t const & diagram, float available) -> void
    {
    if(diagram.width <= 0.f or diagram.height <= 0.f or available <= 0.f)
      return;

    float const scale{ImGui::GetFontSize() / 13.f};
    // across: the whole band; upright: the zoom asked for, but never so much taller than wide that
    // neighbouring discs run into each other
    available *= std::clamp(diagram.share, 0.1f, 1.f);
    float const kx{available / diagram.width};
    float const k{std::min(scale * std::max(diagram.zoom, 0.1f), kx * 1.3f)};

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

    for(overlay::disc_t const & disc: diagram.discs)
      {
      ImU32 const colour{ImGui::GetColorU32(to_color(disc.color))};
      if(disc.outline)
        draw->AddCircle(at(disc.x, disc.y), disc.radius * k, colour, 24, 1.6f * k);
      else
        draw->AddCircleFilled(at(disc.x, disc.y), disc.radius * k, colour, 24);
      }

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
    io.Fonts->AddFontDefault(&font_config);

    // the same face rasterised a second time rather than one bitmap stretched: a list of missions is
    // read line by line, not glanced at, and at the size that suits a glance it eats the band
    ImFontConfig small_config{};
    small_config.SizePixels = std::max(8.f, std::round(font_config.SizePixels * small));
    data.small_font = io.Fonts->AddFontDefault(&small_config);

    // the emblems take their place in the atlas before it is built, and are written into it right after
    data.emblem_rects = {-1, -1, -1, -1};
    reserve_emblems(data);
    blit_emblems(data);

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
    if(text_scale(layout, data.extent) == data.font_scale and small_text_ratio(layout) == data.font_small)
      return;

    device_data_t & device{*data.device};
    // only a frame actually submitted has a fence that will ever signal
    for(frame_resources_t const & frame: data.frames)
      if(frame.submitted and frame.fence != VK_NULL_HANDLE)
        device.WaitForFences(device.device, 1u, &frame.fence, VK_TRUE, fence_timeout_ns);

    ImGui_ImplVulkan_DestroyFontsTexture();
    build_fonts(data, layout);
    log("fonts rebuilt at scale {:.2f}, small text {:.2f}", data.font_scale, data.font_small);
    }

  [[nodiscard]]
  auto block_visible(overlay::block_t const & block, uint64_t age_ms) noexcept -> bool
    {
    return (not block.lines.empty() or not block.charts.empty() or not block.diagrams.empty())
           and (block.ttl_ms == 0u or age_ms <= block.ttl_ms);
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

    for(auto const corner:
        {overlay::corner_e::top_left,
         overlay::corner_e::top_right,
         overlay::corner_e::bottom_left,
         overlay::corner_e::bottom_right,
         overlay::corner_e::centre_top_left,
         overlay::corner_e::centre_top_right})
      {
      bool const stats_here{layout.stats and corner == overlay::corner_e::top_right};

      bool anything{stats_here};
      if(snapshot and not anything)
        for(overlay::block_t const & block: snapshot->frame.blocks)
          anything = anything or (block.corner == corner and block_visible(block, age_ms));

      if(not anything)
        continue;

      bool const head_up{
        corner == overlay::corner_e::centre_top_left or corner == overlay::corner_e::centre_top_right
      };
      // a head-up readout is glanced at, not read - past this width it stops being a glance
      float const width{head_up ? std::min(band, centre_screen_width(display, layout) * layout.hud_width) : band};

      auto const [position, pivot]{corner_position(corner, display, layout)};
      ImGui::SetNextWindowBgAlpha(std::clamp(layout.window_alpha, 0.f, 1.f));
      ImGui::SetNextWindowPos(position, ImGuiCond_Always, pivot);
      // long text should wrap inside the band rather than run into the player's field of view
      ImGui::SetNextWindowSizeConstraints(ImVec2{0.f, 0.f}, ImVec2{width, display.y});

      if(ImGui::Begin(window_name(corner), nullptr, flags))
        {
        ImGui::PushTextWrapPos(width - 2.f * ImGui::GetStyle().WindowPadding.x);
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
            if(block.corner != corner or not block_visible(block, age_ms))
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
              draw_diagram(data, diagram, width - 2.f * ImGui::GetStyle().WindowPadding.x);

            if(small)
              ImGui::PopFont();
            }

        ImGui::PopTextWrapPos();
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

    auto const now{now_seconds()};
    auto const delta{std::max(1.0 / 10000.0, now - data.last_draw_seconds)};
    data.last_draw_seconds = now;
    data.fps = static_cast<float>(0.9 * double{data.fps} + 0.1 / delta);
    ++data.drawn_frames;

    ImGui::SetCurrentContext(data.imgui);
    ImGuiIO & io{ImGui::GetIO()};
    io.DisplaySize = ImVec2{static_cast<float>(data.extent.width), static_cast<float>(data.extent.height)};
    io.DeltaTime = static_cast<float>(delta);

    follow_font_layout(data);
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
