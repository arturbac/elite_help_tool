#include <overlay_ipc.h>

#include <boost/ut.hpp>
#include <cmath>

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
        overlay::line_t{.text = "left diff", .color = 0x55aaffu, .swatch_space = true, .pointer = -42.5f},
        overlay::line_t{
          .text = "Steel    578  +142",
          .color = 0xccccccu,
          .swatch = {0xcc6600u, 0x00ccccu},
          .swatch_dot = true,
          .spans = {overlay::span_t{.from = 12u, .length = 7u, .color = 0x66dd66u}}
        },
        overlay::line_t{.text = "Camorra of Purui 6.3%", .color = 0xd9534fu, .emblem_column = true}
      },
      .charts = {},
      .text = overlay::text_e::normal,
      .diagrams = {},
      .pictures = {},
      .picture_columns = 3u,
      .beside = true,
      .middle = true,
      .middle_y = -0.105f,
      .middle_width = 0.6f
    }},
    .capture = overlay::capture_t{.id = 7u, .path = "sky_7.ppm", .size = 0.8f, .quiet = true, .aspect = 16.f / 9.f},
    .covers = {overlay::cover_t{
      .x = 0.0014f,
      .y = -0.1808f,
      .width = 0.085f,
      .height = 0.04f,
      .emblem = overlay::emblem_e::empire,
      .emblem_height = 0.03f,
      .emblem_color = 0x4a90d9u
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
      expect(received->frame.blocks.front().beside);
      expect(received->frame.blocks.front().middle);
      expect(received->frame.blocks.front().middle_y == -0.105f);
      expect(received->frame.capture.id == 7_ul);
      expect(received->frame.capture.quiet);
      expect(std::abs(received->frame.capture.aspect - 16.f / 9.f) < 1e-6f);
      expect(received->frame.covers.size() == 1_ul);
      if(received->frame.covers.size() == 1u)
        {
        overlay::cover_t const & cover{received->frame.covers.front()};
        expect(cover.emblem == overlay::emblem_e::empire);
        expect(cover.y == -0.1808f);
        expect(cover.ground == 0x020304u) << "the panel's black is the default ground";
        }
      auto const & lines{received->frame.blocks.front().lines};
      expect(lines.size() == 4_ul);
      expect(lines.front().text == std::string{"Bleia Eohn QT-O d7-43"});
      expect(lines.back().color == 0xd9534fu);
      if(lines.size() == 4u)
        {
        expect(lines[1].swatch_space);
        expect(not lines[1].emblem_column);
        expect(lines[1].pointer == std::optional{-42.5f});
        expect(not lines[0].pointer.has_value());
        expect(lines[3].emblem_column);
        expect(lines[1].swatch.empty());
        expect(lines[2].swatch.size() == 2_ul);
        expect(lines[2].swatch_dot);
        expect(lines[2].spans.size() == 1_ul);
        if(not lines[2].spans.empty())
          {
          expect(lines[2].spans.front().from == 12_u);
          expect(lines[2].spans.front().length == 7_u);
          expect(lines[2].spans.front().color == 0x66dd66u);
          }
        }
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
