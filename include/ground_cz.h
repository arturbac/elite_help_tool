#pragma once

#include <databse_storage.h>
#include <data/war.h>
#include <events/combat.h>
#include <events/navigation.h>
#include <events/station.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

///\brief where the commander is fighting on foot - which settlement, and whether on foot at all
///
/// A settlement is reached only by flying there or by dropship: an approach, a docking, a disembark at
/// it, or a Frontline Solutions booking name it. A relog in the middle of the fighting - which gives a
/// fresh conflict zone at the same place - brings only a Location with the planet, so the settlement is
/// kept for as long as the commander stays on foot in the same system. Each state owns its own tracker.
class ground_cz_tracker_t
  {
public:
  auto approach(events::approach_settlement_t const & event) -> void;
  auto docked(events::docked_t const & event) -> void;
  auto disembark(events::disembark_t const & event) -> void;
  auto book_dropship(events::book_dropship_t const & event) -> void;
  ///\brief the booked settlement is looked up by its name in the system the dropship set us down in
  auto dropship_deploy(database_storage_t & db, events::dropship_deploy_t const & event) -> void;
  auto location(events::location_t const & event) -> void;
  auto jumped(uint64_t system_address) -> void;
  auto embark() -> void;
  auto died() -> void;

  ///\brief whether the commander stands on their own legs - a bond is then a kill in a ground conflict zone
  [[nodiscard]]
  auto on_foot() const noexcept -> bool { return on_foot_; }

  ///\brief the kill recorded where it was made, when it was made on foot; a kill from a ship is no zone on foot
  [[nodiscard]]
  auto bond(std::chrono::sys_seconds when, events::faction_kill_bond_t const & event) const
    -> std::optional<info::ground_bond_t>;

private:
  uint64_t system_address_{};
  uint64_t market_id_{};
  bool on_foot_{};
  ///\brief a booking names the place by its name only - resolved when the dropship lands
  std::string booked_;
  };

///\brief the combat bonds not yet handed in, per awarding faction - read back from the journals, the
/// newest first: every kill of a faction counts until that faction's bonds are redeemed. A death does
/// not take them away
[[nodiscard]]
auto unsold_bonds(std::filesystem::path const & journal_dir, std::string_view commander_fid)
  -> std::map<std::string, uint64_t>;
