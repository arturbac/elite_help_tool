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

  "a repeated guard remembers a failure until a call succeeds"_test = []
  {
    eht::repeated_guard_t guard;
    expect(not guard.failing());
    expect(not guard("test", [] { throw std::runtime_error{"broken"}; }));
    expect(guard.failing());
    expect(not guard("test", [] { throw 42; }));
    expect(guard.failing());
    expect(guard("test", [] {}));
    expect(not guard.failing());
  };
  }
