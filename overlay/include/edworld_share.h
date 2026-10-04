// edworld - what a read-only d3d11 observer in the game process publishes about the cockpit panels it
// sees drawn. Plain fixed-width layout, written on the Windows side, mapped here read-only.
#pragma once

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
  }  // namespace edworld
