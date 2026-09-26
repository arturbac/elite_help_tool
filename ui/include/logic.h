#pragma once
#include <elite_events.h>
#include <elite_data.h>
#include <simple_enum/simple_enum.hpp>
#include <databse_storage.h>
#include <mutex>

class main_window_t;

///\brief the sentence about a recalculation, ready to show - the same in the system window and in the overlay
struct tick_view_t
  {
  ///\brief when this system last recalculated, and how long ago
  std::string here;
  ///\brief the span the last wave took across the galaxy, and how regularly it comes
  std::string galaxy;
  ///\brief the wave has started but we have not yet seen this system in it - for influence that
  /// means "there is still time to hand missions in", for wars "bonds not recalculated yet"
  bool awaiting;
  };

///\brief builds the description of a recalculation from what was observed - it never adds a forecast
[[nodiscard]]
auto describe_tick(
  database_storage_t & db, uint64_t system_address, info::tick_kind_e kind, std::chrono::sys_seconds now
) -> tick_view_t;

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

  ///\brief what is in the hold right now - a passing state, Cargo.json gets overwritten
  events::cargo_file_t cargo;

  ///\brief the ship under the crosshairs right now
  ///\detail live only, and deliberately so: a target is gone the moment it is let go, and writing
  /// down seventy thousand of them a year would say nothing a screen does not say better
  events::ship_targeted_t target;
  ///\brief the last kill that paid, kept only long enough to be read off the screen
  events::bounty_t last_bounty;
  ///\brief when that kill reached us, by a clock that is ours
  ///\detail the journal is stamped by the game's clock, which need not agree with this machine's
  /// even when both call themselves UTC - so how long ago something happened is measured by the
  /// only clock both ends of the question share, the one counting since this process started
  std::chrono::steady_clock::time_point last_bounty_at{};

  ///\brief where the hired fighter is
  enum struct fighter_e : uint8_t
    {
    stowed,
    deployed,
    destroyed
    };
  fighter_e fighter{fighter_e::stowed};
  ///\brief false while it is the commander flying it
  bool fighter_crewed{};
  ///\brief the hired pilot on duty, and how good they have become
  std::string crew_name;
  uint32_t crew_combat_rank{};

  events::fsd_jump_t jump_info;
  events::fsd_target_t next_target;
  
  std::vector<events::event_holder_t> event_buffer_;
  std::mutex buffer_mtx_;
  
  std::vector<info::route_item_t> route_;
  uint64_t current_system_address_{};
  ///\brief the settlement we are in - collected micro resources get its market_id
  uint64_t settlement_market_id_{};

  ///\brief the account this database belongs to, read at startup
  ///\detail if someone logged into a second account from this game profile, their missions and finds
  /// must not land in somebody else's career - the world we still take, because the galaxy is shared
  std::string owner_fid_;
  bool personal_{true};

  current_state_t(main_window_t * p, std::string db_path, std::string journal_path) : generic_state_t{journal_path}, parent{p}, db_{db_path} {}

  void handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) override;
  
  void route_system_visited(uint64_t system_address);

  ///\brief forgets what only held true inside one session of the game
  ///\detail a target lock does not survive the game being closed, and nothing in the journal says
  /// so - the last thing written is that something was locked, and it stays locked for ever unless
  /// the end of the session is taken as the end of it. The hired crew is not forgotten: they are
  /// still aboard tomorrow
  void forget_live_combat();

  // called from the worker thread; db_ is never touched from the GUI thread
  void load_factions();

private:
  void load_missions();
  };
