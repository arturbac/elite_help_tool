#include <planet_faces.h>
#include <codex.h>
#include <face_store.h>
#include <eht_settings.h>
#include <planet_face.h>

#include <spdlog/spdlog.h>

#include <QImage>
#include <QString>

#include <algorithm>
#include <format>
#include <fstream>
#include <optional>
#include <vector>

namespace
  {
using codex_files::codex_dir;
using codex_files::file_safe;
using codex_files::spool_dir;

///\brief how often a body's pictures are looked at again for one newer than its face
constexpr std::chrono::seconds recheck{10};

///\brief where the builds before the views went into live.sqlite kept the best view from the cockpit, and beside
/// it how good it was
[[nodiscard]]
auto old_approach_path(std::string const & body) -> std::filesystem::path
  { return codex_dir() / "approach" / (file_safe(body) + ".png"); }

struct sources_t
  {
  std::vector<std::filesystem::path> views;
  std::optional<std::filesystem::path> photo;
  ///\brief a view from the cockpit is kept in live.sqlite
  bool approach{};
  std::filesystem::file_time_type newest{std::filesystem::file_time_type::min()};
  };

auto adopt_old_view(std::filesystem::path const & live_db, std::string const & body) -> void;

[[nodiscard]]
auto sources_of(std::string const & system, std::string const & body, std::filesystem::path const & live_db) -> sources_t
  {
  sources_t sources;
  std::error_code ec;
  auto const note = [&](std::filesystem::path const & file)
  {
    if(auto const at{std::filesystem::last_write_time(file, ec)}; not ec)
      sources.newest = std::max(sources.newest, at);
  };
  for(auto const & entry: std::filesystem::directory_iterator{codex_dir() / "scanner" / file_safe(body), ec})
    if(entry.path().extension() == ".jpg")
      {
      sources.views.push_back(entry.path());
      note(entry.path());
      }
  std::ranges::sort(sources.views);
  // the sky album names its photos <moment>_<body>.jpg; the newest of them
  std::string const suffix{"_" + file_safe(body) + ".jpg"};
  for(auto const & entry: std::filesystem::directory_iterator{codex_dir() / "sky" / file_safe(system), ec})
    if(std::string const name{entry.path().filename().string()}; name.ends_with(suffix))
      if(not sources.photo or entry.path() > *sources.photo)
        sources.photo = entry.path();
  if(sources.photo)
    note(*sources.photo);
  adopt_old_view(live_db, body);
  if(auto const kept{face_store::stamp(live_db, body)}; kept)
    {
    sources.approach = true;
    sources.newest = std::max(sources.newest, std::chrono::clock_cast<std::chrono::file_clock>(kept->second));
    }
  return sources;
  }

[[nodiscard]]
auto to_image(QImage const & picture) -> planet_face::image_t
  {
  QImage const rgb{picture.convertToFormat(QImage::Format_RGB888)};
  planet_face::image_t image{.width = uint32_t(rgb.width()), .height = uint32_t(rgb.height()), .rgb = {}};
  image.rgb.resize(size_t{image.width} * image.height * 3u);
  for(int y{}; y != rgb.height(); ++y)
    std::copy_n(rgb.constScanLine(y), size_t{image.width} * 3u, image.rgb.data() + size_t(y) * image.width * 3u);
  return image;
  }

[[nodiscard]]
auto load(std::filesystem::path const & file) -> std::optional<planet_face::image_t>
  {
  QImage const picture{QString::fromStdString(file.string())};
  if(picture.isNull())
    return std::nullopt;
  return to_image(picture);
  }

///\brief a view from the cockpit an older build kept as files, taken into live.sqlite the first time the body is asked
/// about; the files stay where they are
auto adopt_old_view(std::filesystem::path const & live_db, std::string const & body) -> void
  {
  std::error_code ec;
  std::filesystem::path const png{old_approach_path(body)};
  if(not std::filesystem::exists(png, ec) or face_store::stamp(live_db, body))
    return;
  std::filesystem::path score_file{png};
  score_file.replace_extension(".score");
  float score{};
  if(std::ifstream in{score_file}; not in or not (in >> score))
    return;
  if(auto image{load(png)}; image and face_store::save(live_db, body, score, *image))
    spdlog::info("faces: the view of {} from the cockpit moved from {} into {}", body, png.string(), live_db.string());
  }

///\brief a binary PPM, the one format the layer reads without a library - put in place only whole
auto write_ppm(planet_face::image_t const & image, std::filesystem::path const & path) -> bool
  {
  std::filesystem::path const partial{path.string() + ".partial"};
    {
    std::ofstream out{partial, std::ios::binary | std::ios::trunc};
    if(not out)
      return false;
    out << std::format("P6\n{} {}\n255\n", image.width, image.height);
    out.write(reinterpret_cast<char const *>(image.rgb.data()), std::streamsize(image.rgb.size()));
    if(not out)
      return false;
    }
  std::error_code ec;
  std::filesystem::rename(partial, path, ec);
  return not ec;
  }

///\brief makes the face and writes it twice: the PNG the codex keeps and the PPM the layer reads
[[nodiscard]]
auto make(
  std::string body,
  sources_t sources,
  uint32_t fallback,
  uint32_t side,
  std::filesystem::path spool,
  std::filesystem::path live_db
) -> std::string
  {
  std::vector<planet_face::image_t> views;
  for(std::filesystem::path const & file: sources.views)
    if(auto image{load(file)}; image)
      views.push_back(std::move(*image));
  std::optional<planet_face::image_t> photo;
  if(sources.photo)
    photo = load(*sources.photo);
  std::optional<planet_face::image_t> approach;
  if(sources.approach)
    if(auto kept{face_store::load(live_db, body)}; kept)
      approach = std::move(kept->image);
  std::array const colour{
    float((fallback >> 16u) & 0xffu) / 255.f, float((fallback >> 8u) & 0xffu) / 255.f, float(fallback & 0xffu) / 255.f
  };
  planet_face::image_t const face{
    planet_face::make_face(views, photo ? &*photo : nullptr, approach ? &*approach : nullptr, colour, side)
  };
  if(face.empty())
    {
    spdlog::info("faces: no clear view of {} among {}", body, views.size());
    return {};
    }

  std::error_code ec;
  std::filesystem::path const kept{codex_dir() / "faces" / (file_safe(body) + ".png")};
  std::filesystem::create_directories(kept.parent_path(), ec);
  QImage const png{face.rgb.data(), int(face.width), int(face.height), int(face.width * 3u), QImage::Format_RGB888};
  if(not png.save(QString::fromStdString(kept.string())))
    spdlog::warn("faces: {} could not be written", kept.string());

  std::filesystem::create_directories(spool.parent_path(), ec);
  if(not write_ppm(face, spool))
    return {};
  spdlog::info(
    "faces: {} made from {} views{}{}",
    body,
    views.size(),
    photo ? ", its photo" : "",
    approach ? ", the cockpit's" : ""
  );
  return spool.string();
  }

///\brief the view judged; kept when at least as good as the best so far - the newer wins a tie
auto judge(std::string body, QImage view, uint32_t side, std::filesystem::path live_db) -> void
  {
  planet_face::image_t const image{to_image(view)};
  auto const judged{planet_face::judge_approach(image, 1.f, eht::settings()->exploration.approach_radius_px)};
  if(not judged or judged->score <= 0.f)
    return;
  float const best{planet_faces_t::best_score(live_db, body)};
  if(judged->score < best)
    return;
  planet_face::image_t const face{planet_face::approach_face(image, judged->disc, side)};
  if(not face_store::save(live_db, body, judged->score, face))
    {
    spdlog::error("faces: the view of {} from the cockpit could not be kept in {}", body, live_db.string());
    return;
    }
  spdlog::info(
    "faces: {} seen from the cockpit, {:.2f} ({:.0f} px, {:.0f}% in daylight) - kept, the best was {:.2f}",
    body,
    judged->score,
    judged->disc.radius,
    judged->lit * 100.f,
    best
  );
  }
  }  // namespace

auto planet_faces_t::best_score(std::filesystem::path const & live_db, std::string const & body) -> float
  {
  adopt_old_view(live_db, body);
  auto const kept{face_store::stamp(live_db, body)};
  return kept ? kept->first : -1.f;
  }

planet_faces_t::planet_faces_t(std::filesystem::path live_db) : live_db_{std::move(live_db)} {}

auto planet_faces_t::offer_view(std::string const & system, std::string const & body, QImage view) -> void
  {
  (void)system;
  if(judging_.valid() and judging_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
    return;
  // the last judgement is collected before the next replaces it - an exception in it would go unheard
  if(judging_.valid())
    try
      {
      judging_.get();
      }
    catch(std::exception const & error)
      {
      spdlog::error("faces: judging a view failed: {}", error.what());
      }
  judging_ = std::async(std::launch::async, judge, body, std::move(view), side, live_db_);
  }

auto planet_faces_t::face(std::string const & system, std::string const & body, uint32_t fallback) -> std::string
  {
  entry_t & entry{entries_[body]};
  if(entry.making.valid() and entry.making.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
    std::string made;
    try
      {
      made = entry.making.get();
      }
    catch(std::exception const & error)
      {
      spdlog::error("faces: the face of {} could not be made: {}", body, error.what());
      }
    if(not made.empty())
      entry.spool = std::move(made);
    // tried even when it failed - the same pictures would fail again
    entry.made_from = entry.making_from;
    }
  if(entry.making.valid())
    return entry.spool;

  auto const now{std::chrono::steady_clock::now()};
  if(entry.checked != std::chrono::steady_clock::time_point{} and now - entry.checked < recheck)
    return entry.spool;
  entry.checked = now;

  sources_t sources{sources_of(system, body, live_db_)};
  if((sources.views.empty() and not sources.approach) or sources.newest <= entry.made_from)
    return entry.spool;
  entry.making_from = sources.newest;
  // named after the newest picture, so a face made again is a new name to the layer, and the same after a restart
  std::filesystem::path spool{
    spool_dir() / "faces" / std::format("{}_{}.ppm", file_safe(body), sources.newest.time_since_epoch().count())
  };
  entry.making = std::async(std::launch::async, make, body, std::move(sources), fallback, side, std::move(spool), live_db_);
  return entry.spool;
  }
