#include <boost/ut.hpp>
#include <event_guard.h>

#include <stdexcept>

auto main() -> int
  {
  using namespace boost::ut;

  "a function that returns is reported as done"_test = []
  {
    int calls{};
    expect(eht::event_guard("test", [&calls] { ++calls; }));
    expect(calls == 1_i);
  };

  "a standard exception is caught"_test = []
  { expect(not eht::event_guard("test", [] { throw std::runtime_error{"broken"}; })); };

  "any other exception is caught too"_test = []
  { expect(not eht::event_guard("test", [] { throw 42; })); };
  }
