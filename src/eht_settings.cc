#include <eht_settings.h>
#include <json_glaze.h>

#include <spdlog/spdlog.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>

namespace eht
  {
namespace
  {
  // a reader takes the whole snapshot at once and keeps it for as long as it needs, while a reload
  // swaps in a new one - nobody ever sees half of one file and half of the next
  auto current_settings() -> std::atomic<std::shared_ptr<settings_t const>> &
    {
    static std::atomic<std::shared_ptr<settings_t const>> current{std::make_shared<settings_t const>()};
    return current;
    }

  [[nodiscard]]
  [[nodiscard]]
  auto current_private() -> std::atomic<bool> &
    {
    static std::atomic<bool> sjona_private{};
    return sjona_private;
    }

  auto write_file(std::filesystem::path const & path, settings_t const & value) -> bool
    {
    std::string text;
    if(auto const err{glz::write<glz::opts{.prettify = true}>(value, text)}; err) [[unlikely]]
      {
      spdlog::error("settings: could not put the settings into words");
      return false;
      }
    // A private switch someone set stays in the file when missing settings are written in; one never set
    // never appears, not even in a file of defaults
    if(current_private().load(std::memory_order_acquire) and text.starts_with("{\n"))
      text.insert(2u, "   \"sjona_private\": true,\n");
    text.push_back('\n');

    std::ofstream out{path, std::ios::binary | std::ios::trunc};
    out << text;
    if(not out) [[unlikely]]
      {
      spdlog::error("settings: could not write {}", path.string());
      return false;
      }
    return true;
    }

  [[nodiscard]]
  auto read_text(std::filesystem::path const & path) -> std::optional<std::string>
    {
    std::ifstream in{path, std::ios::binary};
    if(not in)
      return std::nullopt;
    std::stringstream buffer;
    buffer << in.rdbuf();
    return std::move(buffer).str();
    }
  }  // namespace

auto colour_t::read(std::string const & text) -> void
  {
  // "#rrggbb", or the same without the hash; anything else leaves the colour as it was
  std::string_view digits{text};
  if(digits.starts_with('#'))
    digits.remove_prefix(1);
  if(digits.size() != 6u)
    return;
  uint32_t parsed{};
  auto const [ptr, ec]{std::from_chars(digits.data(), digits.data() + digits.size(), parsed, 16)};
  if(ec == std::errc{} and ptr == digits.data() + digits.size())
    rgb = parsed;
  }

auto colour_t::write() const -> std::string
  { return std::format("#{:06x}", rgb & 0xffffffu); }

auto settings() -> std::shared_ptr<settings_t const>
  { return current_settings().load(std::memory_order_acquire); }

auto private_settings() noexcept -> private_settings_t
  { return private_settings_t{.sjona_private = current_private().load(std::memory_order_acquire)}; }

auto load_settings(std::filesystem::path const & path) -> bool
  {
  auto const text{read_text(path)};
  if(not text)
    {
    // the first run, or a file deleted to start over - the defaults are written out to be edited
    settings_t const defaults{};
    current_private().store(false, std::memory_order_release);
    current_settings().store(std::make_shared<settings_t const>(defaults), std::memory_order_release);
    if(write_file(path, defaults))
      spdlog::info("settings: wrote the defaults to {}", path.string());
    return true;
    }

  settings_t loaded{};
  auto err{glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(loaded, *text)};
  bool incomplete{};
  if(err and err.ec == glz::error_code::missing_key)
    {
    // written by an older version: what is there is kept, what is new takes its default
    incomplete = true;
    loaded = settings_t{};
    err = glz::read<glz::opts{.error_on_unknown_keys = false}>(loaded, *text);
    }

  if(err)
    {
    spdlog::warn(
      "settings: {} could not be read, the settings stay as they were: {}",
      path.string(),
      glz::format_error(err, *text)
    );
    return false;
    }

  // read apart from the rest, since settings_t is written out whole and these must not be
  private_settings_t hidden{};
  if(auto const hidden_err{glz::read<glz::opts{.error_on_unknown_keys = false}>(hidden, *text)}; hidden_err)
    hidden = {};
  current_private().store(hidden.sjona_private, std::memory_order_release);

  current_settings().store(std::make_shared<settings_t const>(std::move(loaded)), std::memory_order_release);

  if(incomplete and write_file(path, *settings()))
    spdlog::info("settings: {} lacked some of the settings, their defaults were written in", path.string());
  else
    spdlog::info("settings: read {}", path.string());
  return true;
  }

settings_watcher_t::settings_watcher_t(std::filesystem::path path) :
    path_{std::move(path)},
    worker_{
      [this](std::stop_token stop)
      {
        // Looked at once a second rather than through inotify: a settings file is saved by hand, a
        // second is nothing to wait for, and it keeps working on a file replaced by an editor that
        // writes a new one and renames it over the old
        std::error_code ec;
        auto seen{std::filesystem::last_write_time(path_, ec)};
        while(not stop.stop_requested())
          {
          std::this_thread::sleep_for(std::chrono::seconds{1});
          auto const now{std::filesystem::last_write_time(path_, ec)};
          if(ec or now == seen)
            continue;
          seen = now;
          load_settings(path_);
          }
      }
    }
  {
  }
  }  // namespace eht
