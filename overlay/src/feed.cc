#include <overlay_ipc.h>

#include <chrono>
#include <cstdio>
#include <format>
#include <thread>

///\brief zrodlo testowe - dowodzi ze gniazdo przechodzi przez granice kontenera pressure-vessel
///
/// docelowo te ramki wysyla elite_help_tool; to narzedzie istnieje po to, zeby dalo sie sprawdzic
/// sama droge, bez wciagania w test calej aplikacji i bazy
auto main() -> int
  {
  std::string const path{overlay::default_socket_path()};
  overlay::server_t server{path};

  if(not server.listening())
    {
    std::fprintf(stderr, "nie mozna nasluchiwac na %s\n", path.c_str());
    return 1;
    }

  std::printf("nasluch na %s, Ctrl-C konczy\n", path.c_str());
  std::fflush(stdout);

  for(uint64_t sequence{1u};; ++sequence)
    {
    auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

    overlay::frame_t frame{
      .seq = sequence,
      .blocks = {
        overlay::block_t{
          .corner = overlay::corner_e::top_left,
          .ttl_ms = 5000u,
          .lines
          = {overlay::line_t{.text = "EHT test feed", .color = 0x9ad1ffu}, overlay::line_t{.text = std::format("{:%H:%M:%S} UTC", now), .color = 0xffffffu}, overlay::line_t{.text = std::format("ramka {}", sequence), .color = 0x86d986u}}
        },
        overlay::block_t{
          .corner = overlay::corner_e::bottom_left,
          .ttl_ms = 5000u,
          .lines = {overlay::line_t{.text = "lewy dolny naroznik", .color = 0xd9a34au}}
        }
      }
    };

    server.publish(frame);

    if(sequence % 20u == 0u)
      {
      std::printf("wyslano %llu ramek, klientow: %u\n", (unsigned long long)sequence, server.clients());
      std::fflush(stdout);
      }

    std::this_thread::sleep_for(std::chrono::milliseconds{250});
    }
  }
