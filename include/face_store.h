#pragma once

#include <planet_face.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

///\brief the best view of each planet taken from the cockpit, kept in live.sqlite
///
/// Of everything a planet's face is made from, this view is the one thing nothing can give back: the
/// scanner's views and the sky album's photos are pictures of the codex, and the face itself is made
/// again from them at every start - but the ship will not stand before that planet in that light again.
/// So the view and how good it was sit in one row of live.sqlite, written together or not at all, and go
/// wherever the copy of live.sqlite goes. The pixels are plain RGB packed with zstd.
///
/// Each call opens a connection of its own, so the threads that judge and make faces need share nothing.
namespace face_store
  {
struct view_t
  {
  float score{};
  planet_face::image_t image;
  ///\brief when it was kept - a face made from an older view is made again
  std::chrono::system_clock::time_point at;
  };

///\brief keeps the view as the body's, in place of any before it
[[nodiscard]]
auto save(std::filesystem::path const & live_db, std::string const & body, float score, planet_face::image_t const & image)
  -> bool;

[[nodiscard]]
auto load(std::filesystem::path const & live_db, std::string const & body) -> std::optional<view_t>;

///\brief how good the kept view was and when it was kept, without the pixels; none for a body without one
[[nodiscard]]
auto stamp(std::filesystem::path const & live_db, std::string const & body)
  -> std::optional<std::pair<float, std::chrono::system_clock::time_point>>;
  }  // namespace face_store
