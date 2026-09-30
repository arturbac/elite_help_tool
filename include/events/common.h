#pragma once
#include <cstdint>
#include <string>

///\brief journal events: the few things every domain of the journal shares - body ids, a position, the commander of the session
namespace events
  {

///\brief who is playing this session - every journal opens with this event right after the header
///\detail the FID is the account's fixed identifier while the name can change - so the FID decides
struct commander_t
  {
  std::string FID;
  std::string Name;
  };

using body_id_t = uint32_t;

struct body_location_t
  {
  events::body_id_t body_id;
  double x, y, z;
  };

  }  // namespace events
