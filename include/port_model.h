#pragma once

#include <array>
#include <cstdint>
#include <vector>

///\brief small models of the orbital ports for the picture of the system
///
/// Each kind of port is a handful of plain solids, the shape the game gives it at a glance: the Coriolis a
/// cuboctahedron, the Dodec a dodecahedron, the Ocellus a large ball on a spindle, the Orbis (Artemis and
/// Apollo alike) a small core in a large ring, an outpost a few boxes on a spine, an asteroid base a lump of
/// rock. The model is turned to a fixed view from a little above, lit from the star and flattened here, so
/// what goes to the layer is only filled polygons in the order they are to be painted.
namespace port_model
  {
enum struct kind_e : uint8_t
  {
  coriolis,
  orbis,
  ocellus,
  dodec,
  outpost,
  asteroid
  };

///\brief what a polygon is a piece of - the caller gives each part its colour
enum struct part_e : uint8_t
  {
  hull,
  ///\brief the docking slot, and whatever else is an opening rather than a surface
  slot,
  rock
  };

///\brief one flat polygon of the model on the picture, convex and clockwise with y growing downwards
struct facet_t
  {
  ///\brief offsets from the model's centre, the model filling a circle of the radius asked for
  std::vector<std::array<float, 2>> points;
  part_e part{part_e::hull};
  ///\brief how much light the polygon catches, 0 in shadow to 1 facing the star
  float shade{};
  };

///\brief the model's polygons, farthest first
///\param light_x, light_y towards the star on the picture, y growing downwards; any length, 0 0 lights it from the front
[[nodiscard]]
auto facets(kind_e kind, float radius, float light_x, float light_y) -> std::vector<facet_t>;
  }  // namespace port_model
