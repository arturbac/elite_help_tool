#include <boost/ut.hpp>
#include <eht_settings.h>

#include <unistd.h>

#include <filesystem>
#include <format>
#include <fstream>

auto main() -> int
  {
  using namespace boost::ut;

  // the file is put in place by a rename, so nothing of the writing is left beside it and the old
  // file's permissions go over to the new one
  "settings missing keys are written in whole, keeping the file's permissions"_test = []
  {
    std::filesystem::path const dir{std::filesystem::temp_directory_path() / std::format("eht_settings_ut_{}", ::getpid())};
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path const path{dir / "eht_settings.json"};
    {
    std::ofstream out{path, std::ios::trunc};
    out << "{}\n";
    }
    std::filesystem::permissions(path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, ec);

    expect(eht::load_settings(path));
    expect(std::filesystem::file_size(path, ec) > 100u) << "the defaults were not written in";
    expect(not std::filesystem::exists(dir / "eht_settings.json.partial")) << "the partial file was left behind";
    expect(
      std::filesystem::status(path, ec).permissions()
      == (std::filesystem::perms::owner_read | std::filesystem::perms::owner_write)
    ) << "the rewritten file lost its permissions";

    std::filesystem::remove_all(dir, ec);
  };
  }
