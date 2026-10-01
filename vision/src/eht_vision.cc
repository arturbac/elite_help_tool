#include <vision_recorder.h>

#include <eht_settings.h>
#include <overlay_protocol.h>

#include <spdlog/spdlog.h>

#include <csignal>
#include <filesystem>
#include <optional>
#include <print>
#include <string_view>
#include <thread>

namespace
  {
auto usage() -> int
  {
  std::println(
    stderr,
    "usage: eht_vision record [--journal-dir DIR]\n"
    "  run in the directory of elite_help_tool: it reads eht_settings.json there and, by default,\n"
    "  the journals and Status.json through its journal-dir link. The layer's sample is found\n"
    "  through EHT_OVERLAY_SOCKET, as the tool finds it"
  );
  return 2;
  }
  }  // namespace

auto main(int argc, char ** argv) -> int
  {
  std::filesystem::path journal_dir{"journal-dir"};
  std::optional<std::string_view> mode;
  for(int at{1}; at < argc; ++at)
    {
    std::string_view const arg{argv[at]};
    if(arg == "--journal-dir" and at + 1 < argc)
      journal_dir = argv[++at];
    else if(not mode and not arg.starts_with('-'))
      mode = arg;
    else
      return usage();
    }
  if(mode.value_or("record") != "record")
    return usage();

  // the signals are waited for here, so every thread started below must leave them alone
  sigset_t signals{};
  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &signals, nullptr);

  // read only when it is there - the tool writes it, the recorder never makes one of defaults
  std::filesystem::path const settings_path{eht::settings_file_name};
  if(std::error_code ec; std::filesystem::exists(settings_path, ec))
    eht::load_settings(settings_path);
  eht::settings_watcher_t const watcher{settings_path};

  vision::recorder_t recorder{journal_dir, overlay::sample_file_path()};
  spdlog::info(
    "vision: watching {} and {}, recording {}",
    journal_dir.string(),
    overlay::sample_file_path(),
    eht::settings()->vision.record ? "on" : "off until vision.record is set"
  );
  std::jthread worker{[&recorder](std::stop_token stop) { recorder.run(stop); }};

  int signal{};
  sigwait(&signals, &signal);
  spdlog::info("vision: stopped by signal {}", signal);
  worker.request_stop();
  return 0;
  }
