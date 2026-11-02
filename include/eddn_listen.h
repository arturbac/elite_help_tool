#pragma once

#include <events/carrier.h>

#include <array>
#include <cstdint>
#include <deque>
#include <optional>
#include <set>
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

///\brief what the relay says of markets and places, counted only - to learn what a search for where to buy could take
/// from it before anything is kept
struct market_probe_t
  {
  ///\brief commodity/3 messages - a whole market each
  uint64_t markets{};
  ///\brief journal/1 Docked messages, and of them those naming the station's type and its pads
  uint64_t docked{};
  uint64_t docked_type{};
  uint64_t docked_pads{};
  ///\brief journal/1 messages giving a system's position
  uint64_t star_pos{};
  ///\brief the systems the markets were in
  std::set<std::string> market_systems;

  auto note(std::string_view json) -> void;
  };
  }  // namespace eddn_listen
