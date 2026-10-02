#include <instance_players.h>

#include <algorithm>
#include <charconv>
#include <format>

namespace instance_players
  {
namespace
  {
  using namespace std::chrono_literals;

  ///\brief how long a newcomer stays told as one, and a departure at all
  constexpr auto arrival_shown{60s};
  constexpr auto departure_shown{30s};

  ///\brief "<session>: <runId> x" - what follows the kind of a session line
  struct session_line_t
    {
    std::string_view session;
    uint64_t run_id{};
    };

  [[nodiscard]]
  auto parse_session(std::string_view rest) -> std::optional<session_line_t>
    {
    // 0x00000aa39de2696f: 158670765489061 x 31 [0/2]((86.89.177.166:5100))Name Unknown
    auto const colon{rest.find(": ")};
    if(colon == std::string_view::npos or not rest.starts_with("0x"))
      return std::nullopt;
    session_line_t result{.session = rest.substr(0, colon)};
    std::string_view const run{rest.substr(colon + 2u)};
    if(
      auto const [end, ec]{std::from_chars(run.data(), run.data() + run.size(), result.run_id)};
      ec != std::errc{} or not std::string_view{end, run.data() + run.size()}.starts_with(" x ")
    )
      return std::nullopt;
    return result;
    }

  ///\brief the text after "<keyword>...: " - the kind of the session, or the reason in brackets, skipped
  [[nodiscard]]
  auto after_kind(std::string_view line, size_t keyword_end) -> std::string_view
    {
    std::string_view const rest{line.substr(keyword_end)};
    auto const colon{rest.find(": ")};
    return colon == std::string_view::npos ? std::string_view{} : rest.substr(colon + 2u);
    }

  auto forget(state_t & state, std::string_view session, uint64_t run_id) -> bool
    {
    return std::erase_if(
             state.joined,
             [&](player_t const & player)
             { return player.run_id == run_id and (session.empty() or player.session == session); }
           )
           != 0u;
    }

  [[nodiscard]]
  auto in_wing(state_t const & state, uint64_t run_id) -> bool
    {
    return std::ranges::any_of(state.wing, [run_id](auto const & member) { return member.second == run_id; });
    }

  [[nodiscard]]
  auto seconds_between(time_point_t from, time_point_t to) -> long long
    { return std::chrono::duration_cast<std::chrono::seconds>(to - from).count(); }
  }  // namespace

auto feed(state_t & state, time_point_t at, std::string_view line) -> void
  {
  if(auto const pos{line.find("} machines=")}; pos != std::string_view::npos)
    {
    // machines=2&numturnlinks=0&backlogtotal=0&backlogmax=0&avgsrtt=533&maxLoss=0.000&avgLoss=0.000&act1=34.918&act2=0.367
    auto const number = [line]<typename value_t>(std::string_view key, value_t) -> std::optional<value_t>
    {
      auto const at_key{line.find(key)};
      if(at_key == std::string_view::npos)
        return std::nullopt;
      std::string_view const rest{line.substr(at_key + key.size())};
      value_t value{};
      if(std::from_chars(rest.data(), rest.data() + rest.size(), value).ec != std::errc{})
        return std::nullopt;
      return value;
    };
    state.machines = number("machines=", uint32_t{});
    state.act1 = number("&act1=", double{});
    state.act2 = number("&act2=", double{});
    return;
    }
  bool const mine{line.contains("ThisMachine")};
  if(auto const pos{line.find("JoinSession:")}; pos != std::string_view::npos and pos != 0u)
    {
    std::string_view const kind_and_rest{line.substr(pos + 12u)};
    std::string_view const kind{kind_and_rest.substr(0, kind_and_rest.find(':'))};
    auto const parsed{parse_session(after_kind(line, pos + 12u))};
    if(not parsed)
      return;
    if(mine and line[pos - 1u] == 'I')
      {
      if(kind == "Island")
        {
        // a new instance: the players already joined to it stay, those of any other go
        state.island = std::string{parsed->session};
        std::erase_if(state.joined, [&](player_t const & player) { return player.session != state.island; });
        state.left_at.reset();
        }
      return;
      }
    if(mine or line[pos - 1u] != ' ')
      return;
    if(kind == "Island")
      {
      bool const known{forget(state, parsed->session, parsed->run_id)};
      state.joined.push_back(
        player_t{.session = std::string{parsed->session}, .run_id = parsed->run_id, .relay = line.contains("((Relay))")}
      );
      if(not known and parsed->session == state.island)
        state.arrived_at = at;
      }
    else if(kind == "WingSession" and not in_wing(state, parsed->run_id))
      state.wing.emplace_back(std::string{parsed->session}, parsed->run_id);
    return;
    }

  if(auto const pos{line.find("LeftSession(")}; pos != std::string_view::npos and pos != 0u)
    {
    auto const parsed{parse_session(after_kind(line, pos))};
    if(not parsed)
      return;
    if(mine and line[pos - 1u] == 'I')
      {
      // the game leaves: what was in that session goes with it
      std::erase_if(state.joined, [&](player_t const & player) { return player.session == parsed->session; });
      std::erase_if(state.wing, [&](auto const & member) { return member.first == parsed->session; });
      if(parsed->session == state.island)
        state.island.clear();
      return;
      }
    if(mine)
      return;
    if(forget(state, parsed->session, parsed->run_id) and parsed->session == state.island)
      state.left_at = at;
    std::erase_if(
      state.wing,
      [&](auto const & member) { return member.first == parsed->session and member.second == parsed->run_id; }
    );
    return;
    }

  if(auto const pos{line.find("} Disconnected: ")}; pos != std::string_view::npos and not mine)
    {
    // 158670765489061 x 7 [0/2]((86.89.177.166:5100))Name Unknown (Too many retries)
    std::string_view const run{line.substr(pos + 16u)};
    uint64_t run_id{};
    if(auto const [end, ec]{std::from_chars(run.data(), run.data() + run.size(), run_id)}; ec != std::errc{})
      return;
    bool const here{std::ranges::any_of(
      state.joined, [&](player_t const & player) { return player.run_id == run_id and player.session == state.island; }
    )};
    forget(state, {}, run_id);
    std::erase_if(state.wing, [run_id](auto const & member) { return member.second == run_id; });
    if(here)
      state.left_at = at;
    }
  }

auto summary(state_t const & state) -> summary_t
  {
  summary_t result;
  if(state.island.empty())
    return result;
  for(player_t const & player: state.joined)
    if(player.session == state.island)
      {
      ++result.players;
      result.wingmates += in_wing(state, player.run_id) ? 1u : 0u;
      result.relayed += player.relay ? 1u : 0u;
      }
  return result;
  }

auto line(state_t const & state, time_point_t now) -> std::optional<line_t>
  {
  summary_t const counted{summary(state)};
  if(counted.players == 0u)
    {
    if(state.left_at and now - *state.left_at < departure_shown)
      return line_t{
        .text = std::format("player left {} s ago, none in the instance now", seconds_between(*state.left_at, now))
      };
    return std::nullopt;
    }

  line_t result{.text = std::format("{} player{} in the instance", counted.players, counted.players == 1u ? "" : "s")};
  if(counted.wingmates != 0u)
    result.text += counted.wingmates == counted.players ? ", all of the wing"
                                                        : std::format(", {} of the wing", counted.wingmates);
  if(counted.relayed != 0u)
    result.text += std::format(", {} through a relay", counted.relayed);
  if(state.arrived_at and now - *state.arrived_at < arrival_shown)
    {
    result.text += std::format(" - newest {} s ago", seconds_between(*state.arrived_at, now));
    result.fresh = counted.wingmates < counted.players;
    }
  return result;
  }
  }  // namespace instance_players
