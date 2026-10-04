#include <backup.h>

#include <json_glaze.h>
#include <sqlite3.h>
#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>

// named, not anonymous - glaze's reflection needs the types to have linkage
namespace backup::detail
  {
struct mark_file_t
  {
  int64_t at{};
  uint64_t pictures{};
  };
  }  // namespace backup::detail

namespace backup
  {
namespace
  {
constexpr size_t tar_block{512u};

///\brief an octal number in a tar header field, zero-padded and ended with a NUL
auto put_octal(char * field, size_t width, uint64_t value) -> void
  {
  std::string const text{std::format("{:0{}o}", value, width - 1u)};
  std::memcpy(field, text.data(), std::min(text.size(), width - 1u));
  field[width - 1u] = '\0';
  }

///\brief the POSIX ustar header of a regular file
[[nodiscard]]
auto tar_header(std::string_view name, uint64_t size, int64_t mtime) -> std::array<char, tar_block>
  {
  std::array<char, tar_block> header{};
  std::memcpy(header.data(), name.data(), std::min<size_t>(name.size(), 100u));
  put_octal(&header[100], 8u, 0644u);
  put_octal(&header[108], 8u, 0u);
  put_octal(&header[116], 8u, 0u);
  put_octal(&header[124], 12u, size);
  put_octal(&header[136], 12u, static_cast<uint64_t>(std::max<int64_t>(mtime, 0)));
  header[156] = '0';
  std::memcpy(&header[257], "ustar", 6u);
  std::memcpy(&header[263], "00", 2u);
  // the checksum is summed with its own field taken as spaces
  std::memset(&header[148], ' ', 8u);
  uint64_t sum{};
  for(char const c: header)
    sum += static_cast<unsigned char>(c);
  std::string const text{std::format("{:06o}", sum)};
  std::memcpy(&header[148], text.data(), 6u);
  header[154] = '\0';
  header[155] = ' ';
  return header;
  }

///\brief a stream compressed on its way to a file
class zstd_writer_t
  {
public:
  zstd_writer_t(std::filesystem::path const & path, int level) : out_{path, std::ios::binary | std::ios::trunc}
    {
    if(context_ == nullptr)
      return;
    ZSTD_CCtx_setParameter(context_, ZSTD_c_compressionLevel, level);
    // the journals repeat themselves across files, far apart - long matching finds it; a window of 2^27 is what
    // a decoder takes without being told
    ZSTD_CCtx_setParameter(context_, ZSTD_c_enableLongDistanceMatching, 1);
    ZSTD_CCtx_setParameter(context_, ZSTD_c_windowLog, 27);
    // threads only where the library was built with them - without, the setting fails and it goes on in one
    ZSTD_CCtx_setParameter(context_, ZSTD_c_nbWorkers, 4);
    buffer_.resize(ZSTD_CStreamOutSize());
    }

  zstd_writer_t(zstd_writer_t const &) = delete;
  auto operator=(zstd_writer_t const &) -> zstd_writer_t & = delete;

  ~zstd_writer_t() { ZSTD_freeCCtx(context_); }

  [[nodiscard]]
  auto good() const -> bool
    { return context_ != nullptr and out_.good() and not failed_; }

  auto write(char const * data, size_t size) -> void { push(data, size, ZSTD_e_continue); }

  [[nodiscard]]
  auto finish() -> bool
    {
    push(nullptr, 0u, ZSTD_e_end);
    out_.close();
    return good() and not out_.fail();
    }

private:
  ZSTD_CCtx * context_{ZSTD_createCCtx()};
  std::ofstream out_;
  std::string buffer_;
  bool failed_{};

  auto push(char const * data, size_t size, ZSTD_EndDirective mode) -> void
    {
    if(not good())
      return;
    ZSTD_inBuffer input{data, size, 0u};
    for(;;)
      {
      ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0u};
      size_t const left{ZSTD_compressStream2(context_, &output, &input, mode)};
      if(ZSTD_isError(left))
        {
        failed_ = true;
        return;
        }
      out_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
      bool const done{mode == ZSTD_e_end ? left == 0u : input.pos == input.size};
      if(done)
        return;
      }
    }
  };

[[nodiscard]]
auto mark_path(std::filesystem::path const & destination) -> std::filesystem::path
  { return destination / "last_backup.json"; }

[[nodiscard]]
auto file_time(std::filesystem::path const & path) -> std::filesystem::file_time_type
  {
  std::error_code ec;
  auto const time{std::filesystem::last_write_time(path, ec)};
  return ec ? std::filesystem::file_time_type::min() : time;
  }
  }  // namespace

auto backup_live_db(std::filesystem::path const & destination, std::filesystem::path const & live_db_path) -> bool
  {
  // the journal thread copies it after a market reading while a full backup may be copying it too - both
  // through the same partial file
  static std::mutex copying;
  std::lock_guard const lock{copying};
  std::error_code ec;
  std::filesystem::create_directories(destination, ec);
  std::filesystem::path const target{destination / "live.sqlite"};
  std::filesystem::path const partial{destination / "live.sqlite.partial"};
  std::filesystem::remove(partial, ec);

  sqlite3 * src{};
  if(sqlite3_open_v2(live_db_path.string().c_str(), &src, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
    {
    sqlite3_close(src);
    return false;
    }
  sqlite3 * dst{};
  if(sqlite3_open(partial.string().c_str(), &dst) != SQLITE_OK)
    {
    sqlite3_close(dst);
    sqlite3_close(src);
    return false;
    }

  // the online backup API copies a consistent snapshot even while another connection is writing the
  // source through its own WAL - a plain file copy could catch it mid-write
  bool ok{false};
  if(sqlite3_backup * const b{sqlite3_backup_init(dst, "main", src, "main")}; b != nullptr)
    {
    ok = sqlite3_backup_step(b, -1) == SQLITE_DONE;
    sqlite3_backup_finish(b);
    }
  sqlite3_close(dst);
  sqlite3_close(src);

  if(ok)
    std::filesystem::rename(partial, target, ec);
  else
    std::filesystem::remove(partial, ec);
  return ok and not ec;
  }

auto expand_home(std::string_view path) -> std::filesystem::path
  {
  if(path == "~" or path.starts_with("~/"))
    if(char const * const home{std::getenv("HOME")}; home != nullptr and *home != '\0')
      return std::filesystem::path{home} / std::filesystem::path{path.substr(std::min<size_t>(path.size(), 2u))};
  return std::filesystem::path{path};
  }

auto write_tar_zst(std::filesystem::path const & archive, std::span<std::filesystem::path const> files, int level)
  -> bool
  {
  std::error_code ec;
  std::filesystem::create_directories(archive.parent_path(), ec);
  std::filesystem::path const partial{archive.string() + ".partial"};
  {
  zstd_writer_t out{partial, level};
  std::string content;
  for(std::filesystem::path const & file: files)
    {
    std::ifstream in{file, std::ios::binary};
    if(not in)
      return false;
    content.assign(std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{});
    auto const mtime{std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::clock_cast<std::chrono::system_clock>(file_time(file)).time_since_epoch()
    )
                       .count()};
    auto const header{tar_header(file.filename().string(), content.size(), mtime)};
    out.write(header.data(), header.size());
    out.write(content.data(), content.size());
    std::array<char, tar_block> const padding{};
    if(size_t const rest{content.size() % tar_block}; rest != 0u)
      out.write(padding.data(), tar_block - rest);
    }
  // the end of an archive is two empty blocks
  std::array<char, tar_block * 2u> const end{};
  out.write(end.data(), end.size());
  if(not out.finish())
    return false;
  }
  std::filesystem::rename(partial, archive, ec);
  return not ec;
  }

auto run(
  std::filesystem::path const & destination,
  std::filesystem::path const & journal_dir,
  std::filesystem::path const & codex_dir,
  std::filesystem::path const & live_db_path,
  int level
) -> summary_t
  {
  summary_t summary;
  std::error_code ec;

  if(summary.live_db_copied = backup_live_db(destination, live_db_path); not summary.live_db_copied)
    summary.errors.push_back(std::format("live.sqlite could not be copied from {}", live_db_path.string()));

  // the journals by month, from their names: Journal.2026-09-28T213338.01.log
  std::map<std::string, std::vector<std::filesystem::path>> months;
  for(auto const & entry: std::filesystem::directory_iterator{journal_dir, ec})
    if(auto const name{entry.path().filename().string()};
       name.starts_with("Journal.") and name.ends_with(".log") and name.size() > 15u)
      months[name.substr(8, 7)].push_back(entry.path());

  for(auto & [month, files]: months)
    {
    std::ranges::sort(files);
    std::filesystem::path const archive{destination / std::format("journals-{}.tar.zst", month)};
    // packed again only when a journal of the month is newer than its archive - a month gone by stays as it is
    auto const packed{file_time(archive)};
    if(std::filesystem::exists(archive, ec)
       and std::ranges::none_of(files, [&](auto const & file) { return file_time(file) > packed; }))
      continue;
    if(write_tar_zst(archive, files, level))
      summary.months.push_back(month);
    else
      summary.errors.push_back(std::format("the journals of {} could not be packed into {}", month, archive.string()));
    }

  // the pictures and their lists, copied when missing or newer - and never deleted from the backup
  std::filesystem::path const codex_copy{destination / "codex"};
  for(auto const & entry: std::filesystem::recursive_directory_iterator{codex_dir, ec})
    {
    if(not entry.is_regular_file(ec) or entry.path().extension() == ".part" or entry.path().extension() == ".partial")
      continue;
    std::filesystem::path const target{codex_copy / entry.path().lexically_relative(codex_dir)};
    if(std::filesystem::exists(target, ec) and file_time(target) >= file_time(entry.path()))
      continue;
    std::filesystem::create_directories(target.parent_path(), ec);
    if(std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec))
      ++summary.pictures_copied;
    else
      summary.errors.push_back(std::format("{} could not be copied: {}", entry.path().string(), ec.message()));
    }
  return summary;
  }

auto kept_from_game(std::filesystem::path const & file_name) -> bool
  {
  std::string const name{file_name.filename().string()};
  return name == "AppConfigLocal.xml" or name == "GraphicsConfiguration.xml" or file_name.extension() == ".ini";
  }

auto game_dir_of(std::filesystem::path const & netlog_dir) -> std::filesystem::path
  {
  if(netlog_dir.filename() != "Logs")
    return {};
  return netlog_dir.parent_path();
  }

namespace
  {
///\brief the whole of a small file - the settings are a few kilobytes each
auto file_bytes(std::filesystem::path const & path) -> std::optional<std::string>
  {
  std::ifstream in{path, std::ios::binary};
  if(not in)
    return std::nullopt;
  return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  }

auto same_content(std::filesystem::path const & a, std::filesystem::path const & b) -> bool
  {
  std::error_code ec_a;
  std::error_code ec_b;
  if(std::filesystem::file_size(a, ec_a) != std::filesystem::file_size(b, ec_b) or ec_a or ec_b)
    return false;
  auto const left{file_bytes(a)};
  auto const right{file_bytes(b)};
  return left and right and *left == *right;
  }

///\brief one file into the backup when missing or newer, the copy it replaces kept as <name>.<its time>
auto keep_file(std::filesystem::path const & source, std::filesystem::path const & target, settings_summary_t & summary)
  -> void
  {
  std::error_code ec;
  auto const source_time{file_time(source)};
  if(std::filesystem::exists(target, ec))
    {
    auto const target_time{file_time(target)};
    if(target_time >= source_time)
      return;
    // the game writes some of its options again unchanged - only the time moves then, no copy is set aside
    if(same_content(source, target))
      {
      std::filesystem::last_write_time(target, source_time, ec);
      return;
      }
    std::string const stamp{std::format(
      "{:%Y%m%d-%H%M%S}",
      std::chrono::floor<std::chrono::seconds>(std::chrono::clock_cast<std::chrono::system_clock>(target_time))
    )};
    std::filesystem::rename(target, std::filesystem::path{target.string() + "." + stamp}, ec);
    if(ec)
      {
      summary.errors.push_back(std::format("{} could not be set aside: {}", target.string(), ec.message()));
      return;
      }
    }
  std::filesystem::create_directories(target.parent_path(), ec);
  // copy_file keeps no time - the copy is given the source's, so the next run sees it as up to date
  if(std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec))
    {
    std::filesystem::last_write_time(target, source_time, ec);
    ++summary.copied;
    }
  else
    summary.errors.push_back(std::format("{} could not be copied: {}", source.string(), ec.message()));
  }
  }  // namespace

auto copy_settings(std::filesystem::path const & destination, settings_sources_t const & sources) -> settings_summary_t
  {
  settings_summary_t summary;
  std::filesystem::path const settings{destination / "settings"};
  std::error_code ec;

  // a directory iterator throws when stepping fails half way - what was copied stays, the rest waits for the next run
  try
    {
    if(not sources.options_dir.empty() and std::filesystem::is_directory(sources.options_dir, ec))
      for(auto const & entry: std::filesystem::recursive_directory_iterator{sources.options_dir, ec})
        if(entry.is_regular_file(ec))
          keep_file(entry.path(), settings / "options" / entry.path().lexically_relative(sources.options_dir), summary);

    // the top of the game's directory only - the mods keep their .ini files there, beside the game's executable
    if(not sources.game_dir.empty() and std::filesystem::is_directory(sources.game_dir, ec))
      for(auto const & entry: std::filesystem::directory_iterator{sources.game_dir, ec})
        if(entry.is_regular_file(ec) and kept_from_game(entry.path()))
          keep_file(entry.path(), settings / "game" / entry.path().filename(), summary);

    if(not sources.tool_settings.empty() and std::filesystem::is_regular_file(sources.tool_settings, ec))
      keep_file(sources.tool_settings, settings / sources.tool_settings.filename(), summary);
    }
  catch(std::exception const & e)
    {
    summary.errors.push_back(std::format("the settings could not be gone through: {}", e.what()));
    }
  return summary;
  }

auto read_mark(std::filesystem::path const & destination) -> mark_t
  {
  detail::mark_file_t file{};
  std::string buffer;
  if(glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(file, mark_path(destination).string(), buffer))
    return {};
  return mark_t{.at = std::chrono::sys_seconds{std::chrono::seconds{file.at}}, .pictures = file.pictures};
  }

auto write_mark(std::filesystem::path const & destination, mark_t const & mark) -> void
  {
  detail::mark_file_t const file{.at = mark.at.time_since_epoch().count(), .pictures = mark.pictures};
  std::string text;
  if(glz::write_json(file, text))
    return;
  std::error_code ec;
  std::filesystem::create_directories(destination, ec);
  std::ofstream out{mark_path(destination), std::ios::binary | std::ios::trunc};
  out << text << '\n';
  }

auto count_pictures(std::filesystem::path const & codex_dir) -> uint64_t
  {
  uint64_t count{};
  std::error_code ec;
  for(auto const & entry: std::filesystem::recursive_directory_iterator{codex_dir, ec})
    if(entry.is_regular_file(ec) and entry.path().extension() == ".jpg")
      ++count;
  return count;
  }

auto due(mark_t const & last, std::chrono::sys_seconds now, uint64_t pictures, uint32_t every_days, uint32_t every_pictures)
  -> bool
  {
  if(last.at == std::chrono::sys_seconds{})
    return true;
  if(every_days != 0u and now - last.at >= std::chrono::days{every_days})
    return true;
  return every_pictures != 0u and pictures >= last.pictures + every_pictures;
  }
  }  // namespace backup
