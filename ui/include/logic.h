#pragma once
#include <elite_events.h>
#include <elite_data.h>
#include <simple_enum/simple_enum.hpp>
#include <databse_storage.h>
#include <mutex>

class main_window_t;

struct current_state_t : public generic_state_t
  {
  struct buffered_signal_t
    {
    events::body_id_t body_id;
    std::vector<events::signal_t> signals_;
    std::vector<events::genus_t> genuses_;
    };
  main_window_t * parent;
  star_system_t system;
  std::vector<info::faction_info_t> system_factions;
  std::vector<info::faction_info_t> known_factions;
  std::vector<info::mission_t> active_missions;
  
  ship_loadout_t ship_loadout;
  database_storage_t db_;
  std::vector<buffered_signal_t> buffered_signals;

  ///\brief co jest w ladowni teraz - stan przejsciowy, Cargo.json jest nadpisywany
  events::cargo_file_t cargo;

  events::fsd_jump_t jump_info;
  events::fsd_target_t next_target;
  
  std::vector<events::event_holder_t> event_buffer_;
  std::mutex buffer_mtx_;
  
  std::vector<info::route_item_t> route_;
  uint64_t current_system_address_{};
  ///\brief osada w ktorej jestesmy - zdobyte mikrozasoby dostaja jej market_id
  uint64_t settlement_market_id_{};

  ///\brief konto do ktorego nalezy ta baza, odczytane przy starcie
  ///\detail gdyby ktos zalogowal sie z tego profilu gry na drugie konto, jego misje i zdobycze
  /// nie moga trafic do cudzej kariery - swiat bierzemy dalej, bo galaktyka jest wspolna
  std::string owner_fid_;
  bool personal_{true};

  current_state_t(main_window_t * p, std::string db_path, std::string journal_path) : generic_state_t{journal_path}, parent{p}, db_{db_path} {}

  void handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) override;
  
  void route_system_visited(uint64_t system_address);

  // wołane z wątku roboczego, db_ nie jest dotykane z wątku GUI
  void load_factions();

private:
  void load_missions();
  };
