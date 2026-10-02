#include <graphics_profile.h>

#include <charconv>
#include <cmath>
#include <format>
#include <system_error>

namespace graphics_profile
  {
namespace
  {
  ///\brief the text between <tag> and </tag>
  [[nodiscard]]
  auto element(std::string_view text, std::string_view tag) -> std::optional<std::string_view>
    {
    std::string const open{std::format("<{}>", tag)};
    auto const from{text.find(open)};
    if(from == std::string_view::npos)
      return std::nullopt;
    auto const begin{from + open.size()};
    auto const end{text.find('<', begin)};
    if(end == std::string_view::npos)
      return std::nullopt;
    return text.substr(begin, end - begin);
    }

  template<typename value_type>
  [[nodiscard]]
  auto number(std::string_view text, std::string_view tag) -> std::optional<value_type>
    {
    auto const found{element(text, tag)};
    if(not found)
      return std::nullopt;
    value_type value{};
    auto const [end, ec]{std::from_chars(found->data(), found->data() + found->size(), value)};
    if(ec != std::errc{} or end != found->data() + found->size())
      return std::nullopt;
    return value;
    }

  ///\brief the menu steps by 0.01 at the finest, the file writes six places
  constexpr double supersampling_tolerance{0.005};

  [[nodiscard]]
  auto describe(setting_t const & setting, profile_t const & planet, profile_t const & space) -> std::string
    {
    if(matches(setting, planet))
      return planet.name;
    if(matches(setting, space))
      return space.name;
    return std::format(
      "upscaling {}, supersampling {:.2f}, AA mode {}", setting.upscaling, setting.supersampling, setting.anti_aliasing
    );
    }
  }  // namespace

auto parse(std::string_view text) -> std::optional<setting_t>
  {
  auto const upscaling{number<uint32_t>(text, "UpscalingQuality")};
  auto const supersampling{number<double>(text, "SSAAMultiplier")};
  auto const anti_aliasing{number<uint32_t>(text, "AAMode")};
  if(not upscaling or not supersampling or not anti_aliasing)
    return std::nullopt;
  return setting_t{.upscaling = *upscaling, .supersampling = *supersampling, .anti_aliasing = *anti_aliasing};
  }

auto newest_file(std::filesystem::path const & graphics_dir) -> std::filesystem::path
  {
  std::error_code ec;
  std::filesystem::path best;
  auto newest{std::filesystem::file_time_type::min()};
  for(auto const & entry: std::filesystem::directory_iterator{graphics_dir, ec})
    {
    std::string const name{entry.path().filename().string()};
    if(not name.starts_with("Custom.") or not name.ends_with(".fxcfg"))
      continue;
    if(auto const written{entry.last_write_time(ec)}; not ec and written > newest)
      {
      newest = written;
      best = entry.path();
      }
    }
  return best;
  }

auto graphics_dir_of(std::filesystem::path const & journal_dir) -> std::filesystem::path
  {
  std::error_code ec;
  std::string const journals{std::filesystem::weakly_canonical(journal_dir, ec).generic_string()};
  constexpr std::string_view saved_games{"/Saved Games/"};
  auto const at{journals.rfind(saved_games)};
  if(at == std::string::npos)
    return {};
  return std::filesystem::path{journals.substr(0u, at)} / "AppData" / "Local" / "Frontier Developments"
         / "Elite Dangerous" / "Options" / "Graphics";
  }

auto matches(setting_t const & setting, profile_t const & profile) noexcept -> bool
  {
  return setting.upscaling == profile.upscaling and setting.anti_aliasing == profile.anti_aliasing
         and std::abs(setting.supersampling - profile.supersampling) < supersampling_tolerance;
  }

auto place_of(uint64_t flags, uint64_t flags2) noexcept -> place_e
  {
  constexpr uint64_t has_lat_long_flag{1u << 21u};
  constexpr uint64_t on_foot_on_planet_flag{1u << 4u};
  return (flags & has_lat_long_flag) != 0u or (flags2 & on_foot_on_planet_flag) != 0u ? place_e::planet
                                                                                      : place_e::space;
  }

auto settle(
  watch_t & watch, place_e now_seen, std::chrono::steady_clock::time_point now, std::chrono::milliseconds wait_for
) -> std::optional<place_e>
  {
  if(watch.seen != now_seen)
    {
    watch.seen = now_seen;
    watch.seen_at = now;
    }
  // the first place read counts at once - the tool may start with the ship already there
  if(not watch.place or now - watch.seen_at >= wait_for)
    watch.place = now_seen;
  return watch.place;
  }

auto hint(place_e place, setting_t const & setting, profile_t const & planet, profile_t const & space)
  -> std::optional<std::string>
  {
  profile_t const & wanted{place == place_e::planet ? planet : space};
  if(matches(setting, wanted))
    return std::nullopt;
  return std::format(
    "graphics {}: set {} - now {}",
    place == place_e::planet ? "near the planet" : "in space",
    wanted.name,
    describe(setting, planet, space)
  );
  }
  }  // namespace graphics_profile
