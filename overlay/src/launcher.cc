#include <overlay_protocol.h>

#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace
  {
auto prepend_path(char const * name, std::string const & value) -> void
  {
  std::string merged{value};
  if(char const * const current{std::getenv(name)}; current != nullptr and *current != '\0')
    {
    merged += ':';
    merged += current;
    }
  ::setenv(name, merged.c_str(), 1);
  }
  }  // namespace

///\brief a wrapper for the Steam launch option: eht-overlay-run %command%
///
/// it draws nothing itself - it turns the layer on and hands the process to the game. whatever
/// happens here, the game must still start, because this wrapper stands in its way
auto main(int argc, char ** argv) -> int
  {
  if(argc < 2)
    {
    std::fputs(
      "uzycie: eht-overlay-run <polecenie> [argumenty...]\n"
      "w opcjach uruchamiania Steam: eht-overlay-run %command%\n",
      stderr
    );
    return 2;
    }

  // a zero overwrite only applies when the variable is absent - a deliberate opt-out is respected
  ::setenv("ENABLE_EHT_OVERLAY", "1", 0);

  if(
    char const * const manifest_dir{std::getenv("EHT_OVERLAY_MANIFEST_DIR")};
    manifest_dir != nullptr and *manifest_dir != '\0'
  )
    prepend_path("VK_ADD_IMPLICIT_LAYER_PATH", manifest_dir);

  // the socket directory must exist before the game tries to connect; a missing one is no reason to stop
  std::error_code ec{};
  std::filesystem::create_directories(std::filesystem::path{overlay::default_socket_path()}.parent_path(), ec);

  ::execvp(argv[1], argv + 1);

  // we only get here when the game failed to start
  std::fprintf(stderr, "eht-overlay-run: cannot launch %s: %s\n", argv[1], std::strerror(errno));
  return 127;
  }
