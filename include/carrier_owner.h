#pragma once

#include <data/carrier.h>

#include <string_view>

///\brief whose carrier a carrier is - a fleet carrier is its owner's alone, the squadron's carrier everyone's
namespace carrier_owner
  {
///\brief whether the carrier is ours for the commander of this tool: the squadron's carrier always; a fleet carrier
/// when its owner - whose session its CarrierStats came in - is this commander; one never managed yet when it is
/// marked mine, as before the owner was kept
[[nodiscard]]
inline auto ours(info::carrier_t const & carrier, std::string_view commander_fid) -> bool
  {
  if(carrier.carrier_type == "SquadronCarrier")
    return true;
  if(not carrier.owner_fid.empty())
    return not commander_fid.empty() and carrier.owner_fid == commander_fid;
  return carrier.tracked;
  }
  }  // namespace carrier_owner
