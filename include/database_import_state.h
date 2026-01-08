#pragma once

#include <elite_events.h>
#include <databse_storage.h>
#include <vector>

struct database_import_state_t : public generic_state_t
  {
  struct buffered_signal_t
    {
    events::body_id_t body_id;
    std::vector<events::signal_t> signals_;
    std::vector<events::genus_t> genuses_;
    };

  struct state_t
    {
    star_system_t system{};
    std::vector<buffered_signal_t> buffered_signals;
    database_storage_t db_;

    explicit state_t(std::string_view db_path) : db_{db_path} {}
    };

  state_t * state;

  explicit database_import_state_t(std::string_view journal_dir) : generic_state_t{journal_dir} {}

  void handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) override;
  };
