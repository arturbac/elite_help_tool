#include <sky_album.h>
#include <codex.h>
#include <picture_records.h>
#include <eht_settings.h>

#include <glaze/glaze.hpp>
#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <algorithm>
#include <format>
#include <fstream>
#include <ranges>

namespace
  {
using codex_files::codex_dir;
using codex_files::file_safe;
using codex_files::spool_dir;

///\brief a layer that never answers must not hold the next picture back forever
constexpr std::chrono::seconds pending_limit{10};

[[nodiscard]]
auto album_dir() -> std::filesystem::path
  { return codex_dir() / "sky"; }

[[nodiscard]]
auto list_path() -> std::filesystem::path
  { return album_dir() / "sky.json"; }

[[nodiscard]]
auto escaped(std::string_view text) -> std::string
  {
  std::string out;
  out.reserve(text.size());
  for(char const c: text)
    switch(c)
      {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default:  out += c; break;
      }
  return out;
  }
  }  // namespace

auto sky_album_t::page_path() -> std::filesystem::path { return codex_dir() / "sky.html"; }

auto sky_album_t::load() -> void
  {
  loaded_ = true;
  std::error_code ec;
  if(not std::filesystem::exists(list_path(), ec))
    return describe_unlisted();
  std::string buffer;
  if(glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(entries_, list_path().string(), buffer))
    {
    // kept aside rather than written over - whatever it still holds may be read by hand
    std::filesystem::path const broken{list_path().string() + ".broken"};
    std::filesystem::rename(list_path(), broken, ec);
    spdlog::error("sky: {} could not be read, kept as {}; the pictures are described again", list_path().string(), broken.string());
    entries_.clear();
    }
  describe_unlisted();
  }

auto sky_album_t::describe_unlisted() -> void
  {
  // every picture is named after the journal's moment it was taken for, so one missing from the list is
  // described again out of the journals
  std::vector<std::string> unlisted;
  for(std::string & file: pictures::pictures_under(codex_dir(), "sky"))
    if(std::ranges::none_of(entries_, [&](entry_t const & entry) { return entry.file == file; }))
      unlisted.push_back(std::move(file));
  if(unlisted.empty() or journal_dir_.empty())
    return;
  for(pictures::sky_record_t & record: pictures::rebuild_sky(unlisted, journal_dir_))
    entries_.push_back(entry_t{
      .file = std::move(record.file),
      .taken = std::move(record.taken),
      .kind = std::move(record.kind),
      .system = std::move(record.system),
      .body = std::move(record.body),
      .detail = std::move(record.detail),
      .first = record.first
    });
  // the names begin with the moment, so their order is the order the pictures were taken in
  std::ranges::sort(entries_, {}, [](entry_t const & entry) { return std::filesystem::path{entry.file}.filename().string(); });
  spdlog::info("sky: {} pictures described again out of the journals", unlisted.size());
  save();
  write_page();
  }

auto sky_album_t::save() const -> void
  {
  std::string text;
  if(glz::write<glz::opts{.prettify = true}>(entries_, text))
    return;
  std::filesystem::path const partial{list_path().string() + ".part"};
    {
    std::ofstream out{partial, std::ios::binary | std::ios::trunc};
    out << text << '\n';
    if(not out)
      {
      spdlog::error("sky: could not write {}", partial.string());
      return;
      }
    }
  std::error_code ec;
  std::filesystem::rename(partial, list_path(), ec);
  if(ec)
    spdlog::error("sky: could not put {} in place: {}", list_path().string(), ec.message());
  }

auto sky_album_t::kept(std::string const & body) -> bool
  {
  if(not loaded_)
    load();
  return std::ranges::any_of(entries_, [&](entry_t const & entry) { return entry.body == body; });
  }

auto sky_album_t::ask(entry_t subject, std::chrono::milliseconds after) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->exploration.sky_pictures or subject.body.empty() or kept(subject.body))
    return;
  if(pending_ and pending_->subject.body == subject.body)
    return;
  due_.clear();
  due_.push_back(
    due_t{.subject = std::move(subject), .at = std::chrono::steady_clock::now() + after, .suffix = {}, .moment = moment_}
  );
  }

auto sky_album_t::ask_series(entry_t subject, std::span<std::chrono::milliseconds const> after) -> void
  {
  if(after.size() == 1u)
    return ask(std::move(subject), after.front());
  auto const cfg{eht::settings()};
  if(not cfg->exploration.sky_pictures or subject.body.empty() or kept(subject.body) or after.empty())
    return;
  due_.clear();
  auto const now{std::chrono::steady_clock::now()};
  for(std::chrono::milliseconds const delay: after)
    due_.push_back(
      due_t{
        .subject = subject,
        .at = now + delay,
        .suffix = std::format("-{:.1f}s", std::chrono::duration<double>(delay).count()),
        .moment = moment_
      }
    );
  std::ranges::sort(due_, {}, &due_t::at);
  }

auto sky_album_t::describe(std::string const & system, entry_t const & known) -> void
  {
  auto const fill = [&](entry_t & entry)
  {
    if(entry.kind != known.kind or entry.system != system)
      return false;
    entry.body = known.body;
    entry.detail = known.detail;
    entry.first = known.first;
    return true;
  };
  for(due_t & due: due_)
    fill(due.subject);
  if(pending_)
    fill(pending_->subject);
  if(not loaded_)
    load();
  bool changed{};
  for(entry_t & entry: entries_)
    // only what was taken without a description - an older picture of the system keeps its own
    if(entry.detail.empty())
      changed = fill(entry) or changed;
  if(changed)
    {
    save();
    write_page();
    }
  }

auto sky_album_t::request(std::chrono::steady_clock::time_point) -> overlay::capture_t
  {
  auto const cfg{eht::settings()};
  // the moment, made unlike the codex's and the scanner's numbers - neither is to be taken for this one
  uint64_t const id{
    uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
               .count())
    * 4u
  };
  return overlay::capture_t{
    .id = id,
    .path = (spool_dir() / std::format("sky_{}.ppm", id)).string(),
    .size = cfg->exploration.sky_size,
    .delay_ms = 0u,
    .quiet = true,
    // the middle screen's own shape - a star or a planet is a landscape, not a square
    .aspect = 16.f / 9.f
  };
  }

auto sky_album_t::offer(entry_t subject) -> void
  {
  auto const cfg{eht::settings()};
  if(not cfg->exploration.sky_pictures or subject.body.empty() or pending_ or not due_.empty() or kept(subject.body))
    return;
  auto const now{std::chrono::steady_clock::now()};
  // one every two seconds is enough to have one taken just before the scanner, whenever it is opened
  constexpr std::chrono::seconds every{2};
  if(candidate_ and candidate_->subject.body == subject.body and now - candidate_->at < every)
    return;
  std::error_code ec;
  std::filesystem::create_directories(spool_dir(), ec);
  if(ec)
    return;
  request_ = request(now);
  pending_ = pending_t{
    .spool = request_.path, .subject = std::move(subject), .suffix = {}, .moment = {}, .asked = now, .candidate = true
  };
  }

auto sky_album_t::hold(std::string const & body) -> void
  {
  // taken a moment ago - older, and the ship may have been looking elsewhere
  constexpr std::chrono::seconds fresh{6};
  if(candidate_ and candidate_->subject.body == body and std::chrono::steady_clock::now() - candidate_->at < fresh)
    {
    held_ = std::move(candidate_);
    spdlog::info("sky: the view of {} before the scanner is held", body);
    }
  candidate_.reset();
  }

auto sky_album_t::keep_held(std::string const & body, std::chrono::sys_seconds moment) -> bool
  {
  if(not held_ or held_->subject.body != body)
    return false;
  aside_t aside{std::move(*held_)};
  held_.reset();
  return aside.image != nullptr and keep(*aside.image, std::move(aside.subject), {}, moment);
  }

auto sky_album_t::tick(bool view_clear) -> void
  {
  if(due_.empty() or pending_)
    return;
  if(not view_clear)
    {
    // the view is a menu or the ship is somewhere else - the moment the pictures were for has passed
    due_.clear();
    return;
    }
  auto const now{std::chrono::steady_clock::now()};
  if(now < due_.front().at)
    return;

  std::error_code ec;
  std::filesystem::create_directories(spool_dir(), ec);
  if(ec)
    return;

  request_ = request(now);
  pending_ = pending_t{
    .spool = request_.path,
    .subject = std::move(due_.front().subject),
    .suffix = std::move(due_.front().suffix),
    .moment = due_.front().moment,
    .asked = now
  };
  due_.pop_front();
  }

auto sky_album_t::collect() -> bool
  {
  if(not pending_)
    return false;
  // read before the new picture is on the disk, or it would be taken for one the list lost
  if(not loaded_)
    load();

  std::error_code ec;
  if(not std::filesystem::exists(pending_->spool, ec))
    {
    if(std::chrono::steady_clock::now() - pending_->asked > pending_limit)
      pending_.reset();
    return false;
    }

  QImage const image{QString::fromStdString(pending_->spool.string())};
  std::filesystem::remove(pending_->spool, ec);
  entry_t subject{std::move(pending_->subject)};
  std::string const suffix{std::move(pending_->suffix)};
  std::chrono::sys_seconds const moment{pending_->moment};
  bool const candidate{pending_->candidate};
  pending_.reset();
  if(image.isNull())
    return false;
  if(candidate)
    {
    candidate_ = aside_t{
      .image = std::make_shared<QImage const>(image), .subject = std::move(subject), .at = std::chrono::steady_clock::now()
    };
    return false;
    }
  return keep(image, std::move(subject), suffix, moment);
  }

auto sky_album_t::keep(QImage const & image, entry_t subject, std::string const & suffix, std::chrono::sys_seconds moment)
  -> bool
  {
  std::error_code ec;

  std::filesystem::path const relative{
    std::filesystem::path{"sky"} / file_safe(subject.system) / (pictures::stem(moment, subject.body) + suffix + ".jpg")
  };
  std::filesystem::path const target{codex_dir() / relative};
  std::filesystem::create_directories(target.parent_path(), ec);
  if(not image.save(QString::fromStdString(target.string()), "JPG", int(eht::settings()->exploration.jpeg_quality)))
    {
    spdlog::error("sky: the picture could not be saved as {}", target.string());
    return false;
    }

  subject.file = relative.generic_string();
  // the journal's clock, as the codex has it - the same moment a rebuilt list would give
  subject.taken = std::format("{:%Y-%m-%d %H:%M:%S}", moment);
  spdlog::info("sky: {} {} kept as {}", subject.kind, subject.body, target.string());
  if(not suffix.empty())
    subject.taken += std::format("  ({} after)", suffix.substr(1));
  entries_.push_back(std::move(subject));
  save();
  write_page();
  return true;
  }

auto sky_album_t::write_page() const -> void
  {
  std::string html{
    "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Sky album</title>\n<style>\n"
    ":root{color-scheme:dark;--ground:#0b0f17;--card:#141a26;--text:#d8dde6;--dim:#8a93a3;--first:#3cb371}\n"
    "body{margin:0;padding:16px;background:var(--ground);color:var(--text);font:15px/1.4 system-ui,sans-serif}\n"
    "h1{font-weight:500;margin:0 0 4px} p.lead{color:var(--dim);margin:0 0 16px}\n"
    ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(420px,1fr));gap:14px}\n"
    ".card{background:var(--card);border-radius:8px;overflow:hidden}\n"
    ".card img{display:block;width:100%;aspect-ratio:16/9;object-fit:cover}\n"
    ".card div{padding:8px 10px} .name{font-weight:600} .meta{color:var(--dim);font-size:13px}\n"
    ".first{color:var(--first);font-weight:600}\n"
    "@media (max-width:480px){.grid{grid-template-columns:1fr}}\n"
    "</style></head><body>\n<h1>Sky album</h1>\n"
  };
  size_t const stars{static_cast<size_t>(std::ranges::count(entries_, std::string{"star"}, &entry_t::kind))};
  html += std::format(
    "<p class=\"lead\">{} stars and {} planets, taken as the ship looked at them - the newest first.</p>\n"
    "<div class=\"grid\">\n",
    stars,
    entries_.size() - stars
  );
  for(entry_t const & entry: entries_ | std::views::reverse)
    html += std::format(
      "<figure class=\"card\" style=\"margin:0\"><a href=\"{0}\"><img src=\"{0}\" loading=\"lazy\" alt=\"{1}\"></a>"
      "<div><div class=\"name\">{1}{2}</div><div>{3}</div><div class=\"meta\">{4} - {5}</div></div></figure>\n",
      escaped(entry.file),
      escaped(entry.body),
      entry.first ? " <span class=\"first\">first discovery</span>" : "",
      escaped(entry.detail),
      escaped(entry.system),
      escaped(entry.taken)
    );
  html += "</div>\n</body></html>\n";

  std::filesystem::path const partial{page_path().string() + ".part"};
    {
    std::ofstream out{partial, std::ios::binary | std::ios::trunc};
    out << html;
    if(not out)
      return;
    }
  std::error_code ec;
  std::filesystem::rename(partial, page_path(), ec);
  }
