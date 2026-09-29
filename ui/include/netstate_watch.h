#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>

///\brief the state of the game's network connections written down every couple of seconds - see netstate.h
///\detail only while evidence.dir is set and the game runs; in a thread of its own, the game's process looked
/// for again every few seconds while there is none
class netstate_watch_t final
  {
public:
  netstate_watch_t();
  netstate_watch_t(netstate_watch_t const &) = delete;
  auto operator=(netstate_watch_t const &) -> netstate_watch_t & = delete;
  ~netstate_watch_t() = default;

  ///\brief the journals' directory - the game watched is the one whose Wine prefix holds it
  auto set_journal_dir(std::filesystem::path dir) -> void;

  ///\brief the game's process while one is watched
  [[nodiscard]]
  auto game() const -> std::optional<int>;

private:
  mutable std::mutex mutex_;
  std::filesystem::path journal_dir_;
  std::optional<int> pid_;
  std::jthread worker_;
  };
