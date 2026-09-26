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
///\brief ramka razem z chwila odbioru - bez niej nie da sie zrealizowac wygasania blokow
struct received_frame_t
  {
  frame_t frame;
  std::chrono::steady_clock::time_point at;
  };

///\brief strona w procesie gry - czyta w tle, watek prezentacji dostaje wylacznie gotowy wskaznik
///
/// nic tu nie moze zablokowac watku prezentacji, bo to on liczy klatki gry. calosc gniazda
/// i parsowania siedzi w osobnym watku, a snapshot() to podbicie licznika referencji i nic wiecej
class client_t
  {
public:
  explicit client_t(std::string socket_path);
  client_t(client_t const &) = delete;
  auto operator=(client_t const &) -> client_t & = delete;
  ~client_t();

  ///\brief ostatnia kompletna ramka albo pusty wskaznik gdy jeszcze nic nie przyszlo
  [[nodiscard]]
  auto snapshot() const -> std::shared_ptr<received_frame_t const>;

  [[nodiscard]]
  auto connected() const noexcept -> bool;

  ///\brief ile ramek dotarlo od startu - do diagnostyki
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

///\brief strona w elite_help_tool - rozsyla ramki, publish() nigdy nie blokuje watku wolajacego
class server_t
  {
public:
  explicit server_t(std::string socket_path);
  server_t(server_t const &) = delete;
  auto operator=(server_t const &) -> server_t & = delete;
  ~server_t();

  [[nodiscard]]
  auto listening() const noexcept -> bool;

  ///\brief gniazdo na ktorym stanal serwer - przy niepowodzeniu jedyna wskazowka co poprawic
  [[nodiscard]]
  auto path() const noexcept -> std::string_view;

  ///\brief liczba podlaczonych gier
  [[nodiscard]]
  auto clients() const noexcept -> unsigned;

  ///\brief serializuje raz i zostawia watkowi io; ramka jeszcze niewyslana jest zastepowana nowa
  ///
  /// ostatnia ramka zostaje zapamietana i trafia do kazdego nowego klienta. gra startuje zwykle
  /// pozniej niz narzedzie, wiec bez tego overlay swiecilby pustka az do najblizszej zmiany
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
  ///\brief ostatni wyslany obraz, gotowy do podania nowo podlaczonym
  std::string retained_;
  std::thread worker_;
  };
  }  // namespace overlay
