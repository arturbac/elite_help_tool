#pragma once
#include <elite_events.h>
#include <simple_enum/simple_enum.hpp>

class main_window_t;

struct current_state_t : public generic_state_t
  {
  main_window_t * parent;
  star_system_t system;
  ship_loadout_t ship_loadout;
  events::fsd_jump_t jump_info;
  events::fsd_target_t next_target;
  bool fss_complete;
  std::vector<events::event_holder_t> event_buffer_;
  std::mutex buffer_mtx_;

  current_state_t(main_window_t * p) : parent{p} {}

  void handle(events::event_holder_t && event) override;
  };
