#include <boost/ut.hpp>
#include <line_fit.h>

#include <vector>

auto main() -> int
  {
  using namespace boost::ut;
  using overlay::fit_line_t;
  using overlay::lines_that_fit;

  // the heading of the list, a system, two missions, another system, a mission
  std::vector<fit_line_t> const lines{
    {.height = 10.f, .more = 0u, .with_next = false},
    {.height = 10.f, .more = 3u, .with_next = true},
    {.height = 10.f, .more = 3u, .with_next = false},
    {.height = 10.f, .more = 2u, .with_next = false},
    {.height = 10.f, .more = 1u, .with_next = true},
    {.height = 10.f, .more = 1u, .with_next = false}
  };

  "all of it when there is room, without a line saying more"_test = []
  {
    std::vector<fit_line_t> const two{{.height = 10.f, .more = 0u, .with_next = false}, {.height = 10.f, .more = 1u, .with_next = false}};
    expect(lines_that_fit(two, 20.f, 10.f) == 2_ul);
  };

  "as much as leaves room for the line saying how many more"_test = [&lines]
  { expect(lines_that_fit(lines, 50.f, 10.f) == 4_ul); };

  "a heading does not stay behind without its first row"_test = [&lines]
  {
    expect(lines_that_fit(lines, 55.f, 10.f) == 4_ul);
    expect(lines_that_fit(lines, 30.f, 10.f) == 1_ul) << "a system's heading needs its row and the last line";
  };

  "a body and the lines under it go together"_test = []
  {
    // the heading, a body with two genera, another body with one
    std::vector<fit_line_t> const life{
      {.height = 10.f, .more = 0u, .with_next = false},
      {.height = 10.f, .more = 2u, .with_next = true},
      {.height = 10.f, .more = 0u, .with_next = true},
      {.height = 10.f, .more = 0u, .with_next = false},
      {.height = 10.f, .more = 1u, .with_next = true},
      {.height = 10.f, .more = 0u, .with_next = false}
    };
    expect(lines_that_fit(life, 50.f, 10.f) == 4_ul) << "the first body whole, the second given up";
    expect(lines_that_fit(life, 35.f, 10.f) == 1_ul) << "no body cut after its first genus";
    expect(lines_that_fit(life, 60.f, 10.f) == 6_ul);
  };

  "the list's own heading stands however little room there is"_test = [&lines]
  { expect(lines_that_fit(lines, 1.f, 10.f) == 1_ul); };
  }
