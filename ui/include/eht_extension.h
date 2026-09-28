#pragma once

#include <elite_data.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>

///\brief an optional extension, built in from outside this repository
///
/// The tool runs the same without one: a build that names no extension directory gets the empty
/// implementation, which does nothing at all. An extension hears what the tool hears - every journal
/// line and the route being flown - and keeps whatever it does to itself.
namespace eht::extension
  {
///\brief the route being flown, as the Route window knows it
struct route_view_t
  {
  std::span<info::neutron_waypoint_t const> waypoints;
  ///\brief how many waypoints are behind us
  size_t reached{};
  };

class extension_t
  {
public:
  extension_t() = default;
  extension_t(extension_t const &) = delete;
  auto operator=(extension_t const &) -> extension_t & = delete;
  virtual ~extension_t() = default;

  ///\brief every journal line as written, with whether it is happening now - from the journal thread
  virtual auto journal_line(std::string_view line, bool live) -> void = 0;

  ///\brief now and then, from the GUI thread, with the route as it stands
  virtual auto tick(route_view_t const & route) -> void = 0;

  ///\brief whether the extension takes a destination the commander asks for in the Route window; without
  /// it the window puts the system on the clipboard
  [[nodiscard]]
  virtual auto sets_destination() const -> bool { return false; }

  ///\brief the commander asked for this system as the destination, from the GUI thread
  virtual auto set_destination(std::string const & system) -> void { (void)system; }
  };

///\brief the extension built in, or nullptr when there is none
[[nodiscard]]
auto make_extension(std::filesystem::path journal_dir) -> std::unique_ptr<extension_t>;
  }  // namespace eht::extension
