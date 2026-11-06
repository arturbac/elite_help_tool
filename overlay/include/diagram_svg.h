#pragma once

#include <overlay_protocol.h>
#include <ring_ellipse.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

///\brief the overlay's picture of a system as an SVG for a page: the same shapes in the same order, lit by a gradient
/// where the layer has a shader. A ball's face is the disc's face, which here is a link to the picture
namespace overlay
  {
namespace svg_detail
  {
[[nodiscard]]
inline auto hex(uint32_t rgb) -> std::string
  { return std::format("#{:06x}", rgb & 0xffffffu); }

[[nodiscard]]
inline auto dimmed(uint32_t rgb, float by) -> std::string
  {
  auto const part = [rgb, by](uint32_t shift) { return static_cast<uint32_t>(static_cast<float>((rgb >> shift) & 0xffu) * by); };
  return std::format("#{:02x}{:02x}{:02x}", part(16u), part(8u), part(0u));
  }

[[nodiscard]]
inline auto escaped(std::string_view text) -> std::string
  {
  std::string out;
  out.reserve(text.size());
  for(char const c: text)
    switch(c)
      {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c;
      }
  return out;
  }

[[nodiscard]]
inline auto ring_half(disc_t const & disc, ball_ring_t const & ring, bool near) -> std::string
  {
  std::string path;
  for(ring_point_t const & p: half_ring(ring.radius, disc.ring_lean, near))
    path += std::format("{}{:.2f},{:.2f}", path.empty() ? "M" : " L", disc.x + p.x, disc.y + p.y);
  return std::format("<path d=\"{}\" fill=\"none\" stroke=\"{}\" stroke-width=\"0.6\"/>", path, hex(ring.color));
  }
  }  // namespace svg_detail

///\brief the picture at this many pixels for one of its own
inline constexpr float svg_scale{2.6f};
inline constexpr float svg_label_size{6.5f};

[[nodiscard]]
inline auto diagram_svg(diagram_t const & diagram) -> std::string
  {
  using namespace svg_detail;
  std::string defs;
  std::string shapes;

  for(segment_t const & s: diagram.segments)
    {
    float const ox{s.relative ? s.ax : 0.f};
    float const oy{s.relative ? s.ay : 0.f};
    shapes += std::format(
      "<line x1=\"{:.2f}\" y1=\"{:.2f}\" x2=\"{:.2f}\" y2=\"{:.2f}\" stroke=\"{}\" stroke-width=\"0.6\"/>",
      ox + s.x0,
      oy + s.y0,
      ox + s.x1,
      oy + s.y1,
      hex(s.color)
    );
    }

  for(facet_t const & facet: diagram.facets)
    {
    std::string points;
    for(point_t const & p: facet.points)
      points += std::format("{:.2f},{:.2f} ", facet.ax + p.x, facet.ay + p.y);
    shapes += std::format("<polygon points=\"{}\" fill=\"{}\"/>", points, hex(facet.color));
    }

  size_t id{};
  for(disc_t const & disc: diagram.discs)
    {
    ++id;
    if(disc.outline)
      {
      shapes += std::format(
        "<circle cx=\"{:.2f}\" cy=\"{:.2f}\" r=\"{:.2f}\" fill=\"none\" stroke=\"{}\" stroke-width=\"0.8\"/>",
        disc.x,
        disc.y,
        disc.radius,
        hex(disc.color)
      );
      continue;
      }
    for(ball_ring_t const & ring: disc.rings)
      shapes += ring_half(disc, ring, false);

    float const length{std::max(std::hypot(disc.light_x, disc.light_y), 0.001f)};
    float const lit_x{0.5f + 0.32f * disc.light_x / length};
    float const lit_y{0.5f + 0.32f * disc.light_y / length};
    std::string ball;
    if(disc.sphere)
      ball += std::format("<a href=\"#b{}\">", disc.body);
    if(disc.glows)
      {
      defs += std::format(
        "<radialGradient id=\"g{0}\"><stop offset=\"0.6\" stop-color=\"{1}\"/><stop offset=\"1\" stop-color=\"{2}\"/>"
        "</radialGradient>",
        id,
        hex(disc.color),
        dimmed(disc.color, 0.75f)
      );
      ball += std::format("<circle cx=\"{:.2f}\" cy=\"{:.2f}\" r=\"{:.2f}\" fill=\"url(#g{})\"/>", disc.x, disc.y, disc.radius, id);
      }
    else if(not disc.sphere)
      ball += std::format("<circle cx=\"{:.2f}\" cy=\"{:.2f}\" r=\"{:.2f}\" fill=\"{}\"/>", disc.x, disc.y, disc.radius, hex(disc.color));
    else
      {
      // the night side keeps some of its colour, as on the overlay - the colour says what the body is
      if(disc.face.empty())
        {
        defs += std::format(
          "<radialGradient id=\"g{0}\" cx=\"{1:.2f}\" cy=\"{2:.2f}\" r=\"0.8\"><stop offset=\"0\" stop-color=\"{3}\"/>"
          "<stop offset=\"1\" stop-color=\"{4}\"/></radialGradient>",
          id,
          lit_x,
          lit_y,
          hex(disc.color),
          dimmed(disc.color, 0.28f)
        );
        ball += std::format("<circle cx=\"{:.2f}\" cy=\"{:.2f}\" r=\"{:.2f}\" fill=\"url(#g{})\"/>", disc.x, disc.y, disc.radius, id);
        }
      else
        {
        defs += std::format(
          "<clipPath id=\"c{0}\"><circle cx=\"{1:.2f}\" cy=\"{2:.2f}\" r=\"{3:.2f}\"/></clipPath>"
          "<radialGradient id=\"g{0}\" cx=\"{4:.2f}\" cy=\"{5:.2f}\" r=\"0.8\"><stop offset=\"0\" stop-color=\"#000\" "
          "stop-opacity=\"0\"/><stop offset=\"1\" stop-color=\"#000\" stop-opacity=\"0.72\"/></radialGradient>",
          id,
          disc.x,
          disc.y,
          disc.radius,
          lit_x,
          lit_y
        );
        ball += std::format(
          "<image href=\"{0}\" x=\"{1:.2f}\" y=\"{2:.2f}\" width=\"{3:.2f}\" height=\"{3:.2f}\" clip-path=\"url(#c{4})\"/>"
          "<circle cx=\"{5:.2f}\" cy=\"{6:.2f}\" r=\"{7:.2f}\" fill=\"url(#g{4})\"/>",
          escaped(disc.face),
          disc.x - disc.radius,
          disc.y - disc.radius,
          disc.radius * 2.f,
          id,
          disc.x,
          disc.y,
          disc.radius
        );
        }
      }
    if(disc.atmosphere_depth > 0.f)
      {
      float const reach{disc.radius * (1.f + disc.atmosphere_depth)};
      defs += std::format(
        "<radialGradient id=\"a{0}\"><stop offset=\"{1:.3f}\" stop-color=\"{2}\" stop-opacity=\"0\"/>"
        "<stop offset=\"{3:.3f}\" stop-color=\"{2}\" stop-opacity=\"0.6\"/>"
        "<stop offset=\"1\" stop-color=\"{2}\" stop-opacity=\"0\"/></radialGradient>",
        id,
        disc.radius / reach * 0.7f,
        hex(disc.atmosphere),
        disc.radius / reach
      );
      ball += std::format("<circle cx=\"{:.2f}\" cy=\"{:.2f}\" r=\"{:.2f}\" fill=\"url(#a{})\"/>", disc.x, disc.y, reach, id);
      }
    if(disc.sphere)
      ball += "</a>";
    shapes += ball;
    for(ball_ring_t const & ring: disc.rings)
      shapes += ring_half(disc, ring, true);
    }

  for(label_t const & label: diagram.labels)
    {
    if(label.text.empty())
      continue;
    shapes += std::format(
      "<text x=\"{:.2f}\" y=\"{:.2f}\" fill=\"{}\" font-size=\"{}\" text-anchor=\"{}\" dominant-baseline=\"middle\">{}</text>",
      label.x,
      label.y,
      hex(label.color),
      svg_label_size,
      label.align < 0.25f ? "start" : (label.align > 0.75f ? "end" : "middle"),
      escaped(label.text)
    );
    }

  return std::format(
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 {0:.1f} {1:.1f}\" width=\"{2:.0f}\" height=\"{3:.0f}\">"
    "<defs>{4}</defs>{5}</svg>",
    diagram.width,
    diagram.height,
    diagram.width * svg_scale,
    diagram.height * svg_scale,
    defs,
    shapes
  );
  }
  }  // namespace overlay
