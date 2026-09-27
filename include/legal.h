#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

///\brief a faction that probably still holds a bounty on the commander, and when the last crime against it was
struct bounty_holder_t
  {
  std::string faction;
  std::chrono::sys_seconds last_crime;
  ///\brief the bounties of the crimes counted - before anything the game made of them
  uint64_t crimes_bounty{};
  };

///\brief what the journals tell of the commander's standing
struct legal_standing_t
  {
  std::vector<bounty_holder_t> holders;
  ///\brief the notoriety the game wrote in its statistics at the last login, and when
  uint32_t notoriety{};
  std::chrono::sys_seconds notoriety_at{};
  };

///\brief read back from the journals, the newest first: the factions the commander committed crimes with
/// a bounty against since their bounties were last paid - summed over all that time, since a bounty grows
/// over weeks of small crimes - the last of them within the window from since. The squadron's
/// Notoriety Decay clears bounties unseen - 100k every two hours while notoriety lasts - so a faction whose
/// crimes add up to less than one such step is gone two hours after the last of them. Not the sums: the
/// game counts bounties in amounts the journal does not write. A fuller model of the decay, notoriety step
/// by step, cleared the 371k bounty the game still showed - the journal cannot tell how the game counts.
/// The latest crime first
[[nodiscard]]
auto legal_standing(
  std::filesystem::path const & journal_dir,
  std::string_view commander_fid,
  std::chrono::sys_seconds since,
  std::chrono::sys_seconds now
) -> legal_standing_t;
