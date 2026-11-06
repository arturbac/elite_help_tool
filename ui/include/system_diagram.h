#pragma once

#include <data/missions.h>
#include <data/navigation.h>
#include <data/station.h>
#include <events/companion_files.h>
#include <overlay_protocol.h>
#include <star_system.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

///\brief what stands for a body's face in the picture: the system, the body's full name and the colour for a
/// face whose views are all tinted; empty while there is none. The overlay takes a file, a page a link
using face_of_t = std::function<std::string(std::string const &, std::string const &, uint32_t)>;

///\brief where a mission's reward is owed: its system and its place
[[nodiscard]]
auto owed_place(info::mission_t const & mission) -> std::pair<std::string, std::string>;

///\brief numbers in names compare as numbers, so body 10 comes after body 9 - which is also a planet before its moons
[[nodiscard]]
auto natural_less(std::string_view a, std::string_view b) -> bool;

///\brief the bodies and ports of a system as a picture of plain shapes, for the overlay and for the album's pages
[[nodiscard]]
auto build_system_diagram(
  star_system_t const & system,
  std::span<info::station_t const> stations,
  std::string_view here,
  std::optional<events::status_file_t::destination_t> const & destination,
  std::span<info::mission_t const> missions,
  face_of_t const & face_of,
  bool exploring
) -> std::optional<overlay::diagram_t>;
