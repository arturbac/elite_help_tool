#pragma once
#include <chrono>
#include <cstdint>
#include <string>

///\brief database rows: what the personal database knows of itself and of this commander's exploring
namespace info
  {
///\brief how far the journal has already been read into this database
///\detail one row, so that starting the tool again does not walk the whole session back through
/// the database writing what is already there. Compared against journal stamps and nothing else:
/// the game's clock and this machine's need not agree, so the only safe comparison is like for like
struct journal_progress_t
  {
  ///\brief always 1; the key exists so the write has something to conflict on
  uint32_t id;
  std::chrono::sys_seconds last_event;
  };

///\brief the account this personal database belongs to
///\detail written during the import; the GUI reads it and refuses to add another commander's career, should
/// somebody log into a second account from the same game profile
struct db_owner_t
  {
  std::string fid;
  std::string name;
  };

///\brief what THIS commander did in the galaxy - this must not be shared between accounts
///\detail the world is shared, but a scan and a mapping belong to one commander. telling
/// one commander that something is mapped when another did it leads straight to a bad decision when
/// planning a flight - so these tables stay in the personal database and key themselves naturally,
/// on names and numbers from the game rather than oids, which change with every galaxy rebuild
struct system_progress_t
  {
  uint64_t system_address;
  bool fss_complete;
  };

struct body_progress_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  uint32_t body_id;
  bool mapped;
  bool footfalled;
  };

struct genus_progress_t
  {
  int64_t oid{-1};
  uint64_t system_address;
  uint32_t body_id;
  std::string genus;
  bool sampled;
  };

///\brief a light projection of star_system for pick lists; field names must match the columns
struct system_ref_t
  {
  uint64_t system_address;
  std::string name;
  };
  }  // namespace info
