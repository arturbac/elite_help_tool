#include <surface_nav.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <fstream>
#include <utility>
#include <vector>

namespace nav
  {
namespace
  {
[[nodiscard]]
constexpr auto is_digit(char c) noexcept -> bool
  { return c >= '0' and c <= '9'; }

[[nodiscard]]
constexpr auto is_letter(char c) noexcept -> bool
  { return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z'); }

///\brief a number or a lone hemisphere letter, in the order the text has them
struct token_t
  {
  double value{};
  ///\brief 0 for a number, else the letter upper-cased
  char hemisphere{};
  };

struct number_t
  {
  double value{};
  char hemisphere{};
  };

///\brief the numbers of the text, each with the hemisphere letter that goes with it, if any
[[nodiscard]]
auto read_numbers(std::string_view text, bool comma_is_decimal) -> std::vector<number_t>
  {
  std::vector<token_t> tokens;
  size_t i{};
  while(i < text.size())
    {
    char const c{text[i]};
    bool const signed_start{(c == '-' or c == '+') and i + 1u < text.size() and is_digit(text[i + 1u])};
    if(is_digit(c) or signed_start)
      {
      std::string digits;
      if(c == '-')
        digits += '-';
      if(signed_start)
        ++i;
      while(i < text.size() and is_digit(text[i]))
        digits += text[i++];
      bool const separator{
        i + 1u < text.size() and (text[i] == '.' or (comma_is_decimal and text[i] == ',')) and is_digit(text[i + 1u])
      };
      if(separator)
        {
        digits += '.';
        ++i;
        while(i < text.size() and is_digit(text[i]))
          digits += text[i++];
        }
      double value{};
      std::from_chars(digits.data(), digits.data() + digits.size(), value);
      tokens.push_back(token_t{.value = value, .hemisphere = 0});
      continue;
      }
    if(is_letter(c))
      {
      size_t const begin{i};
      while(i < text.size() and is_letter(text[i]))
        ++i;
      // only a letter standing on its own - the n of "Lon" is no hemisphere
      if(i - begin == 1u)
        switch(char const upper{static_cast<char>(c & ~0x20)}; upper)
          {
          case 'N':
          case 'S':
          case 'E':
          case 'W': tokens.push_back(token_t{.value = 0.0, .hemisphere = upper}); break;
          default:  break;
          }
      continue;
      }
    ++i;
    }

  // a letter belongs to the number before it, unless that one already has its own - then to the next
  std::vector<number_t> numbers;
  char pending{};
  for(token_t const & token: tokens)
    {
    if(token.hemisphere == 0)
      {
      numbers.push_back(number_t{.value = token.value, .hemisphere = std::exchange(pending, char{})});
      continue;
      }
    if(not numbers.empty() and numbers.back().hemisphere == 0 and pending == 0)
      numbers.back().hemisphere = token.hemisphere;
    else
      pending = token.hemisphere;
    }
  return numbers;
  }

[[nodiscard]]
constexpr auto is_longitude_letter(char c) noexcept -> bool
  { return c == 'E' or c == 'W'; }

[[nodiscard]]
constexpr auto is_latitude_letter(char c) noexcept -> bool
  { return c == 'N' or c == 'S'; }

[[nodiscard]]
auto signed_value(number_t const & number) -> double
  { return number.hemisphere == 'S' or number.hemisphere == 'W' ? -std::abs(number.value) : number.value; }
  }  // namespace

auto parse_point(std::string_view text) -> std::optional<bio::surface_point_t>
  {
  // A comma is the decimal mark only in a text that has no points at all - "12,34 -56,78" - and even then
  // "12, 34" reads as two whole numbers once the other reading does not give two
  bool const no_points{not text.contains('.')};
  std::vector<number_t> numbers{read_numbers(text, no_points)};
  if(numbers.size() != 2u and no_points)
    numbers = read_numbers(text, false);
  if(numbers.size() != 2u)
    return std::nullopt;

  number_t lat{numbers[0]};
  number_t lon{numbers[1]};
  if(is_longitude_letter(lat.hemisphere) or is_latitude_letter(lon.hemisphere))
    std::swap(lat, lon);
  if(is_longitude_letter(lat.hemisphere) or is_latitude_letter(lon.hemisphere))
    return std::nullopt;

  double const latitude{signed_value(lat)};
  double longitude{signed_value(lon)};
  if(std::abs(latitude) > 90.0 or std::abs(longitude) > 360.0)
    return std::nullopt;
  if(longitude > 180.0)
    longitude -= 360.0;
  else if(longitude < -180.0)
    longitude += 360.0;
  return bio::surface_point_t{.latitude = latitude, .longitude = longitude};
  }

auto guide(bio::surface_point_t here, std::optional<double> heading_deg, bio::surface_point_t target, double radius_m)
  -> guidance_t
  {
  guidance_t result{
    .distance_m = bio::surface_distance_m(here, target, radius_m), .bearing_deg = bio::bearing_deg(here, target)
  };
  if(heading_deg)
    result.turn_deg = std::remainder(result.bearing_deg - *heading_deg, 360.0);
  return result;
  }

auto format_distance(double metres) -> std::string
  {
  if(metres < 1'000.0)
    return std::format("{:.0f} m", metres);
  if(metres < 100'000.0)
    return std::format("{:.2f} km", metres / 1'000.0);
  return std::format("{:.0f} km", metres / 1'000.0);
  }

auto format_turn(double turn_deg) -> std::string
  {
  double const size{std::abs(turn_deg)};
  if(size < 5.0)
    return "ahead";
  if(size > 170.0)
    return "behind";
  return std::format("{:.0f}° {}", size, turn_deg > 0.0 ? "right" : "left");
  }

auto format_duration(std::chrono::seconds duration) -> std::string
  {
  auto const seconds{duration.count()};
  if(seconds < 90)
    return std::format("{} s", seconds);
  auto const minutes{(seconds + 30) / 60};
  if(minutes < 60)
    return std::format("{} min", minutes);
  return std::format("{} h {} min", minutes / 60, minutes % 60);
  }

namespace
  {
///\brief how far back the speed looks, and how much of it has to be there before it is said
constexpr std::chrono::seconds speed_window{4};
constexpr std::chrono::milliseconds speed_least{1500};
  }  // namespace

auto ground_speed_t::push(clock::time_point at, bio::surface_point_t point, double radius_m) -> void
  {
  // another body, or the same file read twice - neither is a step
  if(radius_m != radius_m_)
    {
    samples_.clear();
    radius_m_ = radius_m;
    }
  if(not samples_.empty() and samples_.back().point.latitude == point.latitude
     and samples_.back().point.longitude == point.longitude and at - samples_.back().at < speed_window)
    return;
  samples_.push_back(sample_t{.at = at, .point = point});
  while(samples_.size() > 2u and at - samples_[1].at >= speed_window)
    samples_.pop_front();
  }

auto ground_speed_t::metres_per_second(clock::time_point now) const -> std::optional<double>
  {
  if(samples_.size() < 2u or radius_m_ <= 0.0)
    return std::nullopt;
  sample_t const & first{samples_.front()};
  sample_t const & last{samples_.back()};
  // standing still writes nothing, so the last step long ago means we stopped
  if(now - last.at > speed_window)
    return 0.0;
  auto const span{last.at - first.at};
  if(span < speed_least)
    return std::nullopt;
  double const seconds{std::chrono::duration<double>(span).count()};
  return bio::surface_distance_m(first.point, last.point, radius_m_) / seconds;
  }

// named, not anonymous - glaze's reflection needs the types to have linkage
namespace detail
  {
struct codex_line_t
  {
  std::string timestamp;
  std::string Name;
  std::string Name_Localised;
  std::string Category_Localised;
  std::string SubCategory_Localised;
  uint64_t SystemAddress{};
  std::optional<uint32_t> BodyID;
  std::optional<double> Latitude;
  std::optional<double> Longitude;
  bool IsNewEntry{};
  };

///\brief ApproachBody and Touchdown both - the two that pair a body's name with its number
struct body_line_t
  {
  uint64_t SystemAddress{};
  std::string Body;
  std::optional<uint32_t> BodyID;
  };
  }  // namespace detail

auto codex_index_t::read_line(std::string const & line) -> void
  {
  constexpr auto opts{glz::opts{.error_on_unknown_keys = false}};
  if(line.contains("\"event\":\"CodexEntry\""))
    {
    if(not line.contains("\"Latitude\""))
      return;
    detail::codex_line_t entry{};
    if(glz::read<opts>(entry, line) or not entry.BodyID or not entry.Latitude or not entry.Longitude)
      return;
    auto & points{points_[body_key_t{entry.SystemAddress, *entry.BodyID}]};
    // the same thing scanned again where it stands is not another place
    bio::surface_point_t const point{*entry.Latitude, *entry.Longitude};
    constexpr double same_place_deg{0.0005};
    bool const known{std::ranges::any_of(
      points,
      [&](codex_point_t const & other)
      {
        return other.name == (entry.Name_Localised.empty() ? entry.Name : entry.Name_Localised)
               and std::abs(other.point.latitude - point.latitude) < same_place_deg
               and std::abs(other.point.longitude - point.longitude) < same_place_deg;
      }
    )};
    if(known)
      return;
    points.push_back(codex_point_t{
      .name = entry.Name_Localised.empty() ? entry.Name : entry.Name_Localised,
      .category = entry.SubCategory_Localised.empty() ? entry.Category_Localised : entry.SubCategory_Localised,
      .point = point,
      .seen = std::move(entry.timestamp),
      .first = entry.IsNewEntry
    });
    return;
    }
  if(line.contains("\"event\":\"ApproachBody\"") or line.contains("\"event\":\"Touchdown\""))
    {
    detail::body_line_t body{};
    if(not glz::read<opts>(body, line) and body.BodyID and not body.Body.empty())
      bodies_[std::move(body.Body)] = body_key_t{body.SystemAddress, *body.BodyID};
    }
  }

auto codex_index_t::update(std::filesystem::path const & journal_dir) -> void
  {
  std::vector<std::filesystem::path> journals;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()}; name.starts_with("Journal.") and name.ends_with(".log"))
      journals.push_back(entry.path());
  std::ranges::sort(journals);

  for(std::filesystem::path const & path: journals)
    {
    std::uintmax_t const size{std::filesystem::file_size(path, ec)};
    if(ec)
      continue;
    std::uintmax_t & done{read_[path]};
    if(size <= done)
      continue;
    std::ifstream in{path, std::ios::binary};
    in.seekg(static_cast<std::streamoff>(done));
    std::string line;
    while(std::getline(in, line))
      {
      // a line the game is still writing has no end yet - it is read whole the next time
      if(in.eof())
        break;
      done += line.size() + 1u;
      if(line.contains("CodexEntry") or line.contains("ApproachBody") or line.contains("Touchdown"))
        read_line(line);
      }
    }
  }

auto codex_index_t::on_body(std::string const & body_name) const -> std::vector<codex_point_t>
  {
  auto const body{bodies_.find(body_name)};
  if(body == bodies_.end())
    return {};
  auto const points{points_.find(body->second)};
  return points == points_.end() ? std::vector<codex_point_t>{} : points->second;
  }
  }  // namespace nav
