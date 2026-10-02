#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

///\brief other players in the same instance, read from the game's netLog as it is written
///
/// The journal names another commander only when targeted, heard or met in a wing; the netLog writes every
/// machine the game links to, and the sessions it shares with them. The instance is the "Island" session:
/// "IJoinSession:Island: <id>: ... ThisMachine" when the game enters one, " JoinSession:Island: <id>: <runId>
/// x N [0/2]((ip:port))Name Unknown" for each other player in it, " LeftSession(n): <id>: <runId> ..." when one
/// goes. In the history checked, 96 joins in a hundred named the island the game was in at the time. The
/// name of the commander is never written - "Name Unknown" always - nor the ship
namespace instance_players
  {
using time_point_t = std::chrono::system_clock::time_point;

struct player_t
  {
  ///\brief the session the player joined
  std::string session;
  ///\brief the machine's number - the same through the whole game run
  uint64_t run_id{};
  ///\brief linked through a relay, not straight - "((Relay))"
  bool relay{};
  };

struct state_t
  {
  ///\brief the island the game is in now, empty outside one
  std::string island;
  ///\brief players joined to an island - the game's own, or one it is about to enter
  std::vector<player_t> joined;
  ///\brief the wings the players are in with this commander: session and machine
  std::vector<std::pair<std::string, uint64_t>> wing;
  ///\brief when a player last came into the game's island, and when one last went
  std::optional<time_point_t> arrived_at;
  std::optional<time_point_t> left_at;
  };

///\brief takes one line of netLog, its moment already known
auto feed(state_t & state, time_point_t at, std::string_view line) -> void;

struct summary_t
  {
  ///\brief other players in the instance now
  uint32_t players{};
  ///\brief of them, wingmates
  uint32_t wingmates{};
  ///\brief of them, linked through a relay
  uint32_t relayed{};
  };

[[nodiscard]]
auto summary(state_t const & state) -> summary_t;

struct line_t
  {
  std::string text;
  ///\brief a player not of the wing came lately - worth the eye
  bool fresh{};
  };

///\brief what to show now: the players in the instance, or the last one gone lately; none while alone
[[nodiscard]]
auto line(state_t const & state, time_point_t now) -> std::optional<line_t>;
  }  // namespace instance_players
