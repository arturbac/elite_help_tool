#pragma once

#include <overlay_protocol.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

///\brief the surface scanner's views of a body, kept to be looked at again from the ground
///
/// The journal says which genera grow on a mapped body but never where. The scanner shows it - each
/// filter paints the regions of one genus - and it shows it from orbit, while the samples are taken
/// from a Nomad far below. So while the scanner is open its view is photographed every second, and
/// a view unlike any kept of that body is kept as another: switching the filter or the side of the
/// planet makes one, the planet turning slowly under the ship only freshens the one already there.
/// The pictures carry the filter's name themselves, so nothing has to know which filter was on.
/// They live under the codex directory, one directory per body; the thumbnails the layer draws are
/// made beside the socket, where the game's container can read them.
class scanner_sheet_t final
  {
public:
  ///\brief asks for a picture while the scanner is open on a body, at most once an interval
  auto observe(bool scanner_open, std::string const & body) -> void;

  ///\brief what goes into the frame - the newest request, or none
  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

  ///\brief files the picture the layer has finished, if there is one
  auto collect() -> void;

  ///\brief the thumbnails of a body's views, in the order they were first seen
  [[nodiscard]]
  auto pictures(std::string const & body) -> std::vector<overlay::picture_t>;

private:
  struct view_t
    {
    std::filesystem::path file;
    ///\brief a tiny grey copy, what two views are told apart by
    std::vector<uint8_t> signature;
    ///\brief the thumbnail for the layer, made when first wanted - a new name each time the view changes
    std::filesystem::path thumbnail;
    uint32_t generation{};
    };

  struct pending_t
    {
    std::filesystem::path spool;
    std::string body;
    std::chrono::steady_clock::time_point asked;
    };

  overlay::capture_t request_;
  std::optional<pending_t> pending_;
  std::chrono::steady_clock::time_point last_asked_{};
  std::map<std::string, std::vector<view_t>> views_;

  ///\brief the views of a body, read from its directory the first time the body comes up
  auto views_of(std::string const & body) -> std::vector<view_t> &;
  };
