#pragma once
#include <chrono>
#include <cstdint>
#include <string>

///\brief database rows: fleet carriers - where they are and go, their cargo and their bartender
namespace info
  {
///\brief one move of a carrier as the journal tells it: a jump ordered or cancelled, a position, a jump seen aboard
struct carrier_movement_t
  {
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t carrier_id;
  std::string carrier_type;
  ///\brief request, cancel, location or jump
  std::string kind;
  std::string system;
  uint64_t system_address;
  std::string body;
  ///\brief when an ordered jump leaves; empty for the other kinds
  std::chrono::sys_seconds departure;
  };

///\brief where a carrier is, and the jump it is on or waits for
struct carrier_state_t
  {
  uint64_t carrier_id;
  std::string carrier_type;
  std::string name;
  std::string callsign;
  std::string system;
  ///\brief when it was last known there
  std::chrono::sys_seconds since;
  ///\brief a jump ordered and not cancelled, whose cooldown has not run out
  bool jumping{};
  std::string from;
  std::string to;
  std::string to_body;
  std::chrono::sys_seconds departure;
  ///\brief when it arrived, or is expected to - the next jump can be ordered five minutes after it
  std::chrono::sys_seconds arrival;
  ///\brief whether the arrival was seen in the journal rather than reckoned
  bool arrived{};
  };

///\brief a commodity on a carrier - kept by hand where the game says nothing, and moved by what the
/// commander leaves on it or takes off it
struct carrier_cargo_t
  {
  int64_t oid{-1};
  uint64_t carrier_id;
  std::string key;
  std::string commodity;
  int64_t count;
  };

///\brief one change of a carrier's cargo and where it came from - a docking's balance, an escape, an edit
struct carrier_cargo_change_t
  {
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t carrier_id;
  std::string key;
  std::string commodity;
  int64_t delta;
  std::string source;
  };

///\brief a micro resource kept in a carrier's store - left there from the backpack and the ship's locker, taken
/// back the same way; the game writes none of it, so it is kept by the balance of every visit and by hand
struct carrier_micro_t
  {
  int64_t oid{-1};
  uint64_t carrier_id;
  ///\brief the internal name, the key the backpack and the locker use
  std::string name;
  int64_t count;
  };

///\brief one change of a carrier's store of micro resources and where it came from - a visit's balance, an edit
struct carrier_micro_change_t
  {
  int64_t oid{-1};
  std::chrono::sys_seconds timestamp;
  uint64_t carrier_id;
  std::string name;
  int64_t delta;
  std::string source;
  };

struct fcmaterial_t
{
  int64_t oid;
  int64_t carrier_id;
  int64_t timestamp;
  uint64_t material_id;
  uint32_t price;
  uint32_t stock;
  uint32_t demand;
};

struct carrier_t
{
  int64_t oid;
  uint64_t market_id;
  std::string carrier_name;
  std::string carrier_id;
  ///\brief the old mark of one's own carrier, one for every account sharing the database - kept in the schema, no
  /// longer read: a carrier is one's own by its owner_fid
  bool tracked;

  ///\brief the state from the last CarrierStats - empty until we have seen one
  ///
  /// The event arrives on docking and on managing the carrier, so these numbers always come
  /// from the last such moment rather than from now - hence the timestamp beside them
  std::string carrier_type;
  std::string docking_access;
  uint32_t fuel_level;
  double jump_range_curr;
  double jump_range_max;
  uint32_t total_capacity;
  uint32_t free_space;
  uint32_t cargo;
  uint64_t balance;
  uint64_t available_balance;
  std::chrono::sys_seconds stats_seen;
  ///\brief the commander whose session the carrier's CarrierStats came in, or who marked it mine - a fleet
  /// carrier's owner, the only one who loads and unloads it; empty for the squadron's and for one never managed
  std::string owner_fid;
};

///\brief a bartender shelf row after joining with the dictionary
struct carrier_stock_t
{
  std::string name;
  std::string localised;
  std::string category;
  uint32_t price;
  uint32_t stock;
  uint32_t demand;
  std::chrono::sys_seconds timestamp;
};
  }  // namespace info
