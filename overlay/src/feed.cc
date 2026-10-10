#include <overlay_ipc.h>

#include <chrono>
#include <cstdio>
#include <format>
#include <thread>

///\brief a test source - it proves the socket crosses the pressure-vessel container boundary
///
/// in the end elite_help_tool sends these frames; this tool exists so that the path itself and the
/// layout of the text can be checked without dragging the whole application, database and game into it.
/// the content deliberately matches the real thing, long wrapping line in the side band included
namespace
  {
constexpr uint32_t colour_heading{0x9ad1ffu};
constexpr uint32_t colour_plain{0xddddddu};
constexpr uint32_t colour_alert{0xd9a34au};
constexpr uint32_t colour_first{0x3cb371u};
constexpr uint32_t sample_ttl_ms{5000u};

[[nodiscard]]
auto sample_frame(uint64_t sequence) -> overlay::frame_t
  {
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  return overlay::frame_t{
    .seq = sequence,
    .blocks = {
      overlay::block_t{
        .corner = overlay::corner_e::top_left, .ttl_ms = sample_ttl_ms, .lines = {overlay::line_t{.text = "Bleia Eohn QT-O d7-43", .color = colour_heading}, overlay::line_t{.text = "Camorra of Purui", .color = colour_plain}, overlay::line_t{.text = "Industrial / Anarchy", .color = colour_plain}, overlay::line_t{.text = "security: Anarchy", .color = colour_plain}, overlay::line_t{.text = "FSS incomplete, 14 bodies known", .color = colour_alert}, overlay::line_t{.text = std::format("feed {:%H:%M:%S} UTC, frame {}", now, sequence), .color = colour_plain}}
      },
      overlay::
        block_t{
          .corner = overlay::corner_e::bottom_right,
          .ttl_ms = sample_ttl_ms,
          .lines
          = {overlay::line_t{.text = "worth mapping: 4 bodies, 2'913'400 Cr", .color = colour_heading}, overlay::line_t{.text = "2  1'204'800 Cr  512 ls", .color = colour_first}, overlay::line_t{.text = "3 a  879'100 Cr  1'204 ls  landable", .color = colour_first}, overlay::line_t{.text = "5  521'000 Cr  3'880 ls", .color = colour_plain}, overlay::line_t{.text = "7 c  308'500 Cr  14'902 ls  landable", .color = colour_plain}}
        },
      overlay::block_t{
        .corner = overlay::corner_e::bottom_left,
        .ttl_ms = sample_ttl_ms,
        .lines = {overlay::line_t{.text = "next: Bleia Eohn WO-A d13-24 (M)", .color = colour_plain}}
      },
      overlay::block_t{
        .corner = overlay::corner_e::top_right,
        .ttl_ms = sample_ttl_ms,
        .lines = {overlay::line_t{
          .text = "a very long test line that has to wrap inside the side band "
                  "instead of running into the middle of the screen the game occupies",
          .color = colour_plain
        }}
      }
    }
  };
  }
  }  // namespace

auto main() -> int
  {
  std::string const path{overlay::default_socket_path()};
  overlay::server_t server{path};

  if(not server.listening())
    {
    std::fprintf(stderr, "cannot listen on %s\n", path.c_str());
    return 1;
    }

  std::printf("nasluch na %s, Ctrl-C konczy\n", path.c_str());
  std::fflush(stdout);

  for(uint64_t sequence{1u};; ++sequence)
    {
    [[maybe_unused]]
    auto const published{server.publish(sample_frame(sequence))};

    if(sequence % 20u == 0u)
      {
      std::printf("wyslano %llu ramek, klientow: %u\n", (unsigned long long)sequence, server.clients());
      std::fflush(stdout);
      }

    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    }
  }
