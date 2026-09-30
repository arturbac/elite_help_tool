#include <boost/ut.hpp>
#include <backup.h>

#include <sqlite3.h>
#include <zstd.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace
  {
///\brief the whole archive unpacked out of zstd - the tar inside, as bytes
auto unpacked(std::filesystem::path const & archive) -> std::string
  {
  std::ifstream in{archive, std::ios::binary};
  std::string const packed{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  ZSTD_DCtx * const context{ZSTD_createDCtx()};
  std::string result;
  std::string buffer(ZSTD_DStreamOutSize(), '\0');
  ZSTD_inBuffer input{packed.data(), packed.size(), 0u};
  while(input.pos < input.size)
    {
    ZSTD_outBuffer output{buffer.data(), buffer.size(), 0u};
    if(ZSTD_isError(ZSTD_decompressStream(context, &output, &input)))
      break;
    result.append(buffer.data(), output.pos);
    }
  ZSTD_freeDCtx(context);
  return result;
  }
  }  // namespace

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  "home"_test = []
  {
    ::setenv("HOME", "/home/someone", 1);
    expect(backup::expand_home("~/.backups/eht") == std::filesystem::path{"/home/someone/.backups/eht"});
    expect(backup::expand_home("/srv/eht") == std::filesystem::path{"/srv/eht"});
  };

  "due"_test = []
  {
    std::chrono::sys_seconds const then{std::chrono::sys_days{std::chrono::year{2026} / 9 / 1}};
    backup::mark_t const last{.at = then, .pictures = 40u};
    expect(backup::due({}, then, 0u, 30u, 100u)) << "never backed up";
    expect(not backup::due(last, then + std::chrono::days{29}, 139u, 30u, 100u));
    expect(backup::due(last, then + std::chrono::days{30}, 40u, 30u, 100u)) << "a month gone";
    expect(backup::due(last, then + std::chrono::days{2}, 140u, 30u, 100u)) << "a hundred pictures more";
    expect(not backup::due(last, then + std::chrono::days{400}, 40u, 0u, 0u)) << "both turned off";
  };

  "packed and copied"_test = []
  {
    std::filesystem::path const root{std::filesystem::temp_directory_path() / "eht_backup_ut"};
    std::filesystem::path const journals{root / "journals"};
    std::filesystem::path const codex{root / "codex"};
    std::filesystem::path const destination{root / "backup"};
    std::filesystem::path const live_db{root / "live.sqlite"};
    std::filesystem::create_directories(journals);
    std::filesystem::create_directories(codex / "sky" / "Somewhere");
    {
    std::ofstream{journals / "Journal.2026-08-31T200000.01.log"} << R"({ "timestamp":"2026-08-31T20:00:00Z", "event":"Fileheader" })" "\n";
    std::ofstream{journals / "Journal.2026-09-28T213338.01.log"} << R"({ "timestamp":"2026-09-28T21:33:38Z", "event":"Fileheader" })" "\n";
    std::ofstream{journals / "Status.json"} << "{}";
    std::ofstream{codex / "sky" / "Somewhere" / "20260928-223528_Somewhere.jpg"} << "not really a picture";
    }
    {
    sqlite3 * db{};
    sqlite3_open(live_db.string().c_str(), &db);
    sqlite3_exec(db, "CREATE TABLE market(market_id INTEGER PRIMARY KEY); INSERT INTO market VALUES(42);", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    }

    auto const first{backup::run(destination, journals, codex, live_db, 3)};
    expect(first.errors.empty());
    expect(first.months.size() == 2_ul);
    expect(first.pictures_copied == 1_ul);
    expect(first.live_db_copied);
    expect(std::filesystem::exists(destination / "codex" / "sky" / "Somewhere" / "20260928-223528_Somewhere.jpg"));
    expect(std::filesystem::exists(destination / "live.sqlite"));

    std::string const tar{unpacked(destination / "journals-2026-09.tar.zst")};
    expect(fatal(tar.size() % 512u == 0u and tar.size() >= 512u * 4u));
    expect(tar.substr(0, 32) == std::string{"Journal.2026-09-28T213338.01.log"});
    expect(tar[32] == '\0');
    expect(tar.substr(257, 5) == std::string{"ustar"});
    expect(tar.substr(512, 13) == std::string{R"({ "timestamp")"});

    // the copy is a real, independent database - not just bytes lying next to the original
    {
    sqlite3 * db{};
    expect(sqlite3_open_v2((destination / "live.sqlite").string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
    sqlite3_stmt * stmt{};
    sqlite3_prepare_v2(db, "SELECT market_id FROM market", -1, &stmt, nullptr);
    expect(sqlite3_step(stmt) == SQLITE_ROW);
    expect(sqlite3_column_int64(stmt, 0) == 42_l);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    }

    // nothing changed in the journals or codex - nothing packed or copied again, live.sqlite still refreshed
    auto const second{backup::run(destination, journals, codex, live_db, 3)};
    expect(second.months.empty());
    expect(second.pictures_copied == 0_ul);
    expect(second.live_db_copied);
    expect(backup::count_pictures(codex) == 1_ul);

    backup::write_mark(destination, backup::mark_t{.at = std::chrono::sys_seconds{1'000'000s}, .pictures = 7u});
    auto const mark{backup::read_mark(destination)};
    expect(mark.at == std::chrono::sys_seconds{1'000'000s});
    expect(mark.pictures == 7_ul);

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  };
  }
