#pragma once

#include <data/carrier.h>

#include <cstdint>
#include <filesystem>
#include <istream>
#include <map>
#include <set>
#include <string>
#include <string_view>

///\brief whose carrier a carrier is - a fleet carrier is its owner's alone, the squadron's carrier everyone's
///\detail the owner is an account (FID), never a mark shared by every account: two commanders on one machine keep
/// their carriers in one database
namespace carrier_owner
  {
///\brief whether the fleet carrier is the account's own: its owner - the commander whose session its CarrierStats
/// came in, or who marked it mine - is that account
[[nodiscard]]
inline auto own(info::carrier_t const & carrier, std::string_view account_fid) -> bool
  {
  return carrier.carrier_type != "SquadronCarrier" and not account_fid.empty() and carrier.owner_fid == account_fid;
  }

///\brief whether the carrier is ours for the account of this tool: the squadron's carrier always, a fleet carrier
/// when it is the account's own
[[nodiscard]]
inline auto ours(info::carrier_t const & carrier, std::string_view account_fid) -> bool
  { return carrier.carrier_type == "SquadronCarrier" or own(carrier, account_fid); }

///\brief whether the account may mark the carrier mine or take the mark off: a fleet carrier nobody owns yet, or its
/// own - another commander's carrier is not to be taken over by a click
[[nodiscard]]
inline auto may_mark(info::carrier_t const & carrier, std::string_view account_fid) -> bool
  {
  return carrier.carrier_type != "SquadronCarrier" and not account_fid.empty()
         and (carrier.owner_fid.empty() or carrier.owner_fid == account_fid);
  }

using owners_t = std::map<uint64_t, std::string, std::less<>>;

///\brief the owners of fleet carriers one journal tells: the account (FID) of the Commander event before each fleet
/// carrier's CarrierStats - only the owner manages a fleet carrier; the squadron's carrier is nobody's
[[nodiscard]]
auto owners_in_journal(std::istream & journal) -> owners_t;

///\brief the owners of the wanted fleet carriers as the journals of the directory tell, newest journal first, read
/// until every one is found - a directory copied with a game prefix holds another account's sessions too, so each
/// journal names its own account
///\param wanted market ids of fleet carriers whose owner is not known yet
[[nodiscard]]
auto owners_from_journals(std::filesystem::path const & journal_dir, std::set<uint64_t> wanted) -> owners_t;
  }  // namespace carrier_owner
