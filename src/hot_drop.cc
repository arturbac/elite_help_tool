#include <hot_drop.h>

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <ranges>
#include <tuple>

namespace hot_drop
  {
namespace detail
  {
///\brief an approach as written into hot_drop.jsonl - the readings as [ms from the start, Ls, seconds, seconds as
/// read]
struct attempt_json_t
  {
  uint64_t started_ms{};
  uint64_t ended_ms{};
  std::string outcome;
  bool overspeed{};
  double overspeed_from_ls{};
  double first_overspeed_ls{};
  uint32_t overspeed_entries{};
  int32_t least_seconds{-1};
  double end_speed_mm_s{};
  double last_ls{};
  context_t where;
  std::vector<std::tuple<uint64_t, double, int32_t, int32_t>> readings;
  };
  }  // namespace detail

namespace
  {
///\brief the reader's slips inside a number: letters it takes for the digits they look like
[[nodiscard]]
auto as_digit(char c) noexcept -> char
  {
  switch(c)
    {
    case 'S':
    case 's': return '5';
    case 'O':
    case 'o':
    case 'D':
    case 'Q': return '0';
    case 'I':
    case 'l':
    case '|':
    case '!': return '1';
    case 'B': return '8';
    case 'Z': return '2';
    case ',': return '.';
    default:  return c;
    }
  }

///\brief without the spaces and line ends around it - the reader of one line ends it with a line end
[[nodiscard]]
auto trimmed(std::string_view text) -> std::string_view
  {
  while(not text.empty() and std::isspace(static_cast<unsigned char>(text.front())) != 0)
    text.remove_prefix(1u);
  while(not text.empty() and std::isspace(static_cast<unsigned char>(text.back())) != 0)
    text.remove_suffix(1u);
  return text;
  }

[[nodiscard]]
auto lower(std::string_view text) -> std::string
  {
  std::string out{text};
  std::ranges::transform(out, out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return out;
  }

[[nodiscard]]
auto letters_and_digits(std::string_view text) -> std::string
  {
  std::string out;
  for(unsigned char const c: text)
    if(std::isalnum(c) != 0)
      out.push_back(char(std::toupper(c)));
  return out;
  }

[[nodiscard]]
auto edits(std::string_view a, std::string_view b) -> size_t
  {
  std::vector<size_t> row(b.size() + 1u);
  for(size_t j{}; j != row.size(); ++j)
    row[j] = j;
  for(size_t i{1u}; i <= a.size(); ++i)
    {
    size_t diagonal{row[0]};
    row[0] = i;
    for(size_t j{1u}; j <= b.size(); ++j)
      {
      size_t const up{row[j]};
      row[j] = std::min({row[j] + 1u, row[j - 1u] + 1u, diagonal + (a[i - 1u] == b[j - 1u] ? 0u : 1u)});
      diagonal = up;
      }
    }
  return row[b.size()];
  }

[[nodiscard]]
auto read_number(std::string_view text) -> std::optional<double>
  {
  std::string digits;
  for(char const c: text)
    if(char const d{as_digit(c)}; (d >= '0' and d <= '9') or d == '.')
      digits.push_back(d);
    else if(c != ' ')
      return std::nullopt;
  if(digits.empty() or std::ranges::count(digits, '.') > 1)
    return std::nullopt;
  double value{};
  auto const [end, ec]{std::from_chars(digits.data(), digits.data() + digits.size(), value)};
  if(ec != std::errc{} or end != digits.data() + digits.size())
    return std::nullopt;
  return value;
  }

  }  // namespace

auto parse_distance_ls(std::string_view text) -> std::optional<double>
  {
  text = trimmed(text);
  std::string const low{lower(text)};
  // the unit is read off the end; a 5 read as an S right before it stays a digit, so "53.5Ls" read as
  // "53.SLs" still ends in ls
  struct unit_t
    {
    std::string_view suffix;
    double ls;
    };
  static constexpr std::array units{
    unit_t{"ls", 1.0}, unit_t{"mm", 1.0 / mm_per_ls}, unit_t{"km", 1.0 / (mm_per_ls * 1000.0)}
  };
  for(unit_t const & unit: units)
    if(low.ends_with(unit.suffix) and low.size() > unit.suffix.size())
      {
      auto const value{read_number(text.substr(0u, text.size() - unit.suffix.size()))};
      if(not value)
        return std::nullopt;
      return *value * unit.ls;
      }
  return std::nullopt;
  }

auto parse_seconds(std::string_view text) -> std::optional<uint32_t>
  {
  text = trimmed(text);
  uint32_t total{};
  uint32_t parts{};
  for(auto const part: text | std::views::split(':'))
    {
    std::string_view const piece{part.begin(), part.end()};
    std::string digits;
    for(char const c: piece)
      if(char const d{as_digit(c)}; d >= '0' and d <= '9')
        digits.push_back(d);
      else if(c != ' ')
        return std::nullopt;
    if(digits.empty() or digits.size() > 2u)
      return std::nullopt;
    uint32_t value{};
    std::from_chars(digits.data(), digits.data() + digits.size(), value);
    // only the first part may run past 59
    if(parts != 0u and value > 59u)
      return std::nullopt;
    total = total * 60u + value;
    ++parts;
    }
  if(parts < 2u or parts > 3u)
    return std::nullopt;
  return total;
  }

auto name_likeness(std::string_view a, std::string_view b) -> double
  {
  std::string const x{letters_and_digits(a)};
  std::string const y{letters_and_digits(b)};
  if(x.empty() or y.empty())
    return 0.0;
  return 1.0 - double(edits(x, y)) / double(std::max(x.size(), y.size()));
  }

auto read_label(std::span<text_line_t const> lines, std::string_view target) -> std::optional<label_t>
  {
  // the name may stand alone or end a longer line, a carrier's name before its call sign or the other way round
  text_line_t const * name{};
  double best{0.7};
  for(text_line_t const & line: lines)
    if(double const likeness{name_likeness(line.text, target)}; likeness >= best)
      {
      best = likeness;
      name = &line;
      }
  if(name == nullptr)
    return std::nullopt;

  int32_t const height{std::max(name->bottom - name->top, 8)};
  std::vector<text_line_t const *> under;
  for(text_line_t const & line: lines)
    if(
      line.top >= name->bottom - height / 2 and line.top <= name->bottom + 5 * height
      and line.left >= name->left - 2 * height and line.left <= name->left + 4 * height
    )
      under.push_back(&line);
  std::ranges::sort(under, std::less{}, &text_line_t::top);
  for(auto it{under.begin()}; it != under.end(); ++it)
    if(auto const distance{parse_distance_ls((*it)->text)}; distance)
      {
      label_t label{.distance_ls = *distance, .seconds = {}, .distance_box = **it, .seconds_box = {}};
      if(auto const next{std::next(it)}; next != under.end())
        {
        label.seconds = parse_seconds((*next)->text);
        label.seconds_box = **next;
        }
      return label;
      }
  return std::nullopt;
  }

auto outcome_name(outcome_e outcome) noexcept -> std::string_view
  {
  switch(outcome)
    {
    case outcome_e::dropped:    return "dropped";
    case outcome_e::overshot:   return "overshot";
    case outcome_e::broken_off: return "broken_off";
    }
  return "broken_off";
  }

auto outcome_of(std::string_view name) noexcept -> std::optional<outcome_e>
  {
  for(outcome_e const o: {outcome_e::dropped, outcome_e::overshot, outcome_e::broken_off})
    if(outcome_name(o) == name)
      return o;
  return std::nullopt;
  }

auto summarise(attempt_t const & attempt) -> summary_t
  {
  summary_t summary;
  // too early into overspeed, the pilot turns hard to lose speed and goes in again nearer - the last way in is
  // the one the outcome tells of. It takes two readings in a row out of overspeed to end one, a single one is
  // more likely the reader's slip than the ship slowing down
  uint32_t calm{2u};
  for(reading_t const & read: attempt.readings)
    {
    summary.last_ls = read.distance_ls;
    if(read.seconds < 0 or read.distance_ls < drop_zone_ls)
      continue;
    if(summary.least_seconds < 0 or read.seconds < summary.least_seconds)
      summary.least_seconds = read.seconds;
    if(read.seconds > 5)
      {
      ++calm;
      continue;
      }
    if(calm >= 2u)
      {
      summary.overspeed = true;
      summary.overspeed_from_ls = read.distance_ls;
      if(summary.overspeed_entries == 0u)
        summary.first_overspeed_ls = read.distance_ls;
      ++summary.overspeed_entries;
      }
    calm = 0u;
    }
  // the speed from the last two readings a second or more apart - nearer ones differ by the reader's rounding
  auto const & r{attempt.readings};
  if(not r.empty())
    for(size_t i{r.size() - 1u}; i-- != 0u;)
      if(uint64_t const dt{r.back().ms - r[i].ms}; dt >= 1000u)
        {
        summary.end_speed_mm_s = std::abs(r[i].distance_ls - r.back().distance_ls) * mm_per_ls * 1000.0 / double(dt);
        break;
        }
  return summary;
  }

auto tracker_t::approach(uint64_t now_ms, std::optional<context_t> const & port) -> std::optional<attempt_t>
  {
  std::optional<attempt_t> done;
  if(attempt_ and port and port->station == attempt_->where.station and port->market_id == attempt_->where.market_id)
    {
    // the status file said otherwise for a moment, and the approach goes on
    ending_since_.reset();
    return done;
    }
  if(attempt_)
    {
    if(not ending_since_)
      ending_since_ = now_ms;
    if(now_ms - *ending_since_ < drop_wait_ms and not port)
      return done;
    done = finish(now_ms, outcome_e::broken_off);
    }
  if(port)
    {
    attempt_ = attempt_t{.where = *port, .started_ms = now_ms, .ended_ms = {}, .readings = {}, .outcome = {}};
    least_ls_ = 0.0;
    growing_ = 0u;
    }
  return done;
  }

auto estimate_seconds(std::span<reading_t const> earlier, uint64_t ms, double distance_ls) -> std::optional<double>
  {
  // the furthest back within a few seconds - the distance is written with three digits, and a longer stretch
  // makes the speed out of it finer
  constexpr uint64_t least_ms{1000u};
  constexpr uint64_t most_ms{4000u};
  for(reading_t const & before: earlier)
    if(before.ms < ms and ms - before.ms >= least_ms and ms - before.ms <= most_ms)
      {
      double const speed{(before.distance_ls - distance_ls) * 1000.0 / double(ms - before.ms)};
      if(speed <= 0.0)
        return std::nullopt;
      return distance_ls / speed;
      }
  return std::nullopt;
  }

auto checked_seconds(int32_t read, std::optional<double> estimate) -> int32_t
  {
  if(not estimate)
    return read;
  int32_t const guess{int32_t(std::lround(*estimate))};
  if(read < 0)
    return guess;
  int32_t const within_minute{read % 60};
  int32_t const minutes{std::max(0, int32_t(std::lround((*estimate - within_minute) / 60.0)))};
  int32_t const mended{minutes * 60 + within_minute};
  double const tolerance{std::max(1.5, *estimate * 0.25)};
  return std::abs(double(mended) - *estimate) <= tolerance ? mended : guess;
  }

auto tracker_t::reading(reading_t const & given) -> std::optional<attempt_t>
  {
  if(not active())
    return std::nullopt;
  auto & readings{attempt_->readings};
  reading_t read{given};
  read.read_seconds = given.seconds;
  // the picture may have been taken a moment before the approach was seen to begin
  attempt_->started_ms = std::min(attempt_->started_ms, read.ms);
  read.seconds = checked_seconds(given.seconds, estimate_seconds(readings, read.ms, read.distance_ls));
  if(readings.empty() or read.distance_ls < least_ls_)
    {
    least_ls_ = read.distance_ls;
    growing_ = 0u;
    }
  else if(read.distance_ls > least_ls_ + overshoot_margin_ls and read.distance_ls > readings.back().distance_ls)
    ++growing_;
  else
    growing_ = 0u;
  if(readings.size() < max_readings)
    readings.push_back(read);
  if(growing_ < 2u)
    return std::nullopt;

  // the port is behind: an approach of its own ends here, and the way back to it is a new one
  outcome_e const outcome{least_ls_ <= overshoot_within_ls ? outcome_e::overshot : outcome_e::broken_off};
  context_t const where{attempt_->where};
  auto done{finish(read.ms, outcome)};
  attempt_ = attempt_t{.where = where, .started_ms = read.ms, .ended_ms = {}, .readings = {read}, .outcome = {}};
  least_ls_ = read.distance_ls;
  growing_ = 0u;
  return done;
  }

auto tracker_t::dropped(uint64_t now_ms, std::string_view name, uint64_t market_id) -> std::optional<attempt_t>
  {
  if(not attempt_)
    return std::nullopt;
  context_t const & where{attempt_->where};
  bool const here{
    (market_id != 0u and where.market_id != 0u) ? market_id == where.market_id : name_likeness(name, where.station) >= 0.8
  };
  return finish(now_ms, here ? outcome_e::dropped : outcome_e::broken_off);
  }

auto tracker_t::last_distance() const noexcept -> std::optional<double>
  {
  if(not attempt_ or attempt_->readings.empty())
    return std::nullopt;
  return attempt_->readings.back().distance_ls;
  }

auto tracker_t::finish(uint64_t now_ms, outcome_e outcome) -> std::optional<attempt_t>
  {
  std::optional<attempt_t> done{std::move(attempt_)};
  attempt_.reset();
  ending_since_.reset();
  // an approach never read, or read only at the port after the drop, is nothing to learn from
  if(done and std::ranges::none_of(done->readings, [](reading_t const & r) { return r.distance_ls >= drop_zone_ls; }))
    return std::nullopt;
  if(done)
    {
    done->ended_ms = now_ms;
    done->outcome = outcome;
    }
  return done;
  }

auto attempt_line(attempt_t const & attempt) -> std::string
  {
  summary_t const s{summarise(attempt)};
  detail::attempt_json_t json{
    .started_ms = attempt.started_ms,
    .ended_ms = attempt.ended_ms,
    .outcome = std::string{outcome_name(attempt.outcome)},
    .overspeed = s.overspeed,
    .overspeed_from_ls = s.overspeed_from_ls,
    .first_overspeed_ls = s.first_overspeed_ls,
    .overspeed_entries = s.overspeed_entries,
    .least_seconds = s.least_seconds,
    .end_speed_mm_s = s.end_speed_mm_s,
    .last_ls = s.last_ls,
    .where = attempt.where,
    .readings = {}
  };
  for(reading_t const & read: attempt.readings)
    json.readings.emplace_back(read.ms - attempt.started_ms, read.distance_ls, read.seconds, read.read_seconds);
  std::string line;
  if(glz::write_json(json, line))
    return {};
  return line;
  }

auto parse_attempt_line(std::string const & line) -> std::optional<record_t>
  {
  detail::attempt_json_t json;
  if(glz::read<glz::opts{.error_on_unknown_keys = false}>(json, line))
    return std::nullopt;
  auto const outcome{outcome_of(json.outcome)};
  if(not outcome)
    return std::nullopt;
  // the summary made again from the readings - lines written before a change in how it is made say then what
  // the newer ones do
  attempt_t attempt{
    .where = {}, .started_ms = json.started_ms, .ended_ms = json.ended_ms, .readings = {}, .outcome = *outcome
  };
  for(auto const & [ms, distance_ls, seconds, read_seconds]: json.readings)
    attempt.readings.push_back(
      reading_t{.ms = json.started_ms + ms, .distance_ls = distance_ls, .seconds = seconds, .read_seconds = read_seconds}
    );
  return record_t{
    .station = std::move(json.where.station),
    .market_id = json.where.market_id,
    .ship = std::move(json.where.ship),
    .outcome = *outcome,
    .summary = attempt.readings.empty() ? summary_t{
      .overspeed = json.overspeed,
      .overspeed_from_ls = json.overspeed_from_ls,
      .first_overspeed_ls = json.first_overspeed_ls,
      .overspeed_entries = json.overspeed_entries,
      .least_seconds = json.least_seconds,
      .end_speed_mm_s = json.end_speed_mm_s,
      .last_ls = json.last_ls
    } : summarise(attempt)
  };
  }

auto record_of(attempt_t const & attempt) -> record_t
  {
  return record_t{
    .station = attempt.where.station,
    .market_id = attempt.where.market_id,
    .ship = attempt.where.ship,
    .outcome = attempt.outcome,
    .summary = summarise(attempt)
  };
  }

auto advise(std::span<record_t const> records, std::string_view station, uint64_t market_id, std::string_view ship)
  -> advice_t
  {
  advice_t advice;
  for(record_t const & r: records)
    {
    bool const here{(market_id != 0u and r.market_id != 0u) ? r.market_id == market_id : r.station == station};
    if(not here or r.ship != ship or not r.summary.overspeed)
      continue;
    double const from{r.summary.overspeed_from_ls};
    if(r.outcome == outcome_e::dropped)
      {
      ++advice.dropped;
      if(not advice.dropped_from_ls or from > *advice.dropped_from_ls)
        {
        advice.dropped_from_ls = from;
        advice.dropped_seconds = r.summary.least_seconds;
        }
      }
    else if(r.outcome == outcome_e::overshot)
      {
      ++advice.overshot;
      if(not advice.overshot_from_ls or from < *advice.overshot_from_ls)
        {
        advice.overshot_from_ls = from;
        advice.overshot_seconds = r.summary.least_seconds;
        }
      }
    }
  return advice;
  }
  }  // namespace hot_drop
