#pragma once

#include <events/carrier.h>
#include <events/station.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <vector>
#include <string>
#include <string_view>

///\brief the client half of ZMTP 3.0 (rfc.zeromq.org/spec/23) as far as a SUB socket on the NULL mechanism needs it -
/// enough to hear EDDN's relay, with no ZeroMQ library on the machine
namespace zmtp
  {
///\brief the whole 64-octet greeting: signature, version 3.0, NULL, not a server
[[nodiscard]]
auto greeting() -> std::array<uint8_t, 64>;

///\brief whether the peer's greeting is ZMTP 3.0 or later on the NULL mechanism - a lower version is not spoken here
[[nodiscard]]
auto greeting_ok(std::string_view peer) -> bool;

///\brief the READY command of a SUB socket
[[nodiscard]]
auto ready_sub() -> std::string;

///\brief the message subscribing to everything - the relay does not filter, so nothing narrower would help
[[nodiscard]]
auto subscribe_all() -> std::string;

struct frame_t
  {
  bool command{};
  bool more{};
  std::string body;
  };

///\brief cuts the octets after the greeting into frames, as many as have come whole
class reader_t
  {
public:
  ///\param most a frame larger than this breaks the stream - nothing EDDN sends comes near it
  explicit reader_t(uint64_t most = 16u << 20u) : most_{most} {}

  auto feed(std::string_view octets) -> void;

  ///\brief the next whole frame, if one has come
  [[nodiscard]]
  auto next() -> std::optional<frame_t>;

  ///\brief a frame with reserved flags or larger than allowed came - the connection is to be dropped
  [[nodiscard]]
  auto broken() const noexcept -> bool
    { return broken_; }

private:
  uint64_t most_;
  std::string buffer_;
  bool broken_{};
  };
  }  // namespace zmtp

///\brief what EHT takes from EDDN's relay: the bartender's shelf of a carrier, as another commander's tool sent it
namespace eddn_listen
  {
inline constexpr std::string_view fcmaterials_schema{"https://eddn.edcd.io/schemas/fcmaterials_journal/1"};

///\brief the shelf in a relayed message, when the message is one of fcmaterials_journal/1 and whole; the test
/// schema and everything else is not
[[nodiscard]]
auto fcmaterials(std::string const & json) -> std::optional<events::fcmaterials_t>;

inline constexpr std::string_view commodity_schema{"https://eddn.edcd.io/schemas/commodity/3"};
inline constexpr std::string_view journal_schema{"https://eddn.edcd.io/schemas/journal/1"};

///\brief one commodity of a relayed market, by its internal name
struct commodity_t
  {
  std::string name;
  int64_t meanPrice{};
  int64_t buyPrice{};
  int64_t stock{};
  int64_t sellPrice{};
  int64_t demand{};
  };

///\brief a whole market another commander's tool sent
struct market_t
  {
  std::string systemName;
  std::string stationName;
  uint64_t marketId{};
  std::chrono::sys_seconds timestamp{};
  std::vector<commodity_t> commodities;
  };

///\brief the market in a relayed message, when it is one of commodity/3 and whole; the test schema is not
[[nodiscard]]
auto market(std::string const & json) -> std::optional<market_t>;

struct station_faction_t
  {
  std::string Name;
  };

///\brief a relayed journal event, as far as a system's position and a station's description go
struct journal_t
  {
  std::string event;
  std::chrono::sys_seconds timestamp{};
  std::string StarSystem;
  uint64_t SystemAddress{};
  std::optional<std::array<double, 3>> StarPos;
  std::string StationName;
  std::string StationType;
  uint64_t MarketID{};
  double DistFromStarLS{};
  std::optional<events::landing_pads_t> LandingPads;
  station_faction_t StationFaction;
  };

///\brief the journal event in a relayed message, when it is one of journal/1 and names a system with its position
[[nodiscard]]
auto journal(std::string const & json) -> std::optional<journal_t>;

///\brief whether a position lies within the radius of any of the centres
[[nodiscard]]
auto in_reach(std::array<double, 3> const & position, std::span<std::array<double, 3> const> centres, double radius)
  -> bool;
  }  // namespace eddn_listen
