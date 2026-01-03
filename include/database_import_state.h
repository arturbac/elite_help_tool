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
    };

  struct state_t
    {
    star_system_t system{};
    std::vector<buffered_signal_t> buffered_signals;
    database_storage_t db_;

    state_t(std::string db_path) : db_{db_path} {}
    };

  state_t * state;

  void handle(events::event_holder_t && event) override;
  };
