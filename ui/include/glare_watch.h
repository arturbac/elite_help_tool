#pragma once

#include <glare.h>
#include <overlay_protocol.h>

#include <QImage>

#include <chrono>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
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

  ///\brief where the game keeps what a report takes besides the evidence directory's own logs
  struct places_t
    {
    std::filesystem::path journal_dir;
    ///\brief the Logs directory with the netLog files; empty when not known
    std::filesystem::path netlog_dir;
    };

  ///\brief measures the picture the layer has finished, if there is one, and writes the marker for a glare; writes
  /// the report of a marker once the minutes after it are in the logs too
  auto collect(glare::game_t const & game, places_t const & places) -> void;

  ///\brief the last glare found, as the player's clock says it - for the word on the overlay that the watch works
  struct noticed_t
    {
    std::string local_time;
    std::chrono::steady_clock::time_point at;
    };

  [[nodiscard]]
  auto noticed() const noexcept -> std::optional<noticed_t> const &
    { return noticed_; }

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
  std::chrono::system_clock::time_point found_at_{};
  ///\brief a report to write once its window has passed - the marker and the picture are read from their files then
  struct report_t
    {
    std::filesystem::path evidence_dir;
    std::filesystem::path marker;
    std::filesystem::path picture;
    ///\brief the promise of the report on disk, removed once it is written
    std::filesystem::path due_file;
    std::chrono::system_clock::time_point moment;
    std::chrono::steady_clock::time_point due;
    ///\brief taken up after a restart, perhaps before the game runs - not written without the game's netLog
    bool waits_for_netlog{};
    };
  std::vector<report_t> reports_;
  ///\brief the reports promised before the last restart taken back up
  bool recovered_{};
  std::optional<noticed_t> noticed_;
  std::vector<std::future<void>> work_;

  ///\brief writes the marker found, with its picture when there is one
  auto write(std::optional<QImage> image) -> void;

  ///\brief the reports a stopped tool promised and never wrote, put back in the queue - once, at the first collect
  auto recover() -> void;

  ///\brief the reports whose window has passed, written in the background
  auto write_due_reports(places_t const & places) -> void;
  };
