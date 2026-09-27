#pragma once

#include <glaze/glaze.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace eddn
  {
///\brief the name and version the messages are signed with
inline constexpr std::string_view software_name{"Elite Help Tool"};
inline constexpr std::string_view software_version{"1.0.0"};

using json_t = glz::generic_u64;

///\brief a message ready to go - its schema for the log, and the whole envelope as the gateway takes it
struct message_t
  {
  std::string schema;
  std::string envelope;
  };

///\brief decides what of the journal goes to EDDN, and shapes it the way EDMC does
///
/// Every line of the journal comes through here, the replayed past as well as the present: the past
/// only teaches the state - the game's version, the commander, the system we are in - and nothing of it
/// is ever sent. What is sent follows the settings (see eddn_settings_t): an exploration account's
/// scans of an undiscovered and unpopulated system, and a bartender account's carrier bar stock.
///
/// Whether a system was discovered is known only from the scan of its arrival star, which comes a
/// moment after the jump. Until then the system's events wait, and they go or are dropped together.
///
/// Scans do not go when they are made: they are held, on disk, until the commander sells the system's
/// cartographic data - so nothing about a system reaches the network before its discovery is the
/// commander's. A sold system with nothing held - scanned before the tool ran, or while it was closed -
/// is found again in the commander's own journals and sent the same way. The bar stock goes at once.
class publisher_t final
  {
public:
  using emit_t = std::function<void(message_t &&)>;

  ///\brief a message held until its system's data is sold
  struct held_t
    {
    std::string fid;
    std::string system;
    std::string schema;
    std::string envelope;
    };

  ///\param held_path where the held messages are kept between runs - one per line
  publisher_t(std::filesystem::path journal_dir, std::filesystem::path held_path, emit_t emit);

  ///\brief how many messages wait for a sale
  [[nodiscard]]
  auto held_count() const noexcept -> size_t
    { return held_.size(); }

  ///\brief one line of the journal; live is false while the past is being replayed
  auto feed(std::string_view line, bool live) -> void;

private:
  std::filesystem::path journal_dir_;
  std::filesystem::path held_path_;
  emit_t emit_;
  std::vector<held_t> held_;
  ///\brief set on the publisher that reads old journals for sold systems: it sends straight away, never
  /// keeps anything, takes no account of age, and heeds only the wanted systems of the one commander
  bool backfill_{};
  std::vector<std::string> wanted_;
  std::string only_fid_;

  std::string game_version_;
  std::string game_build_;
  std::optional<bool> horizons_;
  bool odyssey_{};
  std::string commander_name_;
  std::string commander_fid_;
  bool crew_{};

  std::string system_name_;
  uint64_t system_address_{};
  std::optional<json_t> star_pos_;

  enum struct verdict_e : uint8_t
    {
    unknown,
    send,
    skip
    };
  verdict_e verdict_{verdict_e::unknown};
  ///\brief the system's events waiting for the verdict, with their schemas
  std::vector<std::pair<std::string, json_t>> waiting_;
  ///\brief the messages of this system already on their way - the game writes some events two or three times
  std::set<std::string> seen_;

  ///\brief the last bar stock sent, so an unchanged one is not sent again
  uint64_t last_market_id_{};
  std::string last_items_;

  auto learn(std::string_view event, json_t const & entry) -> void;
  auto explore(std::string_view event, json_t entry, bool live) -> void;
  auto bartender(json_t const & entry, bool live) -> void;
  ///\brief the envelope around a message, as the gateway takes it
  [[nodiscard]]
  auto envelope(std::string_view schema, json_t message) const -> std::optional<message_t>;
  auto hold(std::string_view schema, json_t message) -> void;
  ///\brief sends what was held of the systems just sold
  auto release(json_t const & sale, bool live) -> void;
  auto save_held() const -> void;
  ///\brief the sold systems nothing was held of, found again in the journals and sent
  auto backfill(std::vector<std::string> const & systems) -> void;
  ///\brief StarSystem, SystemAddress and StarPos where the event lacks them - false when they cannot be known
  [[nodiscard]]
  auto augment(json_t & entry) const -> bool;
  };

///\brief removes every key ending in _Localised, at any depth - the gateway wants the game's own names
auto filter_localised(json_t & value) -> void;
  }  // namespace eddn
