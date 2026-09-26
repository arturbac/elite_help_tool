#include <overlay_ipc.h>

#include <glaze/glaze.hpp>

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>

namespace overlay
  {
namespace
  {
  ///\brief ramka to 4 bajty dlugosci little endian i json, nic wiecej nie jest potrzebne
  constexpr size_t header_size{4u};
  ///\brief gornia granica zdrowego rozsadku - powyzej niej strumien jest rozjechany i zrywamy polaczenie
  constexpr uint32_t max_message_size{256u * 1024u};
  constexpr int reconnect_delay_ms{1000};

  auto set_non_blocking(int fd) -> bool
    {
    int const flags{::fcntl(fd, F_GETFL, 0)};
    return flags >= 0 and ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
    }

  [[nodiscard]]
  auto fill_address(sockaddr_un & addr, std::string const & path) -> bool
    {
    if(path.size() + 1u > sizeof(addr.sun_path))
      return false;

    addr = {};
    addr.sun_family = AF_UNIX;
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1u);
    return true;
    }

  [[nodiscard]]
  auto encode(frame_t const & frame) -> std::string
    {
    std::string payload;
    if(auto const err{glz::write_json(frame, payload)}; err)
      return {};

    if(payload.size() > max_message_size)
      return {};

    auto const size{static_cast<uint32_t>(payload.size())};
    std::string out;
    out.reserve(header_size + payload.size());
    for(unsigned shift{}; shift != 32u; shift += 8u)
      out.push_back(static_cast<char>((size >> shift) & 0xffu));
    out += payload;
    return out;
    }

  [[nodiscard]]
  auto decode_size(char const * header) -> uint32_t
    {
    uint32_t size{};
    for(unsigned byte{}; byte != 4u; ++byte)
      size |= static_cast<uint32_t>(static_cast<unsigned char>(header[byte])) << (byte * 8u);
    return size;
    }

  ///\brief obudzenie watku io - jeden licznik eventfd wystarczy i na nowa ramke i na koniec pracy
  auto signal(int fd) -> void
    {
    uint64_t const one{1u};
    [[maybe_unused]]
    auto const ignored{::write(fd, &one, sizeof(one))};
    }

  auto drain(int fd) -> void
    {
    uint64_t value{};
    [[maybe_unused]]
    auto const ignored{::read(fd, &value, sizeof(value))};
    }

  auto ensure_parent_directory(std::string const & path) -> void
    {
    std::error_code ec{};
    std::filesystem::create_directories(std::filesystem::path{path}.parent_path(), ec);
    }
  }  // namespace

client_t::client_t(std::string socket_path) : socket_path_{std::move(socket_path)}
  {
  wakeup_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  worker_ = std::thread{[this] { run(); }};
  }

client_t::~client_t()
  {
  if(wakeup_fd_ >= 0)
    signal(wakeup_fd_);
  if(worker_.joinable())
    worker_.join();
  if(wakeup_fd_ >= 0)
    ::close(wakeup_fd_);
  }

[[nodiscard]]
auto client_t::snapshot() const -> std::shared_ptr<received_frame_t const>
  { return latest_.load(std::memory_order_acquire); }

[[nodiscard]]
auto client_t::connected() const noexcept -> bool
  { return connected_.load(std::memory_order_relaxed); }

[[nodiscard]]
auto client_t::received() const noexcept -> uint64_t
  { return received_.load(std::memory_order_relaxed); }

auto client_t::run() -> void
  {
  std::string buffer;
  std::array<char, 8192> chunk{};

  while(true)
    {
      // proba polaczenia - narzedzie moze wystartowac pozniej niz gra, wiec probujemy w kolko
      {
      int const fd{::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)};
      sockaddr_un address{};
      bool linked{false};

      if(fd >= 0 and fill_address(address, socket_path_))
        linked = ::connect(fd, reinterpret_cast<sockaddr const *>(&address), sizeof(address)) == 0;

      if(not linked)
        {
        if(fd >= 0)
          ::close(fd);

        pollfd wait{.fd = wakeup_fd_, .events = POLLIN, .revents = 0};
        if(::poll(&wait, 1, reconnect_delay_ms) > 0 and (wait.revents & POLLIN) != 0)
          return;
        continue;
        }

      set_non_blocking(fd);
      connected_.store(true, std::memory_order_relaxed);
      buffer.clear();

      bool stop{false};
      while(not stop)
        {
        std::array<pollfd, 2> fds{
          pollfd{.fd = fd, .events = POLLIN, .revents = 0}, pollfd{.fd = wakeup_fd_, .events = POLLIN, .revents = 0}
        };

        if(::poll(fds.data(), fds.size(), -1) < 0)
          {
          if(errno == EINTR)
            continue;
          break;
          }

        if((fds[1].revents & POLLIN) != 0)
          {
          ::close(fd);
          connected_.store(false, std::memory_order_relaxed);
          return;
          }

        if((fds[0].revents & (POLLIN | POLLHUP | POLLERR)) == 0)
          continue;

        auto const got{::read(fd, chunk.data(), chunk.size())};
        if(got < 0)
          {
          if(errno == EAGAIN or errno == EWOULDBLOCK or errno == EINTR)
            continue;
          break;
          }
        if(got == 0)
          break;

        buffer.append(chunk.data(), static_cast<size_t>(got));

        while(buffer.size() >= header_size)
          {
          auto const size{decode_size(buffer.data())};
          if(size > max_message_size)
            {
            stop = true;
            break;
            }
          if(buffer.size() < header_size + size)
            break;

          auto parsed{std::make_shared<received_frame_t>()};
          std::string_view const payload{buffer.data() + header_size, size};
          if(auto const err{glz::read<glz::opts{.error_on_unknown_keys = false}>(parsed->frame, payload)}; not err)
            {
            parsed->at = std::chrono::steady_clock::now();
            latest_.store(std::shared_ptr<received_frame_t const>{std::move(parsed)}, std::memory_order_release);
            received_.fetch_add(1u, std::memory_order_relaxed);
            }

          buffer.erase(0, header_size + size);
          }
        }

      ::close(fd);
      connected_.store(false, std::memory_order_relaxed);
      }
    }
  }

server_t::server_t(std::string socket_path) : socket_path_{std::move(socket_path)}
  {
  ensure_parent_directory(socket_path_);

  wakeup_fd_ = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

  sockaddr_un address{};
  if(listen_fd_ >= 0 and fill_address(address, socket_path_))
    {
    // zostawiony plik po ubitym procesie nie moze blokowac startu
    ::unlink(socket_path_.c_str());
    if(
      ::bind(listen_fd_, reinterpret_cast<sockaddr const *>(&address), sizeof(address)) != 0
      or ::listen(listen_fd_, 4) != 0
    )
      {
      ::close(listen_fd_);
      listen_fd_ = -1;
      }
    }
  else if(listen_fd_ >= 0)
    {
    ::close(listen_fd_);
    listen_fd_ = -1;
    }

  if(listen_fd_ >= 0)
    {
    set_non_blocking(listen_fd_);
    worker_ = std::thread{[this] { run(); }};
    }
  }

server_t::~server_t()
  {
  stopping_.store(true, std::memory_order_relaxed);
  if(wakeup_fd_ >= 0)
    signal(wakeup_fd_);
  if(worker_.joinable())
    worker_.join();

    // bez tego gra po drugiej stronie nie zobaczy konca strumienia i bedzie czekac na ramki w nieskonczonosc
    {
    std::lock_guard const lock{peers_mutex_};
    for(peer_t const & peer: peers_)
      ::close(peer.fd);
    peers_.clear();
    client_count_.store(0u, std::memory_order_relaxed);
    }

  if(listen_fd_ >= 0)
    {
    ::close(listen_fd_);
    ::unlink(socket_path_.c_str());
    }
  if(wakeup_fd_ >= 0)
    ::close(wakeup_fd_);
  }

[[nodiscard]]
auto server_t::path() const noexcept -> std::string_view { return socket_path_; }

auto server_t::listening() const noexcept -> bool
  { return listen_fd_ >= 0; }

[[nodiscard]]
auto server_t::clients() const noexcept -> unsigned
  { return client_count_.load(std::memory_order_relaxed); }

auto server_t::publish(frame_t const & frame) -> void
  {
  if(listen_fd_ < 0)
    return;

  std::string const encoded{encode(frame)};
  if(encoded.empty())
    return;

    {
    std::lock_guard const lock{peers_mutex_};
    retained_ = encoded;

    // liczy sie tylko najswiezszy obraz, wiec zalegla ramka idzie do kosza zamiast rosnac w kolejke
    for(peer_t & peer: peers_)
      {
      peer.outbox = encoded;
      peer.sent = 0u;
      }
    }

  signal(wakeup_fd_);
  }

auto server_t::run() -> void
  {
  std::vector<pollfd> fds;

  while(true)
    {
    fds.clear();
    fds.push_back(pollfd{.fd = listen_fd_, .events = POLLIN, .revents = 0});
    fds.push_back(pollfd{.fd = wakeup_fd_, .events = POLLIN, .revents = 0});

      {
      std::lock_guard const lock{peers_mutex_};
      for(peer_t const & peer: peers_)
        fds.push_back(
          pollfd{
            .fd = peer.fd,
            .events = static_cast<short>(POLLIN | (peer.sent < peer.outbox.size() ? POLLOUT : 0)),
            .revents = 0
          }
        );
      }

    if(::poll(fds.data(), fds.size(), -1) < 0)
      {
      if(errno == EINTR)
        continue;
      return;
      }

    // pobudka przychodzi i od publish i od destruktora - rozroznia je tylko ta flaga
    if((fds[1].revents & POLLIN) != 0)
      {
      drain(wakeup_fd_);
      if(stopping_.load(std::memory_order_relaxed))
        return;
      }

    if((fds[0].revents & POLLIN) != 0)
      while(true)
        {
        int const accepted{::accept4(listen_fd_, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK)};
        if(accepted < 0)
          break;

        std::lock_guard const lock{peers_mutex_};
        // swiezo podlaczona gra dostaje aktualny obraz od razu, nie czeka na nastepna zmiane
        peers_.push_back(peer_t{.fd = accepted, .outbox = retained_, .sent = 0u});
        client_count_.store(static_cast<unsigned>(peers_.size()), std::memory_order_relaxed);
        }

      {
      std::lock_guard const lock{peers_mutex_};
      std::vector<int> dropped;

      // kolejnosc peers_ zmienia wylacznie ten watek, wiec pozycja w fds odpowiada pozycji w wektorze
      for(size_t peer_index{}; peer_index != peers_.size(); ++peer_index)
        {
        peer_t & peer{peers_[peer_index]};
        size_t const poll_index{2u + peer_index};
        if(poll_index >= fds.size())
          break;

        auto const revents{fds[poll_index].revents};
        bool drop{(revents & (POLLHUP | POLLERR | POLLNVAL)) != 0};

        if(not drop and (revents & POLLIN) != 0)
          {
          // klient nic nie mowi, wiec cokolwiek czytelnego oznacza tylko jedno - rozlaczenie
          std::array<char, 64> discard{};
          auto const got{::read(peer.fd, discard.data(), discard.size())};
          if(got == 0 or (got < 0 and errno != EAGAIN and errno != EWOULDBLOCK and errno != EINTR))
            drop = true;
          }

        if(not drop and (revents & POLLOUT) != 0 and peer.sent < peer.outbox.size())
          {
          auto const written{
            ::send(peer.fd, peer.outbox.data() + peer.sent, peer.outbox.size() - peer.sent, MSG_NOSIGNAL)
          };
          if(written > 0)
            peer.sent += static_cast<size_t>(written);
          else if(written < 0 and errno != EAGAIN and errno != EWOULDBLOCK and errno != EINTR)
            drop = true;

          if(peer.sent >= peer.outbox.size())
            {
            peer.outbox.clear();
            peer.sent = 0u;
            }
          }

        if(drop)
          dropped.push_back(peer.fd);
        }

      if(not dropped.empty())
        {
        for(int const fd: dropped)
          ::close(fd);

        std::erase_if(
          peers_, [&dropped](peer_t const & peer) { return std::ranges::find(dropped, peer.fd) != dropped.end(); }
        );
        client_count_.store(static_cast<unsigned>(peers_.size()), std::memory_order_relaxed);
        }
      }
    }
  }
  }  // namespace overlay
