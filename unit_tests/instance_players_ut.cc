#include <boost/ut.hpp>
#include <instance_players.h>

#include <chrono>

auto main() -> int
  {
  using namespace boost::ut;
  using namespace std::chrono_literals;
  using instance_players::time_point_t;

  time_point_t const t0{std::chrono::sys_days{std::chrono::year{2026} / 7 / 11} + 22h + 21min};

  "alone, nothing is shown"_test = [t0]
  {
    instance_players::state_t state;
    feed(
      state,
      t0,
      "{22:20:00GMT 1345.000s} IJoinSession:Island: 0x00000aa39de2696f: 37817412504777 x 21 ThisMachine Name Unknown"
    );
    expect(not line(state, t0 + 1s));
  };

  "a wing comes into the instance, one through a relay, then goes"_test = [t0]
  {
    // what the game wrote on 11 Jul 2026, shortened to two of the three wingmates
    instance_players::state_t state;
    feed(
      state,
      t0,
      "{22:21:17GMT 1422.834s}  JoinSession:WingSession: 0x00000aa39ddde9d9: 158670765489061 x 30 "
      "[0/2]((86.89.177.166:5100))Name Unknown"
    );
    feed(
      state,
      t0,
      "{22:21:17GMT 1422.860s}  JoinSession:MyTalk: 0x000000024c809703: 158670765489061 x 25 "
      "[0/2]((86.89.177.166:5100))Name Unknown"
    );
    feed(
      state,
      t0 + 60s,
      "{22:22:17GMT 1482.900s} IJoinSession:Island: 0x00000aa39de2696f: 37817412504777 x 21 ThisMachine Name Unknown"
    );
    feed(
      state,
      t0 + 60s,
      "{22:22:17GMT 1482.980s}  JoinSession:Island: 0x00000aa39de2696f: 158670765489061 x 31 "
      "[0/2]((86.89.177.166:5100))Name Unknown"
    );
    feed(
      state,
      t0 + 60s,
      "{22:22:17GMT 1482.980s}  JoinSession:Wake: 0x00000aa39de26970: 158670765489061 x 31 "
      "[0/2]((86.89.177.166:5100))Name Unknown"
    );
    feed(
      state,
      t0 + 62s,
      "{22:22:19GMT 1484.000s}  JoinSession:Island: 0x00000aa39de2696f: 248771890809383 x 3 [1/2]((Relay))Name Unknown"
    );

    auto const counted{summary(state)};
    expect(counted.players == 2u) << counted.players;
    expect(counted.wingmates == 1u);
    expect(counted.relayed == 1u);
    auto const shown{line(state, t0 + 70s)};
    expect(fatal(shown.has_value()));
    expect(shown->text == "2 players in the instance, 1 of the wing, 1 through a relay - newest 8 s ago")
      << shown->text;
    expect(shown->fresh);
    expect(line(state, t0 + 200s)->text == "2 players in the instance, 1 of the wing, 1 through a relay");

    feed(
      state,
      t0 + 210s,
      "{22:24:47GMT 1632.000s}  LeftSession(3): 0x00000aa39de2696f: 248771890809383 x 92 [1/2]((Relay))Name Unknown"
    );
    feed(
      state,
      t0 + 215s,
      "{22:24:52GMT 1637.000s} Disconnected: 158670765489061 x 7 [0/2]((86.89.177.166:5100))Name Unknown (Too many "
      "retries)"
    );
    expect(summary(state).players == 0u);
    expect(line(state, t0 + 220s)->text == "player left 5 s ago, none in the instance now")
      << line(state, t0 + 220s)->text;
    expect(not line(state, t0 + 250s));
  };

  "the game leaving its instance takes the players with it"_test = [t0]
  {
    instance_players::state_t state;
    feed(
      state,
      t0,
      "{22:22:17GMT 1482.900s} IJoinSession:Island: 0x00000aa39de2696f: 37817412504777 x 21 ThisMachine Name Unknown"
    );
    feed(
      state,
      t0,
      "{22:22:17GMT 1482.980s}  JoinSession:Island: 0x00000aa39de2696f: 158670765489061 x 31 "
      "[0/2]((86.89.177.166:5100))Name Unknown"
    );
    expect(line(state, t0 + 1s)->text == "1 player in the instance - newest 1 s ago");
    feed(
      state,
      t0 + 5s,
      "{22:22:22GMT 1487.000s} ILeftSession(3): 0x00000aa39de2696f: 37817412504777 x 30 ThisMachine Name Unknown"
    );
    expect(summary(state).players == 0u);
    expect(not line(state, t0 + 6s)) << "it was the game that went, not the player";
  };

  "a player joined before the game enters the same instance is counted there"_test = [t0]
  {
    instance_players::state_t state;
    feed(
      state,
      t0,
      "{22:22:17GMT 1482.980s}  JoinSession:Island: 0x00000aa39de26aad: 158670765489061 x 31 "
      "[0/2]((86.89.177.166:5100))Name Unknown"
    );
    feed(
      state,
      t0,
      "{22:22:17GMT 1482.980s}  JoinSession:Island: 0x00000aa39de26bbb: 248771890809383 x 3 [0/2]((1.2.3.4:5100))Name "
      "Unknown"
    );
    feed(
      state,
      t0 + 1s,
      "{22:22:18GMT 1483.000s} IJoinSession:Island: 0x00000aa39de26aad: 37817412504777 x 21 ThisMachine Name Unknown"
    );
    expect(summary(state).players == 1u);
    expect(state.joined.size() == 1u) << "the other island's player forgotten";
  };
  
  "the game's own ten-minute figures are kept"_test = [t0]
  {
    instance_players::state_t state;
    feed(
      state,
      t0,
      "{22:27:44GMT 1809.613s} machines=5&numturnlinks=0&backlogtotal=0&backlogmax=0&avgsrtt=295&maxLoss=0.000&"
      "avgLoss=0.000&act1=49.892&act2=16.060"
    );
    expect(state.machines == std::optional<uint32_t>{5u});
    expect(state.act1 and *state.act1 > 49.89 and *state.act1 < 49.90);
    expect(state.act2 and *state.act2 > 16.05 and *state.act2 < 16.07);
  };
}
