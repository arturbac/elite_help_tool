#pragma once

#include <overlay_protocol.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

class planet_faces_t;

///\brief views of a planet taken on the way to it, for its face on the system map
///
/// With a planet of this system set as the destination and the ship's own view in supercruise, the
/// middle of the screen is taken quietly now and then. The ship flies at it, so it is mostly in the
/// picture; each picture is judged - how much of the ball is in daylight, how large, how little of the
/// HUD and the cockpit's frame is over it - and kept when it is at least as good as the best so far.
/// The judging is planet_faces_t's; this only asks for the pictures and hands them over.
class approach_views_t final
  {
public:
  ///\brief asks for a picture while flying at the body, at most once an interval; an empty body is none
  auto observe(bool ship_view, std::string const & system, std::string const & body) -> void;

  ///\brief hands the picture the layer has finished to the faces
  auto collect(planet_faces_t & faces) -> void;

  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

private:
  struct pending_t
    {
    std::filesystem::path spool;
    std::string system;
    std::string body;
    std::chrono::steady_clock::time_point asked;
    };

  overlay::capture_t request_;
  std::optional<pending_t> pending_;
  std::chrono::steady_clock::time_point last_asked_{};
  };
