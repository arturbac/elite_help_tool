#pragma once

#include <overlay_protocol.h>

#include <QImage>

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
  ///\brief asks for a picture while the scanner looks at a body from the ship, at most once an interval
  auto observe(bool scanner_open, std::string const & body) -> void;

  ///\brief what goes into the frame - the newest request, or none
  [[nodiscard]]
  auto capture_request() const -> overlay::capture_t const &
    { return request_; }

  ///\brief files the picture the layer has finished once the scanner has stayed open a while after it - closed
  /// before that, the picture shows the view fading out, or the cockpit, and is dropped
  auto collect(bool scanner_open) -> void;

  ///\brief the thumbnails of a body's views, in the order they were first seen
  [[nodiscard]]
  auto pictures(std::string const & body) -> std::vector<overlay::picture_t>;

  ///\brief what the last picture did - the player switching filters wants to know the view is in
  struct news_t
    {
    std::string body;
    ///\brief the view's number, from 1, in the order they were first seen
    size_t view{};
    ///\brief a view not kept before, rather than a known one taken again
    bool fresh{};
    std::chrono::steady_clock::time_point at;
    };

  [[nodiscard]]
  auto news() const noexcept -> std::optional<news_t> const &
    { return news_; }

  ///\brief how many views of a body are kept
  [[nodiscard]]
  auto view_count(std::string const & body) -> size_t
    { return views_of(body).size(); }

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

  ///\brief a picture come from the layer, waiting to see the scanner still open
  struct held_t
    {
    QImage image;
    std::string body;
    std::chrono::steady_clock::time_point came;
    };

  overlay::capture_t request_;
  std::optional<pending_t> pending_;
  std::optional<held_t> held_;
  std::chrono::steady_clock::time_point last_asked_{};
  std::map<std::string, std::vector<view_t>> views_;
  std::optional<news_t> news_;

  ///\brief the views of a body, read from its directory the first time the body comes up
  auto views_of(std::string const & body) -> std::vector<view_t> &;

  ///\brief keeps a picture as a new view of the body, or in place of the view it is like
  auto file(QImage const & image, std::string const & body) -> void;
  };
