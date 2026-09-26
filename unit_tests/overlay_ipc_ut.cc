#include <overlay_ipc.h>

#include <boost/ut.hpp>

#include <unistd.h>

#include <chrono>
#include <format>
#include <thread>

using namespace boost::ut;
using namespace std::chrono_literals;

namespace
  {
///\brief a unix socket path fits in 108 bytes, so it is kept short
[[nodiscard]]
auto scratch_socket(std::string_view tag) -> std::string
  { return std::format("/tmp/eht_ovl_{}_{}.sock", tag, ::getpid()); }

///\brief waits until the condition holds, but no longer than the limit - tests must not hang
template<typename predicate_t>
[[nodiscard]]
auto wait_until(predicate_t predicate, std::chrono::milliseconds limit = 3000ms) -> bool
  {
  auto const deadline{std::chrono::steady_clock::now() + limit};
  while(std::chrono::steady_clock::now() < deadline)
    {
    if(predicate())
      return true;
    std::this_thread::sleep_for(10ms);
    }
  return predicate();
  }

[[nodiscard]]
auto sample_frame(uint64_t sequence) -> overlay::frame_t
  {
  return overlay::frame_t{
    .seq = sequence,
    .blocks = {overlay::block_t{
      .corner = overlay::corner_e::top_right,
      .ttl_ms = 2500u,
      .lines = {
        overlay::line_t{.text = "Bleia Eohn QT-O d7-43", .color = 0x3cb371u},
        overlay::line_t{.text = "Camorra of Purui 6.3%", .color = 0xd9534fu}
      }
    }}
  };
  }
  }  // namespace

auto main() -> int
  {
  "a frame reaches the client whole"_test = []
  {
    auto const path{scratch_socket("roundtrip")};
    overlay::server_t server{path};
    expect(server.listening());

    overlay::client_t client{path};
    expect(wait_until([&] { return server.clients() == 1u; })) << "the client did not connect";

    expect(wait_until(
      [&]
      {
        server.publish(sample_frame(7u));
        return client.snapshot() != nullptr;
      }
    )) << "the frame did not arrive";

    auto const received{client.snapshot()};
    expect(received != nullptr);
    if(received != nullptr)
      {
      expect(received->frame.seq == 7_ul);
      expect(received->frame.blocks.size() == 1_ul);
      expect(received->frame.blocks.front().corner == overlay::corner_e::top_right);
      expect(received->frame.blocks.front().ttl_ms == 2500_u);
      expect(received->frame.blocks.front().lines.size() == 2_ul);
      expect(received->frame.blocks.front().lines.front().text == std::string{"Bleia Eohn QT-O d7-43"});
      expect(received->frame.blocks.front().lines.back().color == 0xd9534fu);
      }
  };

  // the usual order: the tool has been running for a while, the game comes up later
  "a client connecting after a publish gets the last picture"_test = []
  {
    auto const path{scratch_socket("retained")};
    overlay::server_t server{path};
    expect(server.listening());

    server.publish(sample_frame(42u));

    overlay::client_t client{path};
    expect(wait_until([&] { return client.snapshot() != nullptr; })) << "the remembered frame did not arrive";

    auto const received{client.snapshot()};
    expect(received != nullptr);
    if(received != nullptr)
      expect(received->frame.seq == 42_ul);
  };

  // the game can start before the tool - no server must break anything
  "a client with no server lives on and returns nothing"_test = []
  {
    overlay::client_t client{scratch_socket("noserver")};
    std::this_thread::sleep_for(200ms);
    expect(not client.connected());
    expect(client.snapshot() == nullptr);
    expect(client.received() == 0_ul);
  };

  // the tool can be restarted mid game; the client is to come back on its own
  "the client comes back after a server restart"_test = []
  {
    auto const path{scratch_socket("restart")};
    overlay::client_t client{path};

      {
      overlay::server_t first{path};
      expect(first.listening());
      expect(wait_until([&] { return first.clients() == 1u; })) << "the first connection did not happen";
      expect(wait_until(
        [&]
        {
          first.publish(sample_frame(1u));
          return client.received() >= 1u;
        }
      ));
      }

    expect(wait_until([&] { return not client.connected(); })) << "the client did not notice the server go away";

    overlay::server_t second{path};
    expect(second.listening());
    expect(wait_until([&] { return second.clients() == 1u; }, 5000ms)) << "the client did not come back";

    auto const before{client.received()};
    expect(wait_until(
      [&]
      {
        second.publish(sample_frame(2u));
        return client.received() > before;
      }
    )) << "no frames arrive after the return";
  };

  // the server can run with no game at all; publish has no business failing on that
  "a server with no clients accepts a publish"_test = []
  {
    overlay::server_t server{scratch_socket("noclient")};
    expect(server.listening());
    for(uint64_t sequence{}; sequence != 100u; ++sequence)
      server.publish(sample_frame(sequence));
    expect(server.clients() == 0_u);
  };
  }
