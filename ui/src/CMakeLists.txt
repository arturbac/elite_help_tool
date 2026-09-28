
target_sources(elite_help_tool
  PRIVATE
  elite_help_tool.cc
  journal_log.cc
  system_window.cc
  ship_loadout.cc
  mission_window.cc
  current_state.cc
  route_window.cc
  faction_window.cc
  faction_state_window.cc
  micro_resource_window.cc
  bgs_window.cc
  construction_window.cc
  ships_window.cc
  surface_window.cc
  overlay_feed.cc
  overlay_exploration.cc
  codex.cc
  scanner_sheet.cc
  screenshots.cc
  sky_album.cc
  eddn_sender.cc
  # the window's icon; eht.png is eht.svg drawn at 256 px (rsvg-convert -w 256 -h 256)
  ../icons/eht.qrc
  )
