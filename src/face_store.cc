#include <face_store.h>

#include <sqlite3.h>
#include <zstd.h>

#include <memory>

namespace face_store
  {
namespace
  {
  struct close_t
    {
    auto operator()(sqlite3 * db) const -> void
      { sqlite3_close(db); }
    };

  struct finalize_t
    {
    auto operator()(sqlite3_stmt * statement) const -> void
      { sqlite3_finalize(statement); }
    };

  using db_t = std::unique_ptr<sqlite3, close_t>;
  using statement_t = std::unique_ptr<sqlite3_stmt, finalize_t>;

  ///\brief the pixels packed hard - a view is written a few times on the way to a planet, read once a start
  constexpr int level{19};

  [[nodiscard]]
  auto connect(std::filesystem::path const & live_db) -> db_t
    {
    sqlite3 * raw{};
    if(sqlite3_open_v2(live_db.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
      {
      sqlite3_close(raw);
      return {};
      }
    db_t db{raw};
    // the tool's own connection may be writing a market this moment
    sqlite3_busy_timeout(raw, 3000);
    if(
      sqlite3_exec(
        raw,
        "CREATE TABLE IF NOT EXISTS approach_view (body TEXT PRIMARY KEY, score REAL NOT NULL, width INTEGER NOT NULL, "
        "height INTEGER NOT NULL, at_ms INTEGER NOT NULL, rgb BLOB NOT NULL);",
        nullptr,
        nullptr,
        nullptr
      )
      != SQLITE_OK
    )
      return {};
    return db;
    }

  [[nodiscard]]
  auto prepare(db_t const & db, char const * sql) -> statement_t
    {
    sqlite3_stmt * raw{};
    if(sqlite3_prepare_v2(db.get(), sql, -1, &raw, nullptr) != SQLITE_OK)
      {
      sqlite3_finalize(raw);
      return {};
      }
    return statement_t{raw};
    }

  [[nodiscard]]
  auto to_time(int64_t ms) -> std::chrono::system_clock::time_point
    { return std::chrono::system_clock::time_point{std::chrono::milliseconds{ms}}; }
  }  // namespace

auto save(std::filesystem::path const & live_db, std::string const & body, float score, planet_face::image_t const & image)
  -> bool
  {
  if(image.empty())
    return false;
  std::vector<uint8_t> packed(ZSTD_compressBound(image.rgb.size()));
  size_t const size{ZSTD_compress(packed.data(), packed.size(), image.rgb.data(), image.rgb.size(), level)};
  if(ZSTD_isError(size) != 0u)
    return false;

  db_t const db{connect(live_db)};
  if(not db)
    return false;
  statement_t const insert{prepare(
    db,
    "INSERT INTO approach_view (body, score, width, height, at_ms, rgb) VALUES (?, ?, ?, ?, ?, ?) "
    "ON CONFLICT(body) DO UPDATE SET score = excluded.score, width = excluded.width, height = excluded.height, "
    "at_ms = excluded.at_ms, rgb = excluded.rgb;"
  )};
  if(not insert)
    return false;
  auto const now_ms{
    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()
  };
  sqlite3_bind_text(insert.get(), 1, body.data(), int(body.size()), SQLITE_STATIC);
  sqlite3_bind_double(insert.get(), 2, double(score));
  sqlite3_bind_int(insert.get(), 3, int(image.width));
  sqlite3_bind_int(insert.get(), 4, int(image.height));
  sqlite3_bind_int64(insert.get(), 5, int64_t(now_ms));
  sqlite3_bind_blob(insert.get(), 6, packed.data(), int(size), SQLITE_STATIC);
  return sqlite3_step(insert.get()) == SQLITE_DONE;
  }

auto load(std::filesystem::path const & live_db, std::string const & body) -> std::optional<view_t>
  {
  db_t const db{connect(live_db)};
  if(not db)
    return std::nullopt;
  statement_t const select{prepare(db, "SELECT score, width, height, at_ms, rgb FROM approach_view WHERE body = ?;")};
  if(not select)
    return std::nullopt;
  sqlite3_bind_text(select.get(), 1, body.data(), int(body.size()), SQLITE_STATIC);
  if(sqlite3_step(select.get()) != SQLITE_ROW)
    return std::nullopt;

  view_t view{
    .score = float(sqlite3_column_double(select.get(), 0)),
    .image = {.width = uint32_t(sqlite3_column_int(select.get(), 1)), .height = uint32_t(sqlite3_column_int(select.get(), 2)), .rgb = {}},
    .at = to_time(sqlite3_column_int64(select.get(), 3))
  };
  view.image.rgb.resize(size_t{view.image.width} * view.image.height * 3u);
  void const * const packed{sqlite3_column_blob(select.get(), 4)};
  auto const packed_size{size_t(sqlite3_column_bytes(select.get(), 4))};
  size_t const size{ZSTD_decompress(view.image.rgb.data(), view.image.rgb.size(), packed, packed_size)};
  // a row of another size than it says is no view at all
  if(ZSTD_isError(size) != 0u or size != view.image.rgb.size() or view.image.empty())
    return std::nullopt;
  return view;
  }

auto stamp(std::filesystem::path const & live_db, std::string const & body)
  -> std::optional<std::pair<float, std::chrono::system_clock::time_point>>
  {
  db_t const db{connect(live_db)};
  if(not db)
    return std::nullopt;
  statement_t const select{prepare(db, "SELECT score, at_ms FROM approach_view WHERE body = ?;")};
  if(not select)
    return std::nullopt;
  sqlite3_bind_text(select.get(), 1, body.data(), int(body.size()), SQLITE_STATIC);
  if(sqlite3_step(select.get()) != SQLITE_ROW)
    return std::nullopt;
  return std::pair{float(sqlite3_column_double(select.get(), 0)), to_time(sqlite3_column_int64(select.get(), 1))};
  }
  }  // namespace face_store
