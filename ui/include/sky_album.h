#pragma once

#include <overlay_protocol.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <vector>

class QImage;

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

  ///\brief a picture of the body taken now and then while it may be the next one scanned - kept aside, not in
  /// the album, until the scanner opens on it: the view just before the scanner is the one worth having
  auto offer(entry_t subject) -> void;

  ///\brief the scanner opened - the last picture offered of this body is the one, if it is fresh
  auto hold(std::string const & body) -> void;

  ///\brief the scanner mapped the body - the picture held goes into the album, named after the mapping's moment
  ///\returns true when there was one
  auto keep_held(std::string const & body, std::chrono::sys_seconds moment) -> bool;

  ///\brief sends the request when its time comes, provided the view is still the ship's own
  auto tick(bool view_clear) -> void;

  ///\brief files the picture the layer has finished
  ///\returns true when one was kept
  auto collect() -> bool;

  ///\brief what goes into the frame - the newest request, or none
  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

  ///\brief where the journals are - what a picture missing from the list is described out of again
  auto set_journal_dir(std::filesystem::path dir) -> void { journal_dir_ = std::move(dir); }

  ///\brief reads the list at once, describing again what it lacks - so a lost list is back before the first jump
  auto open() -> void
    {
    if(not loaded_)
      load();
    }

  ///\brief the file a picture of the body is kept in, named after the journal's moment it was taken for
  auto set_moment(std::chrono::sys_seconds at) -> void { moment_ = at; }

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
    ///\brief the journal's moment the picture is for - the jump, the scanner - which names its file
    std::chrono::sys_seconds moment;
    };
  struct pending_t
    {
    std::filesystem::path spool;
    entry_t subject;
    std::string suffix;
    std::chrono::sys_seconds moment;
    std::chrono::steady_clock::time_point asked;
    ///\brief a picture offered, kept aside rather than put into the album
    bool candidate{};
    };
  struct aside_t
    {
    ///\brief shared rather than held, so this header needs no Qt of its own
    std::shared_ptr<QImage const> image;
    entry_t subject;
    std::chrono::steady_clock::time_point at;
    };

  overlay::capture_t request_;
  std::deque<due_t> due_;
  std::optional<pending_t> pending_;
  std::optional<aside_t> candidate_;
  std::optional<aside_t> held_;
  std::vector<entry_t> entries_;
  bool loaded_{};
  std::filesystem::path journal_dir_;
  std::chrono::sys_seconds moment_{};

  auto load() -> void;
  ///\brief puts a picture into the album
  auto keep(QImage const & image, entry_t subject, std::string const & suffix, std::chrono::sys_seconds moment) -> bool;
  auto request(std::chrono::steady_clock::time_point now) -> overlay::capture_t;
  auto describe_unlisted() -> void;
  auto save() const -> void;
  auto write_page() const -> void;
  [[nodiscard]]
  auto kept(std::string const & body) -> bool;
  };
