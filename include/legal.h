#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

///\brief a faction that probably holds a bounty on the commander, and when the last crime against it was
struct bounty_holder_t
  {
  std::string faction;
  std::chrono::sys_seconds last_crime;
  };

///\brief the factions the commander committed a crime with a bounty against since that faction's bounties
/// were last paid, within the last days given - read back from the journals, the newest first. Not the
/// sums: the game writes them to no file, and a squadron's Notoriety Decay lowers them unseen; the window
/// leaves out what it has most likely cleared. The latest crime first
[[nodiscard]]
auto bounty_holders(
  std::filesystem::path const & journal_dir, std::string_view commander_fid, std::chrono::sys_seconds since
) -> std::vector<bounty_holder_t>;
