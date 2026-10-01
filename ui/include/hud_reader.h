#pragma once

#include <hot_drop.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

///\brief reads the target's label off pictures of the middle of the screen, on a thread of its own
///\detail the layer writes the picture asked for under its name only once it is whole; the reader waits for it,
/// keeps the bright pixels - the HUD's text - reads the lines with tesseract and finds the label of the target
/// among them. The picture is deleted once read. A reading takes a tenth of a second or so; when newer pictures
/// wait behind one, it is deleted unread - a reading of the past is of no use
class hud_reader_t
  {
public:
  struct job_t
    {
    std::filesystem::path path;
    ///\brief when the picture was asked for - the moment of its reading
    uint64_t ms{};
    std::string target;
    };

  struct result_t
    {
    uint64_t ms{};
    std::string target;
    std::optional<hot_drop::label_t> label;
    };

  hud_reader_t();
  ~hud_reader_t();
  hud_reader_t(hud_reader_t const &) = delete;
  auto operator=(hud_reader_t const &) -> hud_reader_t & = delete;

  ///\brief built with tesseract, and its English data found
  [[nodiscard]]
  static auto available() noexcept -> bool;

  auto submit(job_t job) -> void;

  ///\brief the readings done since the last call
  [[nodiscard]]
  auto take() -> std::vector<result_t>;

private:
  auto work(std::stop_token stop) -> void;

  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::deque<job_t> jobs_;
  std::vector<result_t> done_;
  std::jthread worker_;
  };
