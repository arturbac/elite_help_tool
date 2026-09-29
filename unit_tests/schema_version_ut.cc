#include <boost/ut.hpp>
#include <databse_storage.h>

#include <sqlite3.h>

#include <filesystem>

namespace
  {
namespace fs = std::filesystem;

///\brief sets a database file's own schema stamp, bypassing the app entirely - standing in for
/// whatever a newer (or a plain, unversioned) build once wrote there
auto set_user_version(std::string_view path, int version) -> void
  {
  sqlite3 * db{};
  sqlite3_open(std::string{path}.c_str(), &db);
  std::string const query{std::format("PRAGMA user_version = {}", version)};
  sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
  sqlite3_close(db);
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;

  auto const dir{fs::temp_directory_path() / "eht_schema_version_ut"};
  fs::create_directories(dir);
  auto const wd{fs::current_path()};
  fs::current_path(dir);

  auto const cleanup = [&]
  {
    fs::current_path(wd);
    std::error_code ec;
    fs::remove_all(dir, ec);
  };

  "a fresh database opens and stamps its own schema version"_test = [&]
  {
    for(char const * file: {"a.sqlite", "live.sqlite", "galaxy.sqlite"})
      {
      std::error_code ec;
      fs::remove(file, ec);
      }
    database_storage_t dbs{"a.sqlite"};
    expect(bool(dbs.open()));
    dbs.close();
  };

  "a database from an unversioned build (0) still opens"_test = [&]
  {
    for(char const * file: {"b.sqlite", "live.sqlite", "galaxy.sqlite"})
      {
      std::error_code ec;
      fs::remove(file, ec);
      }
      {
      // an empty file, never touched by any build's migration - PRAGMA user_version reads 0 on it
      sqlite3 * db{};
      sqlite3_open("b.sqlite", &db);
      sqlite3_close(db);
      }
    database_storage_t dbs{"b.sqlite"};
    expect(bool(dbs.open()));
    dbs.close();
  };

  "a database written by a future build is refused, not silently rewritten"_test = [&]
  {
    for(char const * file: {"c.sqlite", "live.sqlite", "galaxy.sqlite"})
      {
      std::error_code ec;
      fs::remove(file, ec);
      }
      {
      // a first, ordinary open - to bring live.sqlite and galaxy.sqlite into being at all, so their
      // own stamp can be pushed far into the future too
      database_storage_t dbs{"c.sqlite"};
      expect(bool(dbs.open()));
      dbs.close();
      }
    for(char const * file: {"c.sqlite", "live.sqlite", "galaxy.sqlite"})
      set_user_version(file, 9999);

    database_storage_t dbs{"c.sqlite"};
    auto const res{dbs.open()};
    expect(not res);
    if(not res)
      expect(res.error() == std::errc::not_supported) << res.error().message();
    dbs.close();
  };

  cleanup();
  }
