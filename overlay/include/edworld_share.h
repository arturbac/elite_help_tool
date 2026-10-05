// edworld - what a d3d11 proxy in the game process publishes about the cockpit panels it sees drawn, and what
// a data source beside the game (EHT) tells it of the jump destination. Plain fixed-width layouts.
#pragma once

#include <cstddef>
#include <cstdint>

namespace edworld
  {
inline constexpr std::uint32_t share_magic{0x44575745u};  // "EWWD"
inline constexpr std::uint32_t share_version{2u};
inline constexpr std::uint32_t max_panels{64u};
inline constexpr std::uint32_t cb0_rows{12u};

///\brief one cockpit panel draw of the frame, as the game issued it
struct panel_t
  {
  ///\brief identity of the interface surface the panel samples (PS t2, or t1 for the unlit variant):
  /// the resource pointer, stable while the game keeps the surface
  std::uint64_t surface_id;
  std::uint32_t surface_width;
  std::uint32_t surface_height;
  std::uint32_t surface_format;  ///< DXGI_FORMAT
  std::uint32_t vs_index;        ///< which watched vertex shader drew it (index into the configured list)
  std::uint32_t index_count;
  std::uint32_t instance_count;
  std::uint32_t start_index;
  std::int32_t base_vertex;
  std::uint32_t start_instance;
  std::uint32_t ordinal;  ///< order among the frame's panel draws
  ///\brief VS cb0 rows 0..11 as bound at the draw; rows 4..7 produce SV_Position (one dp4 each)
  float cb0[cb0_rows][4];
  ///\brief clip position of the panel's origin: rows 4..7 applied to (position, 1) when the instance record
  /// was read (flags bit 0), else their w column alone
  float anchor_clip[4];
  ///\brief from the instance record (VS t33, through the VB0 instance entry): the panel's place relative to
  /// the world-rebase origin (record position - VS cb1[275]), its uniform scale and orientation (x, y, z, w)
  float position[3];
  float scale;
  float orientation[4];
  std::uint32_t record_index;
  std::uint32_t flags;  ///< bit 0: the record was read
  };

struct share_t
  {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t size;  ///< sizeof(share_t) of the writer
  ///\brief seqlock: odd while the writer is inside, readers retry until two equal even reads
  std::uint32_t sequence;
  std::uint64_t frame;         ///< panel frames seen since start
  std::uint64_t source_frame;  ///< the frame the panels below were drawn in (readback lags 1-3 frames)
  std::int64_t unix_ms;        ///< wall clock at publish
  std::uint32_t writer_pid;    ///< Windows process id of the game
  std::uint32_t panel_count;
  float rebase[4];           ///< VS cb1 row 275 of the frame: the world-rebase origin the positions are relative to
  std::uint32_t pool_bytes;  ///< size of the instance record pool copied for the frame
  std::uint32_t reserved;
  panel_t panels[max_panels];
  };

// ---- the destination, written by a data source beside the game (EHT), read by the proxy ----
inline constexpr std::uint32_t target_magic{0x47545745u};  // "EWTG"
inline constexpr std::uint32_t target_version{1u};

inline constexpr std::uint32_t max_target_factions{12u};

///\brief one faction of the destination as the source last saw it
struct target_faction_t
  {
  char name[64];  ///< NUL-terminated
  char states[64];           ///< active states, else recovering ones (and the source's notes); NUL-terminated
  float influence;           ///< 0..1
  std::uint8_t allegiance;   ///< as target_t::allegiance
  std::uint8_t trend;        ///< at the last tick: 0 unknown, 1 up, 2 flat, 3 down
  std::uint8_t controlling;  ///< 1: the faction controls the system
  std::uint8_t reserved;
  };

///\brief what the source knows of the system a jump is being charged to. The proxy takes it before asking
/// EDSM, when system_address is the destination Status.json names and known is set
struct target_t
  {
  std::uint32_t magic;
  std::uint32_t version;
  std::uint32_t size;      ///< sizeof(target_t) of the writer
  std::uint32_t sequence;  ///< seqlock, as in share_t
  std::uint64_t system_address;
  std::int64_t unix_ms;     ///< when written
  std::uint8_t known;       ///< 1: the source has the system; 0: it does not, ask elsewhere
  std::uint8_t allegiance;  ///< 0 unknown, 1 Federation, 2 Empire, 3 Alliance, 4 Independent, 5 other
  std::uint8_t reserved[6];
  char name[64];  ///< the system's name, for logs; NUL-terminated
  // ---- read only when size covers them: a writer of the first layout ends at name ----
  std::uint8_t factions_known;  ///< 1: the factions below are the source's whole list; 0: ask elsewhere
  std::uint8_t faction_count;
  std::uint8_t reserved2[6];
  target_faction_t factions[max_target_factions];  ///< by influence, highest first
  };

///\brief the first layout's size, without the factions: the least a reader accepts
inline constexpr std::uint32_t target_size_first{offsetof(target_t, factions_known)};
  }  // namespace edworld
