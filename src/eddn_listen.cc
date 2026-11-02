#include <eddn_listen.h>

#include <json_glaze.h>

#include <algorithm>
#include <ranges>

namespace zmtp
  {
auto greeting() -> std::array<uint8_t, 64>
  {
  std::array<uint8_t, 64> octets{};
  octets[0] = 0xffu;
  octets[9] = 0x7fu;
  octets[10] = 3u;
  octets[11] = 0u;
  constexpr std::string_view mechanism{"NULL"};
  std::ranges::copy(mechanism, octets.begin() + 12);
  // as-server 0 and the filler are the zeros already there
  return octets;
  }

auto greeting_ok(std::string_view peer) -> bool
  {
  if(peer.size() < 64u)
    return false;
  if(uint8_t(peer[0]) != 0xffu or uint8_t(peer[9]) != 0x7fu or uint8_t(peer[10]) < 3u)
    return false;
  // the mechanism is a null padded name
  std::string_view const mechanism{peer.substr(12u, 20u)};
  return mechanism.substr(0u, mechanism.find('\0')) == "NULL";
  }

auto ready_sub() -> std::string
  {
  std::string body;
  body += char(5);
  body += "READY";
  constexpr std::string_view name{"Socket-Type"};
  constexpr std::string_view value{"SUB"};
  body += char(name.size());
  body += name;
  for(int shift{24}; shift >= 0; shift -= 8)
    body += char((value.size() >> shift) & 0xffu);
  body += value;
  std::string frame;
  frame += char(0x04);
  frame += char(body.size());
  frame += body;
  return frame;
  }

auto subscribe_all() -> std::string { return std::string{"\x00\x01\x01", 3}; }

auto reader_t::feed(std::string_view octets) -> void { buffer_.append(octets); }

auto reader_t::next() -> std::optional<frame_t>
  {
  if(broken_ or buffer_.size() < 2u)
    return std::nullopt;
  uint8_t const flags{uint8_t(buffer_[0])};
  if((flags & 0xf8u) != 0u)
    {
    broken_ = true;
    return std::nullopt;
    }
  bool const is_long{(flags & 0x02u) != 0u};
  size_t const head{is_long ? 9u : 2u};
  if(buffer_.size() < head)
    return std::nullopt;
  uint64_t size{};
  if(is_long)
    for(size_t ix{1u}; ix != 9u; ++ix)
      size = (size << 8u) | uint8_t(buffer_[ix]);
  else
    size = uint8_t(buffer_[1]);
  if(size > most_)
    {
    broken_ = true;
    return std::nullopt;
    }
  if(buffer_.size() - head < size)
    return std::nullopt;
  frame_t frame{
    .command = (flags & 0x04u) != 0u, .more = (flags & 0x01u) != 0u, .body = buffer_.substr(head, size_t(size))
  };
  buffer_.erase(0u, head + size_t(size));
  return frame;
  }
  }  // namespace zmtp

namespace eddn_listen
  {
namespace
  {
  struct envelope_t
    {
    std::string schema_ref;
    events::fcmaterials_t message;
    };
  }  // namespace
  }  // namespace eddn_listen

template<>
struct glz::meta<eddn_listen::envelope_t>
  {
  using T = eddn_listen::envelope_t;
  static constexpr auto value = glz::object("$schemaRef", &T::schema_ref, "message", &T::message);
  };

namespace eddn_listen
  {
namespace detail
  {
  template<typename message_type>
  struct relayed_t
    {
    std::string schema_ref;
    message_type message;
    };
  }  // namespace detail
  }  // namespace eddn_listen

template<typename message_type>
struct glz::meta<eddn_listen::detail::relayed_t<message_type>>
  {
  using T = eddn_listen::detail::relayed_t<message_type>;
  static constexpr auto value = glz::object("$schemaRef", &T::schema_ref, "message", &T::message);
  };

namespace eddn_listen
  {
auto market(std::string const & json) -> std::optional<market_t>
  {
  if(not json.contains("/schemas/commodity/3\""))
    return std::nullopt;
  detail::relayed_t<market_t> relayed{};
  constexpr glz::opts opts{.error_on_unknown_keys = false, .error_on_missing_keys = false};
  if(auto const err{glz::read<opts>(relayed, json)}; err)
    return std::nullopt;
  if(relayed.schema_ref != commodity_schema or relayed.message.marketId == 0u or relayed.message.systemName.empty()
     or relayed.message.timestamp.time_since_epoch().count() == 0)
    return std::nullopt;
  return std::move(relayed.message);
  }

auto journal(std::string const & json) -> std::optional<journal_t>
  {
  if(not json.contains("/schemas/journal/1\"") or not json.contains("\"StarPos\""))
    return std::nullopt;
  detail::relayed_t<journal_t> relayed{};
  constexpr glz::opts opts{.error_on_unknown_keys = false, .error_on_missing_keys = false};
  if(auto const err{glz::read<opts>(relayed, json)}; err)
    return std::nullopt;
  if(relayed.schema_ref != journal_schema or relayed.message.StarSystem.empty() or not relayed.message.StarPos)
    return std::nullopt;
  return std::move(relayed.message);
  }

auto in_reach(std::array<double, 3> const & position, std::span<std::array<double, 3> const> centres, double radius)
  -> bool
  {
  return std::ranges::any_of(
    centres,
    [&](std::array<double, 3> const & c)
    {
      double const dx{position[0] - c[0]};
      double const dy{position[1] - c[1]};
      double const dz{position[2] - c[2]};
      return dx * dx + dy * dy + dz * dz <= radius * radius;
    }
  );
  }

auto fcmaterials(std::string const & json) -> std::optional<events::fcmaterials_t>
  {
  // most of the stream is other schemas - a look for the name spares parsing them
  if(json.find("fcmaterials_journal/1") == std::string::npos)
    return std::nullopt;
  envelope_t envelope{};
  constexpr glz::opts opts{.error_on_unknown_keys = false, .error_on_missing_keys = false};
  if(auto const err{glz::read<opts>(envelope, json)}; err)
    return std::nullopt;
  if(envelope.schema_ref != fcmaterials_schema)
    return std::nullopt;
  events::fcmaterials_t & shelf{envelope.message};
  if(shelf.MarketID == 0u or shelf.CarrierID.empty() or shelf.timestamp.time_since_epoch().count() == 0)
    return std::nullopt;
  return std::move(shelf);
  }
  }  // namespace eddn_listen
