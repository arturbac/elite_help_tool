#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <map>
#include <string>

class QImage;

///\brief the faces of the bodies for the overlay's balls, made from the pictures the codex keeps
///
/// A body gets a face once the surface scanner has been on it: its views under codex/scanner, and the
/// sky album's photo of it for the colour, and the best view of it from the cockpit, kept in live.sqlite (see
/// face_store.h) - planet_face.h says how they are chosen and cut. A face is made in a thread of its own,
/// kept as a PNG under codex/faces for looking at - made again at every start, never read back - and
/// handed to the layer as a PPM
/// beside the socket - a new name each time the face is made again, since the layer reads a name once.
/// The pictures are looked at again every few seconds, so a view taken while the scanner is still open
/// shows on the map a little later.
class planet_faces_t final
  {
public:
  ///\param live_db where the views from the cockpit are kept
  explicit planet_faces_t(std::filesystem::path live_db);

  ///\brief the PPM of the body's face, or empty while there is none; the first call for a body starts
  /// making one
  ///\param fallback the colour for a face whose views are all tinted by a filter and whose photo is missing
  [[nodiscard]]
  auto face(std::string const & system, std::string const & body, uint32_t fallback) -> std::string;

  ///\brief a view of the body from the cockpit, judged in a thread of its own and kept as the body's cockpit
  /// face when it is at least as good as the one kept - the view comes while one is still judged is let go
  auto offer_view(std::string const & system, std::string const & body, QImage view) -> void;

  ///\brief how good the best view from the cockpit kept of the body was, -1 for none - read on a connection of
  /// its own, so any thread may ask
  [[nodiscard]]
  static auto best_score(std::filesystem::path const & live_db, std::string const & body) -> float;

  [[nodiscard]]
  auto live_db() const -> std::filesystem::path const &
    { return live_db_; }

  ///\brief the side of a face in pixels - the layer's atlas holds squares of this size
  static constexpr uint32_t side{128u};

private:
  struct entry_t
    {
    ///\brief what the layer is given
    std::string spool;
    ///\brief the newest picture the face was made from - min, not a default, which is the file clock's
    /// epoch and in libstdc++ lies in 2174
    std::filesystem::file_time_type made_from{std::filesystem::file_time_type::min()};
    std::chrono::steady_clock::time_point checked;
    std::future<std::string> making;
    std::filesystem::file_time_type making_from;
    };

  std::filesystem::path live_db_;
  std::map<std::string, entry_t> entries_;
  std::future<void> judging_;
  };
