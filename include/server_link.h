#pragma once

#include <evidence_log.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

///\brief trouble with Frontier's servers, told while it happens - not after the game's own dialog
///
/// The game says nothing in the journal while its link to the servers fails, and its disconnect dialog comes
/// only once it has given up: in the sessions looked at, the game server had already been silent for 38 to
/// 55 seconds by then. Two things show the trouble earlier, both read without root:
/// - the game's own netLog, its lines read as they are written: "Several LOST packet#" when a packet comes
///   after a gap, "Disconnected: ... (Too many retries)" when one of its servers (the missions' one first, as
///   a rule) is given up - two times in three a disconnect followed within two minutes; the same words of
///   a link to another player - through a relay or straight, in a wing or a fight - are no word of
///   Frontier's servers and are told apart as the player's link; the disconnect itself with its reason, and the web API beside the game server -
///   "Webserver request failed: code N" (0 is no answer at all), and "HTTP Request took N sec", which the
///   game writes only for requests of 10 seconds or more;
/// - the system's count of UDP datagrams received: the game server sends several a second all the time a
///   session runs, so none for a few seconds is the silence itself, seen as it lasts. The count is the
///   whole machine's - another program's traffic can hide a silence, never invent one
namespace server_link
  {
using time_point_t = std::chrono::system_clock::time_point;

///\brief what netLog said lately, kept only as long as it is worth showing
struct netlog_state_t
  {
  ///\brief a packet came from the game server after a gap - some were lost on the way
  std::optional<time_point_t> lost_at;
  std::string lost_server;
  ///\brief one of the game's servers given up after too many retries, the session itself still on
  std::optional<time_point_t> dropped_at;
  std::string dropped_server;
  ///\brief the link to another player given up - through a relay or straight - with the reason; their
  /// leaving the instance in good order ("shutdown") is no trouble and is not kept
  std::optional<time_point_t> player_dropped_at;
  std::string player_dropped_how;
  ///\brief the game's own disconnect, with the reason it names
  std::optional<time_point_t> disconnected_at;
  std::string disconnect_reason;
  ///\brief a server reached again - ends whatever was told of the link before it
  std::optional<time_point_t> connected_at;
  ///\brief the web API's failed requests, oldest first: the moment, and the code ("no answer" for 0)
  std::deque<std::pair<time_point_t, std::string>> api_failures;
  ///\brief the last request of 10 s or more: when it ended, how long it took and what it asked for
  std::optional<time_point_t> slow_at;
  double slow_s{};
  std::string slow_what;
  };

///\brief takes one line of netLog, its moment already known
auto feed(netlog_state_t & state, time_point_t at, std::string_view line) -> void;

///\brief follows the count of UDP datagrams received, to tell a silence while it lasts
struct udp_silence_t
  {
  std::optional<uint64_t> last_count;
  ///\brief when the count last grew
  std::optional<time_point_t> last_growth;
  ///\brief how many growths were seen in a row lately - a silence counts only after traffic, so a game
  /// at its main menu, which hears nothing, is no alarm
  uint32_t flowing{};
  };

///\brief takes the count of datagrams received, read now
auto feed(udp_silence_t & silence, time_point_t now, uint64_t in_datagrams) -> void;

///\brief how long nothing has come, when it is long enough to be a silence and came after traffic
[[nodiscard]]
auto silent_for(udp_silence_t const & silence, time_point_t now, std::chrono::milliseconds threshold)
  -> std::optional<std::chrono::milliseconds>;

enum struct level_e : uint8_t
  {
  ///\brief the game goes on, worse: the web API failing or slow, packets lost on the way - in the history
  /// checked, a third of the losses only were followed by a disconnect
  degraded,
  ///\brief the session itself at stake: the game server silent, given up, or the disconnect come
  failing
  };

struct warning_t
  {
  level_e level{};
  std::string text;
  };

///\brief what to show now, the worst first; empty while all is well
///\param silence the UDP silence lasting now, from silent_for; none while the game is not in a session
[[nodiscard]]
auto warnings(netlog_state_t const & state, std::optional<std::chrono::milliseconds> silence, time_point_t now)
  -> std::vector<warning_t>;

///\brief reads the newest netLog of a directory as it grows: new lines only, a new file from its beginning
class tail_t final
  {
public:
  ///\brief calls on_line for each complete line written since the last call, with its moment
  template<typename on_line_t>
  auto read(std::filesystem::path const & dir, on_line_t on_line) -> void
    {
    for(std::string_view const line: read_lines(dir))
      if(auto const at{moment(line)}; at)
        on_line(*at, line);
    }

  ///\brief the file followed now, empty before the first
  [[nodiscard]]
  auto file() const noexcept -> std::filesystem::path const & { return file_; }

private:
  [[nodiscard]]
  auto read_lines(std::filesystem::path const & dir) -> std::vector<std::string_view>;
  [[nodiscard]]
  auto moment(std::string_view line) -> std::optional<time_point_t>;

  std::filesystem::path file_;
  uint64_t offset_{};
  std::string buffer_;
  std::string pending_;
  std::chrono::steady_clock::time_point listed_;
  std::optional<evidence::netlog_clock_t> clock_;
  };
  }  // namespace server_link
