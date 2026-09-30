#include <planet_faces.h>
#include <codex.h>
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

struct sources_t
  {
  std::vector<std::filesystem::path> views;
  std::optional<std::filesystem::path> photo;
  std::filesystem::file_time_type newest{std::filesystem::file_time_type::min()};
  };

[[nodiscard]]
auto sources_of(std::string const & system, std::string const & body) -> sources_t
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
auto make(std::string body, sources_t sources, uint32_t fallback, uint32_t side, std::filesystem::path spool)
  -> std::string
  {
  std::vector<planet_face::image_t> views;
  for(std::filesystem::path const & file: sources.views)
    if(auto image{load(file)}; image)
      views.push_back(std::move(*image));
  std::optional<planet_face::image_t> photo;
  if(sources.photo)
    photo = load(*sources.photo);
  std::array const colour{
    float((fallback >> 16u) & 0xffu) / 255.f, float((fallback >> 8u) & 0xffu) / 255.f, float(fallback & 0xffu) / 255.f
  };
  planet_face::image_t const face{planet_face::make_face(views, photo ? &*photo : nullptr, colour, side)};
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
  spdlog::info("faces: {} made from {} views{}", body, views.size(), photo ? " and its photo" : "");
  return spool.string();
  }
  }  // namespace

auto planet_faces_t::face(std::string const & system, std::string const & body, uint32_t fallback) -> std::string
  {
  entry_t & entry{entries_[body]};
  if(entry.making.valid() and entry.making.wait_for(std::chrono::seconds{0}) == std::future_status::ready)
    {
    if(std::string made{entry.making.get()}; not made.empty())
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

  sources_t sources{sources_of(system, body)};
  if(sources.views.empty() or sources.newest <= entry.made_from)
    return entry.spool;
  entry.making_from = sources.newest;
  // named after the newest picture, so a face made again is a new name to the layer, and the same after a restart
  std::filesystem::path spool{
    spool_dir() / "faces" / std::format("{}_{}.ppm", file_safe(body), sources.newest.time_since_epoch().count())
  };
  entry.making = std::async(std::launch::async, make, body, std::move(sources), fallback, side, std::move(spool));
  return entry.spool;
  }
