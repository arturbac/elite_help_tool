#include <boost/ut.hpp>
#include <file_io.h>

#include <unistd.h>

#include <chrono>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;

  // the game writes a line in pieces now and then; read half, it would be lost as one that cannot be parsed
  "a line still being written is read whole once its newline comes"_test = []
  {
    std::filesystem::path const path{std::filesystem::temp_directory_path() / std::format("eht_file_io_ut_{}.log", ::getpid())};
    {
    std::ofstream out{path, std::ios::trunc};
    out << "first\n{\"event\":\"Sec" << std::flush;
    }
    std::mutex mutex;
    std::vector<std::string> lines;
    std::jthread reader{[&](std::stop_token stop)
                        {
                          tail_file(
                            path,
                            [&](std::string_view line)
                            {
                              std::scoped_lock const lock{mutex};
                              lines.emplace_back(line);
                            },
                            stop
                          );
                        }};
    std::this_thread::sleep_for(300ms);
    {
    std::ofstream out{path, std::ios::app};
    out << "ond\"}\nthird\n" << std::flush;
    }
    auto const deadline{std::chrono::steady_clock::now() + 3s};
    while(std::chrono::steady_clock::now() < deadline)
      {
      {
      std::scoped_lock const lock{mutex};
      if(lines.size() >= 3u)
        break;
      }
      std::this_thread::sleep_for(20ms);
      }
    reader.request_stop();
    reader.join();
    std::scoped_lock const lock{mutex};
    expect(lines.size() == 3_u);
    if(lines.size() == 3u)
      {
      expect(lines[0] == std::string{"first"});
      expect(lines[1] == std::string{R"({"event":"Second"})"}) << lines[1];
      expect(lines[2] == std::string{"third"});
      }
    std::error_code ec;
    std::filesystem::remove(path, ec);
  };

  "a directory not there yet lists no journals and no error"_test = []
  {
    std::error_code ec;
    auto const found{find_all_journals("/nonexistent/eht/journals", ec)};
    expect(found.empty() and not ec);
  };
  }
