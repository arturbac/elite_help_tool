#pragma once

#include <overlay_protocol.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace overlay
  {
///\brief a frame together with the moment it arrived - block expiry needs it
struct received_frame_t
  {
  frame_t frame;
  std::chrono::steady_clock::time_point at;
  };

///\brief the game-process side - reads in the background, the present thread only ever gets a ready pointer
///
/// nothing here may block the present thread, because that thread counts the game's frames. the socket
/// and the parsing live in a thread of their own, and snapshot() is a refcount bump and nothing more
class client_t
  {
public:
  explicit client_t(std::string socket_path);
  client_t(client_t const &) = delete;
  auto operator=(client_t const &) -> client_t & = delete;
  ~client_t();

  ///\brief the last complete frame, or an empty pointer while nothing has arrived yet
  [[nodiscard]]
  auto snapshot() const -> std::shared_ptr<received_frame_t const>;

  [[nodiscard]]
  auto connected() const noexcept -> bool;

  ///\brief how many frames arrived since start - for diagnostics
  [[nodiscard]]
  auto received() const noexcept -> uint64_t;

private:
  auto run() -> void;

  std::string socket_path_;
  std::atomic<std::shared_ptr<received_frame_t const>> latest_{};
  std::atomic<bool> connected_{false};
  std::atomic<uint64_t> received_{};
  int wakeup_fd_{-1};
  std::thread worker_;
  };

///\brief the elite_help_tool side - sends frames out; publish() never blocks the calling thread
class server_t
  {
public:
  explicit server_t(std::string socket_path);
  server_t(server_t const &) = delete;
  auto operator=(server_t const &) -> server_t & = delete;
  ~server_t();

  [[nodiscard]]
  auto listening() const noexcept -> bool;

  ///\brief the socket the server bound to - on failure the only hint at what to fix
  [[nodiscard]]
  auto path() const noexcept -> std::string_view;

  ///\brief the number of connected games
  [[nodiscard]]
  auto clients() const noexcept -> unsigned;

  ///\brief serializes once and leaves it to the io thread; a frame not yet sent is replaced by the new one
  ///
  /// the last frame is kept and handed to every new client. the game usually starts later than
  /// the tool, so without it the overlay would sit empty until the next change
  auto publish(frame_t const & frame) -> void;

private:
  struct peer_t
    {
    int fd{-1};
    std::string outbox;
    size_t sent{};
    };

  auto run() -> void;

  std::string socket_path_;
  int listen_fd_{-1};
  int wakeup_fd_{-1};
  std::atomic<bool> stopping_{false};
  std::atomic<unsigned> client_count_{};
  std::mutex peers_mutex_;
  std::vector<peer_t> peers_;
  ///\brief the last image sent, ready to hand to whoever connects next
  std::string retained_;
  std::thread worker_;
  };
  }  // namespace overlay
