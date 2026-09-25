#pragma once

#include <simple_enum/glaze_json_enum_name.hpp>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

///\brief protokol miedzy elite_help_tool a warstwa rysujaca w oknie gry
///
/// warstwa jest celowo glupia - dostaje gotowe linie tekstu i nie wie nic o bazie ani o logice gry.
/// dzieki temu w procesie gry nie ma ani SQLite ani zadnego stanu do utrzymania
namespace overlay
  {
enum struct corner_e : uint8_t
  {
  top_left,
  top_right,
  bottom_left,
  bottom_right
  };

consteval auto adl_enum_bounds(corner_e)
  {
  using enum corner_e;
  return simple_enum::adl_info{top_left, bottom_right};
  }

///\brief pojedyncza linia tekstu, kolor jako 0xRRGGBB
struct line_t
  {
  std::string text;
  uint32_t color{0xffffffu};
  };

///\brief zawartosc jednego naroznika ekranu
struct block_t
  {
  corner_e corner{corner_e::top_left};
  ///\brief po tylu milisekundach bez odswiezenia blok gasnie, 0 wylacza wygasanie
  uint32_t ttl_ms{};
  std::vector<line_t> lines;
  };

///\brief pelny obraz do narysowania - zastepuje poprzedni w calosci, liczy sie wylacznie ostatni
struct frame_t
  {
  uint64_t seq{};
  std::vector<block_t> blocks;
  };

///\brief gniazdo lezy pod $HOME, bo to jedyne miejsce widoczne po obu stronach kontenera pressure-vessel
[[nodiscard]]
inline auto default_socket_path() -> std::string
  {
  if(char const * const from_env{std::getenv("EHT_OVERLAY_SOCKET")}; from_env != nullptr and *from_env != '\0')
    return from_env;

  char const * const home{std::getenv("HOME")};
  return std::string{home != nullptr ? home : "/tmp"} + "/.local/share/elite_help_tool/overlay.sock";
  }
  }  // namespace overlay
