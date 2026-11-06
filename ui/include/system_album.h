#pragma once

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

///\brief the sky album as pages: an index of the known systems (sky.html) and a page for each, out of the database
///
/// The pages are made in a thread of its own with a connection of its own, at a low priority, since the game is
/// usually running. A page carries in its first line what it was made from, so a pass over all the systems writes
/// only those that changed, and the first pass is the only long one. Each account writes pages only into its own
/// codex directory, for the systems in its own database: it reads the other accounts' pictures and finds, and lists
/// the systems only they know with a link to their page
class system_album_t final
  {
public:
  explicit system_album_t(std::string db_path);
  ~system_album_t();
  system_album_t(system_album_t const &) = delete;
  auto operator=(system_album_t const &) -> system_album_t & = delete;

  ///\brief the journals the regions are read from, once
  auto set_journal_dir(std::filesystem::path dir) -> void;

  ///\brief a pass over the systems, as soon as the thread is free
  auto request() -> void;

private:
  auto run(std::stop_token stop) -> void;
  auto pass(std::stop_token const & stop) -> void;

  std::string db_path_;
  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::filesystem::path journal_dir_;
  bool requested_{};
  bool regions_read_{};
  std::jthread worker_;
  };
