// journal_context <journal file> <byte> [seconds]: where the commander was at that byte of the journal and what came
// in the seconds after it (60 when not given), as one JSON object on stdout (include/journal_context.h). Reads the
// journal only; no database.
#include <journal_context.h>

#include <spdlog/spdlog.h>

#include <charconv>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <print>
#include <string>
#include <string_view>

namespace
  {
template<typename T>
auto number(std::string_view text, T & value) -> bool
  {
  auto const [end, ec]{std::from_chars(text.data(), text.data() + text.size(), value)};
  return ec == std::errc{} and end == text.data() + text.size();
  }
  }  // namespace

auto main(int argc, char ** argv) -> int
  {
  std::uint64_t offset{};
  std::int64_t seconds{60};
  if(argc < 3 or argc > 4 or not number(argv[2], offset) or (argc == 4 and not number(argv[3], seconds)) or seconds < 0)
    {
    std::println(stderr, "usage: journal_context <journal file> <byte> [seconds]");
    return 2;
    }
  // the journal's events the tool does not read are no news here
  spdlog::set_level(spdlog::level::err);
  std::ifstream in{argv[1], std::ios::binary};
  if(not in)
    {
    std::println(stderr, "journal_context: {} cannot be read", argv[1]);
    return 1;
    }
  std::string const text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
  std::println("{}", journal_context::to_json(journal_context::read(text, offset, std::chrono::seconds{seconds})));
  return 0;
  }
