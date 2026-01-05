target_sources(eht
  PRIVATE
  file_io.cc
  discover_logic.cc
  database_storage.cc
  database_import_state.cc
  elite_data.cc
  )

target_sources(journal_tailer
  PRIVATE
  elite_main.cc
  )
