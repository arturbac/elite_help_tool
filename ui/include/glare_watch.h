#pragma once

#include <glare.h>
#include <overlay_protocol.h>

#include <QImage>

#include <chrono>
#include <filesystem>
#include <future>
#include <optional>
#include <vector>

///\brief watches for the white glare of a settlement's rooms and writes each one down - see glare.h
///
/// Inside a settlement a patch of the middle screen is taken quietly every few seconds and measured on a small
/// grey copy. A patch nearly all burnt out has the whole middle screen taken at once, and that is kept as a
/// PNG with a marker beside it in the evidence directory; then the watch waits for the screen to darken again before the next marker, so one
/// white room makes one marker, not one every few seconds. The PNG is written in a thread of its own - the
/// middle screen takes a moment to pack - and the marker only once the picture is complete
class glare_watch_t final
  {
public:
  glare_watch_t() = default;
  glare_watch_t(glare_watch_t const &) = delete;
  auto operator=(glare_watch_t const &) -> glare_watch_t & = delete;
  ///\brief waits for a marker being written - half of one is worse than a slower exit
  ~glare_watch_t();

  ///\brief asks for a picture now and then while the commander stands inside a settlement
  auto observe(bool inside_settlement) -> void;

  ///\brief what goes into the frame - the newest request, or none
  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

  ///\brief measures the picture the layer has finished, if there is one, and writes the marker for a glare
  auto collect(glare::game_t const & game) -> void;

private:
  struct pending_t
    {
    std::filesystem::path spool;
    std::chrono::system_clock::time_point taken;
    std::chrono::steady_clock::time_point asked;
    ///\brief the whole middle screen, taken for a glare already measured
    bool full{};
    };

  overlay::capture_t request_;
  std::optional<pending_t> pending_;
  std::chrono::steady_clock::time_point last_asked_{};
  ///\brief the last marker, and whether the screen has darkened since
  std::chrono::steady_clock::time_point marked_{};
  bool darkened_{true};
  ///\brief a glare measured, waiting for its whole picture
  std::optional<glare::marker_t> found_;
  std::vector<std::future<void>> work_;

  ///\brief writes the marker found, with its picture when there is one
  auto write(std::optional<QImage> image) -> void;
  };
