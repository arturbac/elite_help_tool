#pragma once

#include <overlay_protocol.h>
#include <planet_face.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <optional>
#include <string>

class planet_faces_t;

///\brief views of a planet taken on the way to it, for its face on the system map
///
/// With a planet of this system set as the destination and the ship's own view in supercruise, the layer
/// is asked for its small sample of the middle of the screen - shrunk on the graphics card, a few hundred
/// pixels across, kept fresh in a file both share. Every new sample is judged in a thread: how much of the
/// ball is in daylight, how large it is on the screen, how little of the HUD and the cockpit's frame is
/// over it. Only when a sample is at least as good as the best face kept is a real picture asked for, and
/// only of the rectangle the ball stands in - a few on the whole way in, instead of one every second or two.
/// The real picture is judged again at its own size and kept by planet_faces_t when it holds.
class approach_views_t final
  {
public:
  ///\brief what flying at the body is; an empty body is none, and the sample is no longer wanted
  auto observe(bool ship_view, std::string const & system, std::string const & body) -> void;

  ///\brief reads a new sample, judges it, asks for a picture when it is worth one, and hands a finished
  /// picture to the faces
  auto tick(planet_faces_t & faces) -> void;

  ///\brief what the layer is asked to keep
  [[nodiscard]]
  auto sample_request() const -> overlay::sample_t;

  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

private:
  struct target_t
    {
    std::string system;
    std::string body;
    };

  struct pending_t
    {
    std::filesystem::path spool;
    target_t target;
    std::chrono::steady_clock::time_point asked;
    };

  ///\brief what a sample told: the ball's rectangle on the surface when it is worth a picture
  struct verdict_t
    {
    target_t target;
    float left{};
    float top{};
    float width{};
    float height{};
    };

  std::optional<target_t> target_;
  overlay::capture_t request_;
  std::optional<pending_t> pending_;
  uint64_t seen_seq_{};
  std::future<std::optional<verdict_t>> judging_;
  std::chrono::steady_clock::time_point last_asked_{};
  };
