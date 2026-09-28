#pragma once

#include <overlay_protocol.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

///\brief pictures of the stars and planets, taken without asking while the ship looks at them anyway
///
/// Out of the jump the ship faces the arrival star, and after the surface scanner it still faces the
/// planet it mapped. At those two moments the middle of the screen is a portrait, so it is taken quietly -
/// no frame, no countdown - once per star and once per planet, and kept under the codex directory with a
/// page of its own. Nothing is asked of the player, and the pictures are of this commander's own flight.
class sky_album_t final
  {
public:
  ///\brief what a picture shows
  struct entry_t
    {
    std::string file;
    std::string taken;
    ///\brief star or planet
    std::string kind;
    std::string system;
    std::string body;
    ///\brief the star's class or the planet's, and whatever else is worth a line under the picture
    std::string detail;
    ///\brief nobody had scanned it before
    bool first{};
    };

  ///\brief a picture wanted after the given delay, unless one of this body is kept already - the moment the
  /// ship turns away is when it is called off
  auto ask(entry_t subject, std::chrono::milliseconds after) -> void;

  ///\brief a series of pictures of one subject, each the given time from now, each kept under its own name - to see
  /// at which moment the view is worth it, before the moment is settled on
  auto ask_series(entry_t subject, std::span<std::chrono::milliseconds const> after) -> void;

  ///\brief what the pictures of a system's star show, once the star's scan has come - a few seconds after the
  /// jump, when the first pictures may be taken already
  auto describe(std::string const & system, entry_t const & known) -> void;

  ///\brief called off - the ship turned away, dropped out, or the view is a menu now
  auto call_off() -> void { due_.clear(); }

  ///\brief sends the request when its time comes, provided the view is still the ship's own
  auto tick(bool view_clear) -> void;

  ///\brief files the picture the layer has finished
  ///\returns true when one was kept
  auto collect() -> bool;

  ///\brief what goes into the frame - the newest request, or none
  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

  ///\brief where the page is, for opening it from the tool
  [[nodiscard]]
  static auto page_path() -> std::filesystem::path;

private:
  struct due_t
    {
    entry_t subject;
    std::chrono::steady_clock::time_point at;
    ///\brief put after the body's name in the file's, so the pictures of a series do not overwrite each other
    std::string suffix;
    };
  struct pending_t
    {
    std::filesystem::path spool;
    entry_t subject;
    std::string suffix;
    std::chrono::steady_clock::time_point asked;
    };

  overlay::capture_t request_;
  std::deque<due_t> due_;
  std::optional<pending_t> pending_;
  std::vector<entry_t> entries_;
  bool loaded_{};

  auto load() -> void;
  auto save() const -> void;
  auto write_page() const -> void;
  [[nodiscard]]
  auto kept(std::string const & body) -> bool;
  };
