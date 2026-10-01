#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace eddn
  {
///\brief the name and version the messages are signed with
inline constexpr std::string_view software_name{"Elite Help Tool"};
inline constexpr std::string_view software_version{"1.0.0"};

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

  ///\param held_path where the held messages are kept between runs - one per line
  publisher_t(std::filesystem::path journal_dir, std::filesystem::path held_path, emit_t emit);
  ~publisher_t();
  publisher_t(publisher_t const &) = delete;
  auto operator=(publisher_t const &) -> publisher_t & = delete;

  ///\brief how many messages wait for a sale
  [[nodiscard]]
  auto held_count() const noexcept -> size_t;

  ///\brief one line of the journal; live is false while the past is being replayed
  auto feed(std::string_view line, bool live) -> void;

private:
  ///\brief the state and the JSON handling, kept out of the header so its includers do not compile glaze
  class impl_t;
  std::unique_ptr<impl_t> impl_;
  };
  }  // namespace eddn
