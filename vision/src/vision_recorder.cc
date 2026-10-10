#include <vision_recorder.h>

#include <backup.h>
#include <json_glaze.h>

#include <png.h>
#include <turbojpeg.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <format>
#include <fstream>
#include <thread>

namespace vision
  {
namespace
  {
  constexpr auto lenient{glz::opts{.error_on_unknown_keys = false}};
  ///\brief a sample older than this is the game no longer drawing - paused, minimised or gone
  constexpr uint64_t stale_ms{5000u};
  ///\brief how often the newest journal is looked for
  constexpr std::chrono::seconds journal_look{2};
  ///\brief how often the dataset's size is measured again - the lines beside the pictures are not counted
  /// as they are written
  constexpr std::chrono::hours remeasure{1};
  ///\brief how often the log says how much was recorded
  constexpr std::chrono::minutes report_every{10};
  }  // namespace

// glaze reflects a type only when it has linkage - the lines' shapes cannot live in the anonymous namespace
namespace detail
  {
  struct frame_json_t
    {
    std::string file;
    uint64_t taken_ms;
    uint64_t seq;
    uint32_t width;
    uint32_t height;
    float left;
    float top;
    float region_width;
    float region_height;
    uint32_t surface_width;
    uint32_t surface_height;
    float difference;
    std::string commander;
    std::string socket;
    uint64_t status_ms;
    glz::raw_json status;
    };

  struct shot_json_t
    {
    std::string file;
    uint64_t asked_ms;
    std::string reason;
    uint32_t width;
    uint32_t height;
    std::string commander;
    std::string socket;
    uint64_t status_ms;
    glz::raw_json status;
    };

  struct status_json_t
    {
    uint64_t ms;
    bool flags_changed;
    glz::raw_json status;
    };

  struct event_json_t
    {
    uint64_t ms;
    std::string timestamp;
    std::string event;
    };
  }  // namespace detail

namespace
  {
  [[nodiscard]]
  auto is_day_name(std::string_view name) noexcept -> bool
    {
    if(name.size() != 10u or name[4] != '-' or name[7] != '-')
      return false;
    for(size_t at{}; at != name.size(); ++at)
      if(at != 4u and at != 7u and (name[at] < '0' or name[at] > '9'))
        return false;
    return true;
    }

  [[nodiscard]]
  auto same_place(overlay::sample_header_t const & a, overlay::sample_header_t const & b) noexcept -> bool
    {
    return a.width == b.width and a.height == b.height and a.left == b.left and a.top == b.top
           and a.region_width == b.region_width and a.region_height == b.region_height;
    }

  [[nodiscard]]
  auto now_ms() -> uint64_t
    {
    return uint64_t(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    );
    }

  [[nodiscard]]
  auto read_text(std::filesystem::path const & path) -> std::optional<std::string>
    {
    std::ifstream in{path, std::ios::binary};
    if(not in)
      return std::nullopt;
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    }
  }  // namespace

auto sample_valid(
  overlay::sample_header_t const & before, overlay::sample_header_t const & after, uint64_t seen
) noexcept -> bool
  {
  return before.magic == overlay::sample_header_t{}.magic and before.seq != seen and (before.seq & 1u) == 0u
         and before.width != 0u and before.height != 0u and before.width <= overlay::sample_max_side
         and before.height <= overlay::sample_max_side and after.seq == before.seq;
  }

auto rgb_of(std::span<uint8_t const> rgba) -> std::vector<uint8_t>
  {
  std::vector<uint8_t> rgb(rgba.size() / 4u * 3u);
  for(size_t at{}; at != rgba.size() / 4u; ++at)
    std::memcpy(rgb.data() + at * 3u, rgba.data() + at * 4u, 3u);
  return rgb;
  }

auto read_sample(std::filesystem::path const & path, uint64_t seen) -> std::optional<sample_read_t>
  {
  std::ifstream in{path, std::ios::binary};
  if(not in)
    return std::nullopt;
  overlay::sample_header_t before{};
  in.read(reinterpret_cast<char *>(&before), sizeof(before));
  // the head alone first - most rounds find the sample already seen, and read no further
  if(not in or before.seq == seen or not sample_valid(before, before, seen))
    return std::nullopt;
  std::vector<uint8_t> rgba(size_t{before.width} * before.height * 4u);
  in.read(reinterpret_cast<char *>(rgba.data()), std::streamsize(rgba.size()));
  overlay::sample_header_t after{};
  in.seekg(0);
  in.read(reinterpret_cast<char *>(&after), sizeof(after));
  // written over meanwhile - the next one will do
  if(not in or not sample_valid(before, after, seen))
    return std::nullopt;
  return sample_read_t{.header = before, .rgb = rgb_of(rgba)};
  }

auto thumbnail(std::span<uint8_t const> rgb, uint32_t width, uint32_t height) -> thumbnail_t
  {
  thumbnail_t thumb{};
  if(width == 0u or height == 0u or rgb.size() < size_t{width} * height * 3u)
    return thumb;
  for(uint32_t cy{}; cy != thumb_height; ++cy)
    {
    uint32_t const y0{cy * height / thumb_height};
    uint32_t const y1{std::min(height, std::max(y0 + 1u, (cy + 1u) * height / thumb_height))};
    for(uint32_t cx{}; cx != thumb_width; ++cx)
      {
      uint32_t const x0{cx * width / thumb_width};
      uint32_t const x1{std::min(width, std::max(x0 + 1u, (cx + 1u) * width / thumb_width))};
      uint64_t sum{};
      for(uint32_t y{y0}; y != y1; ++y)
        for(uint32_t x{x0}; x != x1; ++x)
          {
          uint8_t const * const p{rgb.data() + (size_t{y} * width + x) * 3u};
          sum += 299u * p[0] + 587u * p[1] + 114u * p[2];
          }
      uint64_t const count{uint64_t{y1 - y0} * (x1 - x0) * 1000u};
      thumb[size_t{cy} * thumb_width + cx] = static_cast<uint8_t>((sum + count / 2u) / count);
      }
    }
  return thumb;
  }

auto difference(thumbnail_t const & a, thumbnail_t const & b) noexcept -> float
  {
  uint32_t sum{};
  for(size_t at{}; at != a.size(); ++at)
    sum += static_cast<uint32_t>(std::abs(int{a[at]} - int{b[at]}));
  return float(sum) / float(a.size());
  }

auto keeper_t::judge(
  thumbnail_t const & thumb,
  overlay::sample_header_t const & header,
  uint32_t every_ms,
  float min_difference,
  uint32_t keep_every_ms
) const -> verdict_t
  {
  if(not thumb_)
    return {.keep = true, .difference = -1.f};
  float const changed{difference(thumb, *thumb_)};
  // the layer started again with a clock behind the last picture, or another part of the screen
  if(header.taken_ms < header_.taken_ms or not same_place(header, header_))
    return {.keep = true, .difference = changed};
  uint64_t const since{header.taken_ms - header_.taken_ms};
  if(since < every_ms)
    return {.keep = false, .difference = changed};
  return {.keep = changed >= min_difference or since >= keep_every_ms, .difference = changed};
  }

auto keeper_t::kept(thumbnail_t const & thumb, overlay::sample_header_t const & header) -> void
  {
  thumb_ = thumb;
  header_ = header;
  }

auto day_name(uint64_t ms) -> std::string
  {
  std::chrono::sys_time<std::chrono::milliseconds> const at{std::chrono::milliseconds{ms}};
  return std::format("{:%F}", std::chrono::floor<std::chrono::days>(at));
  }

auto days_to_drop(std::vector<day_size_t> days, uint64_t limit_bytes, std::string_view today)
  -> std::vector<std::string>
  {
  std::ranges::sort(days, {}, &day_size_t::name);
  uint64_t total{};
  for(day_size_t const & day: days)
    total += day.bytes;
  std::vector<std::string> drop;
  for(day_size_t const & day: days)
    {
    if(total <= limit_bytes)
      break;
    if(day.name == today)
      continue;
    total -= day.bytes;
    drop.push_back(day.name);
    }
  return drop;
  }

auto measure_days(std::filesystem::path const & dataset) -> std::vector<day_size_t>
  {
  std::vector<day_size_t> days;
  std::error_code ec;
  for(auto const & entry: std::filesystem::directory_iterator{dataset, ec})
    {
    std::string name{entry.path().filename().string()};
    if(not entry.is_directory(ec) or not is_day_name(name))
      continue;
    uint64_t bytes{};
    for(auto const & file: std::filesystem::recursive_directory_iterator{entry.path(), ec})
      if(file.is_regular_file(ec))
        bytes += file.file_size(ec);
    days.push_back(day_size_t{.name = std::move(name), .bytes = bytes});
    }
  return days;
  }

auto frame_line(frame_record_t const & record) -> std::string
  {
  overlay::sample_header_t const & h{record.header};
  detail::frame_json_t const json{
    .file = record.file,
    .taken_ms = h.taken_ms,
    .seq = h.seq,
    .width = h.width,
    .height = h.height,
    .left = h.left,
    .top = h.top,
    .region_width = h.region_width,
    .region_height = h.region_height,
    .surface_width = h.surface_width,
    .surface_height = h.surface_height,
    .difference = record.difference,
    .commander = record.commander,
    .socket = record.socket,
    .status_ms = record.status_ms,
    .status = {record.status.empty() ? std::string{"null"} : record.status}
  };
  std::string line;
  if(glz::write_json(json, line))
    return {};
  return line;
  }

auto status_line(uint64_t ms, std::string_view status, bool flags_changed) -> std::string
  {
  detail::status_json_t const json{.ms = ms, .flags_changed = flags_changed, .status = {std::string{status}}};
  std::string line;
  if(glz::write_json(json, line))
    return {};
  return line;
  }

auto event_line(uint64_t ms, std::string_view timestamp, std::string_view event) -> std::string
  {
  detail::event_json_t const json{.ms = ms, .timestamp = std::string{timestamp}, .event = std::string{event}};
  std::string line;
  if(glz::write_json(json, line))
    return {};
  return line;
  }

auto parse_status(std::string_view text) -> std::optional<status_flags_t>
  {
  std::string const buffer{text};
  status_flags_t flags{};
  if(buffer.empty() or glz::read<lenient>(flags, buffer))
    return std::nullopt;
  return flags;
  }

auto status_left_behind(std::optional<status_flags_t> const & flags, uint64_t written_ms, uint64_t now_ms) noexcept
  -> bool
  {
  // on foot Flags is 0 and Flags2 is not, so both are asked
  return flags and flags->Flags == 0u and flags->Flags2 == 0u and written_ms + status_silent_ms < now_ms;
  }

auto parse_event(std::string_view line) -> std::optional<journal_event_t>
  {
  std::string const buffer{line};
  journal_event_t event{};
  if(buffer.empty() or glz::read<lenient>(event, buffer) or event.event.empty())
    return std::nullopt;
  return event;
  }

journal_tail_t::journal_tail_t(std::filesystem::path dir) : dir_{std::move(dir)} {}

auto journal_tail_t::note(std::string_view line) -> void
  {
  if(line.find("\"Commander\"") == std::string_view::npos)
    return;
  if(auto const event{parse_event(line)}; event and event->event == "Commander" and not event->Name.empty())
    commander_ = event->Name;
  }

auto journal_tail_t::open_newest() -> void
  {
  looked_ = std::chrono::steady_clock::now();
  std::filesystem::path newest;
  std::error_code ec;
  // Journal.2026-10-01T123456.01.log - the names sort as the moments they began
  for(auto const & entry: std::filesystem::directory_iterator{dir_, ec})
    {
    std::string const name{entry.path().filename().string()};
    if(name.starts_with("Journal.") and name.ends_with(".log") and (newest.empty() or entry.path() > newest))
      newest = entry.path();
    }
  bool const first{std::exchange(first_, false)};
  if(newest.empty() or newest == file_)
    return;
  file_ = newest;
  offset_ = 0u;
  partial_.clear();
  if(not first)
    {
    spdlog::info("vision: following {}", file_.filename().string());
    return;
    }
  // already there at the start: its past is read only for who plays, and followed from its end
  std::ifstream in{file_, std::ios::binary};
  for(std::string line; std::getline(in, line);)
    {
    note(line);
    if(not in.eof())
      offset_ += line.size() + 1u;
    }
  spdlog::info("vision: following {} from its end, commander {}", file_.filename().string(), commander_);
  }

auto journal_tail_t::poll() -> std::vector<std::string>
  {
  if(first_ or std::chrono::steady_clock::now() - looked_ >= journal_look)
    open_newest();
  std::vector<std::string> lines;
  if(file_.empty())
    return lines;
  std::error_code ec;
  uint64_t const size{std::filesystem::file_size(file_, ec)};
  if(ec)
    return lines;
  if(size < offset_)
    {
    offset_ = 0u;
    partial_.clear();
    }
  if(size == offset_)
    return lines;
  std::ifstream in{file_, std::ios::binary};
  in.seekg(std::streamoff(offset_));
  std::string chunk(size - offset_, '\0');
  in.read(chunk.data(), std::streamsize(chunk.size()));
  chunk.resize(size_t(in.gcount()));
  offset_ += chunk.size();
  partial_ += chunk;
  size_t start{};
  for(size_t end{partial_.find('\n')}; end != std::string::npos; end = partial_.find('\n', start))
    {
    std::string_view line{std::string_view{partial_}.substr(start, end - start)};
    if(line.ends_with('\r'))
      line.remove_suffix(1u);
    if(not line.empty())
      {
      note(line);
      lines.emplace_back(line);
      }
    start = end + 1u;
    }
  partial_.erase(0u, start);
  return lines;
  }

auto write_png(std::filesystem::path const & path, std::span<uint8_t const> rgb, uint32_t width, uint32_t height)
  -> bool
  {
  if(rgb.size() < size_t{width} * height * 3u)
    return false;
  png_image image{};
  image.version = PNG_IMAGE_VERSION;
  image.width = width;
  image.height = height;
  image.format = PNG_FORMAT_RGB;
  // written aside and put in place whole, so a reader never finds half a picture under the name
  std::filesystem::path partial{path};
  partial += ".part";
  bool const ok{png_image_write_to_file(&image, partial.c_str(), 0, rgb.data(), 0, nullptr) != 0};
  png_image_free(&image);
  std::error_code ec;
  if(ok)
    std::filesystem::rename(partial, path, ec);
  else
    std::filesystem::remove(partial, ec);
  return ok and not ec;
  }

auto parse_shot_name(std::string_view file_name, std::string_view prefix) -> std::optional<shot_name_t>
  {
  if(not file_name.starts_with(prefix) or not file_name.ends_with(".ppm"))
    return std::nullopt;
  std::string_view rest{file_name.substr(prefix.size(), file_name.size() - prefix.size() - 4u)};
  auto const underscore{rest.find('_')};
  if(underscore == 0u or underscore == std::string_view::npos or underscore + 1u == rest.size())
    return std::nullopt;
  uint64_t ms{};
  for(char const c: rest.substr(0u, underscore))
    {
    if(c < '0' or c > '9')
      return std::nullopt;
    ms = ms * 10u + uint64_t(c - '0');
    }
  return shot_name_t{.asked_ms = ms, .reason = std::string{rest.substr(underscore + 1u)}};
  }

auto read_ppm(std::filesystem::path const & path) -> std::optional<picture_t>
  {
  std::ifstream in{path, std::ios::binary};
  std::string magic;
  uint32_t width{};
  uint32_t height{};
  uint32_t depth{};
  if(not(in >> magic >> width >> height >> depth) or magic != "P6" or depth != 255u or width == 0u or height == 0u
     or width > 16384u or height > 16384u)
    return std::nullopt;
  // a single whitespace ends the head
  in.get();
  picture_t picture{.width = width, .height = height, .rgb = std::vector<uint8_t>(size_t{width} * height * 3u)};
  in.read(reinterpret_cast<char *>(picture.rgb.data()), std::streamsize(picture.rgb.size()));
  if(size_t(in.gcount()) != picture.rgb.size())
    return std::nullopt;
  return picture;
  }

auto write_jpeg(
  std::filesystem::path const & path, std::span<uint8_t const> rgb, uint32_t width, uint32_t height, uint32_t quality
) -> bool
  {
  if(rgb.size() < size_t{width} * height * 3u)
    return false;
  tjhandle const handle{tjInitCompress()};
  if(handle == nullptr)
    return false;
  unsigned char * jpeg{};
  unsigned long size{};
  bool const encoded{
    tjCompress2(
      handle,
      rgb.data(),
      int(width),
      0,
      int(height),
      TJPF_RGB,
      &jpeg,
      &size,
      TJSAMP_420,
      int(std::clamp(quality, 1u, 100u)),
      TJFLAG_FASTDCT
    )
    == 0
  };
  bool written{};
  if(encoded)
    {
    // written aside and put in place whole, so a reader never finds half a picture under the name
    std::filesystem::path partial{path};
    partial += ".part";
      {
      std::ofstream out{partial, std::ios::binary | std::ios::trunc};
      out.write(reinterpret_cast<char const *>(jpeg), std::streamsize(size));
      written = bool(out);
      }
    std::error_code ec;
    if(written)
      std::filesystem::rename(partial, path, ec);
    else
      std::filesystem::remove(partial, ec);
    written = written and not ec;
    }
  tjFree(jpeg);
  tjDestroy(handle);
  return written;
  }

auto shot_line(shot_record_t const & record) -> std::string
  {
  detail::shot_json_t const json{
    .file = record.file,
    .asked_ms = record.name.asked_ms,
    .reason = record.name.reason,
    .width = record.width,
    .height = record.height,
    .commander = record.commander,
    .socket = record.socket,
    .status_ms = record.status_ms,
    .status = {record.status.empty() ? std::string{"null"} : record.status}
  };
  std::string line;
  if(glz::write_json(json, line))
    return {};
  return line;
  }

recorder_t::recorder_t(std::filesystem::path journal_dir, std::filesystem::path sample_path) :
    journal_dir_{std::move(journal_dir)},
    sample_path_{std::move(sample_path)},
    journal_{journal_dir_}
  {
  // overlay-alt_sample.bin - the socket's name, which tells the accounts apart
  socket_ = sample_path_.stem().string();
  if(socket_.ends_with("_sample"))
    socket_.resize(socket_.size() - std::string_view{"_sample"}.size());
  // the whole pictures lie beside the sample, named after the same socket
  spool_ = sample_path_.parent_path();
  shot_prefix_ = socket_ + "_shot_";
  }

auto recorder_t::counted(std::string const & day, std::filesystem::path const & file) -> void
  {
  std::error_code ec;
  uint64_t const size{std::filesystem::file_size(file, ec)};
  if(ec)
    return;
  if(auto const it{std::ranges::find(days_, day, &day_size_t::name)}; it != days_.end())
    it->bytes += size;
  bytes_ += size;
  }

auto recorder_t::look_at_shots(eht::vision_settings_t const & cfg) -> void
  {
  std::error_code ec;
  std::vector<std::pair<std::filesystem::path, shot_name_t>> found;
  for(auto const & entry: std::filesystem::directory_iterator{spool_, ec})
    if(auto name{parse_shot_name(entry.path().filename().string(), shot_prefix_)}; name)
      found.emplace_back(entry.path(), std::move(*name));
  for(auto const & [path, name]: found)
    {
    auto const picture{read_ppm(path)};
    std::filesystem::remove(path, ec);
    if(not picture)
      {
      spdlog::warn("vision: {} is no picture", path.filename().string());
      continue;
      }
    std::string const today{day_name(name.asked_ms)};
    if(not make_room(cfg, today))
      continue;
    std::string const file{std::format("{}_{}.jpg", name.asked_ms, name.reason)};
    std::filesystem::path const dir{dataset_ / today};
    std::filesystem::create_directories(dir, ec);
    bool const written{write_jpeg(dir / file, picture->rgb, picture->width, picture->height, cfg.shot_jpeg_quality)};
    note_write(written, dir / file);
    if(not written)
      continue;
    counted(today, dir / file);
    ++shots_;
    append(
      today,
      "shots.jsonl",
      shot_line(
        shot_record_t{
          .file = file,
          .name = name,
          .width = picture->width,
          .height = picture->height,
          .commander = journal_.commander(),
          .socket = socket_,
          .status_ms = status_ms_,
          .status = status_
        }
      )
    );
    }
  }

auto recorder_t::append(std::string_view day, std::string_view file, std::string_view line) -> void
  {
  if(line.empty())
    return;
  std::filesystem::path const dir{dataset_ / day};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::ofstream out{dir / file, std::ios::binary | std::ios::app};
  out << line << '\n';
  note_write(bool(out), dir / file);
  }

auto recorder_t::note_write(bool written, std::filesystem::path const & path) -> void
  {
  if(not written and not write_failing_)
    spdlog::warn("vision: could not write {} - the next failures are not said until a write succeeds", path.string());
  else if(written and write_failing_)
    spdlog::info("vision: writes again, {}", path.string());
  write_failing_ = not written;
  }

auto recorder_t::look_at_journal(uint64_t now) -> void
  {
  for(std::string const & line: journal_.poll())
    if(auto const event{parse_event(line)}; event)
      append(day_name(now), "events.jsonl", event_line(now, event->timestamp, event->event));
  }

auto recorder_t::look_at_status(uint64_t now) -> void
  {
  std::error_code ec;
  auto const written{std::filesystem::last_write_time(journal_dir_ / "Status.json", ec)};
  if(ec or written == status_time_)
    return;
  auto const text{read_text(journal_dir_ / "Status.json")};
  auto const flags{text ? parse_status(*text) : std::nullopt};
  // caught half written - looked at again in the next round
  if(not flags)
    return;
  status_time_ = written;
  status_written_ms_ = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::clock_cast<std::chrono::system_clock>(written).time_since_epoch()
  )
                                  .count());
  std::string minified;
  std::string input{*text};
  glz::minify_json(input, minified);
  if(minified == status_)
    return;
  bool const flags_changed{not flags_ or *flags_ != *flags};
  status_ = std::move(minified);
  status_ms_ = now;
  flags_ = flags;
  append(day_name(now), "status.jsonl", status_line(now, status_, flags_changed));
  }

auto recorder_t::make_room(eht::vision_settings_t const & cfg, std::string const & today) -> bool
  {
  if(not measured_)
    {
    days_ = measure_days(dataset_);
    measured_ = true;
    }
  if(std::ranges::find(days_, today, &day_size_t::name) == days_.end())
    days_.push_back(day_size_t{.name = today, .bytes = 0u});
  auto const limit{static_cast<uint64_t>(std::max(0.0, cfg.limit_gb) * 1e9)};
  for(std::string const & name: days_to_drop(days_, limit, today))
    {
    std::error_code ec;
    std::filesystem::remove_all(dataset_ / name, ec);
    spdlog::info("vision: the dataset grew above {} GB, {} deleted", cfg.limit_gb, name);
    std::erase_if(days_, [&name](day_size_t const & day) { return day.name == name; });
    }
  uint64_t total{};
  for(day_size_t const & day: days_)
    total += day.bytes;
  if(total > limit)
    {
    if(not full_)
      spdlog::warn("vision: today alone holds more than {} GB - no more pictures until tomorrow", cfg.limit_gb);
    full_ = true;
    return false;
    }
  full_ = false;
  return true;
  }

auto recorder_t::look_at_sample(eht::vision_settings_t const & cfg, uint64_t now) -> void
  {
  auto const sample{read_sample(sample_path_, seen_seq_)};
  if(not sample)
    return;
  overlay::sample_header_t const & header{sample->header};
  seen_seq_ = header.seq;
  if(header.taken_ms + stale_ms < now or header.taken_ms > now + stale_ms)
    return;
  if(bool const left_behind{status_left_behind(flags_, status_written_ms_, now)}; left_behind != status_left_behind_)
    {
    status_left_behind_ = left_behind;
    if(left_behind)
      spdlog::info("vision: Status.json unwritten for over {} min and with no flags - frames not kept until it is",
                   status_silent_ms / 60'000u);
    else
      spdlog::info("vision: Status.json written again - frames kept");
    }
  if(status_left_behind_)
    return;
  thumbnail_t const thumb{thumbnail(sample->rgb, header.width, header.height)};
  auto const verdict{keeper_.judge(thumb, header, cfg.every_ms, cfg.min_difference, cfg.keep_every_s * 1000u)};
  if(not verdict.keep)
    return;
  std::string const today{day_name(header.taken_ms)};
  if(not make_room(cfg, today))
    return;
  std::string const name{std::format("{}.png", header.taken_ms)};
  std::filesystem::path const dir{dataset_ / today};
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  bool const written{write_png(dir / name, sample->rgb, header.width, header.height)};
  note_write(written, dir / name);
  if(not written)
    return;
  counted(today, dir / name);
  keeper_.kept(thumb, header);
  ++frames_;
  append(
    today,
    "frames.jsonl",
    frame_line(
      frame_record_t{
        .file = name,
        .header = header,
        .difference = verdict.difference,
        .commander = journal_.commander(),
        .socket = socket_,
        .status_ms = status_ms_,
        .status = status_
      }
    )
  );
  }

auto recorder_t::step(eht::vision_settings_t const & cfg, uint64_t now) -> void
  {
  if(std::filesystem::path const dataset{backup::expand_home(cfg.dataset_dir)}; dataset != dataset_)
    {
    dataset_ = dataset;
    measured_ = false;
    spdlog::info("vision: recording into {}", dataset_.string());
    }
  look_at_journal(now);
  look_at_status(now);
  look_at_sample(cfg, now);
  look_at_shots(cfg);
  }

auto recorder_t::run(std::stop_token stop) -> void
  {
  auto measured{std::chrono::steady_clock::now()};
  auto reported{measured};
  while(not stop.stop_requested())
    {
    auto const cfg{eht::settings()};
    if(cfg->vision.record)
      step(cfg->vision, now_ms());
    else
      // the journal still followed, so recording switched on later does not take the lines from meanwhile for new
      std::ignore = journal_.poll();

    auto const now{std::chrono::steady_clock::now()};
    if(now - measured >= remeasure)
      {
      measured = now;
      measured_ = false;
      }
    if(now - reported >= report_every)
      {
      reported = now;
      if(frames_ != 0u or shots_ != 0u)
        spdlog::info(
          "vision: {} pictures and {} whole ones, {:.1f} MB in the last {} minutes",
          frames_,
          shots_,
          double(bytes_) / 1e6,
          report_every.count()
        );
      frames_ = 0u;
      shots_ = 0u;
      bytes_ = 0u;
      }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }
  }  // namespace vision
