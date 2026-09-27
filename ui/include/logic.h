#pragma once
#include <map>
#include <optional>
#include <ground_cz.h>
#include <functional>
#include <elite_events.h>
#include <elite_data.h>
#include <simple_enum/simple_enum.hpp>
#include <databse_storage.h>
#include <biology.h>
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

  ///\brief the organism being sampled - the game keeps one at a time and drops it the moment another is begun
  struct organic_sampling_t
    {
    uint64_t system_address{};
    events::body_id_t body{};
    std::string genus;
    std::string species;
    std::string variant;
    ///\brief 1 after the Log, 2 and 3 after each Sample; the Analyse that follows the third closes it
    uint32_t samples{};
    bool analysed{};
    ///\brief the codex had this species already - no first-logged bonus on the way
    bool was_logged{};
    ///\brief where each sample was taken, when we saw it happen - the next one must be a colony's range
    /// from all of them. Missing for samples replayed from the journal, whose places nobody wrote down
    std::vector<bio::surface_point_t> points;
    };

  organic_sampling_t sampling;

  ///\brief a scan that happened while we watched, with the moment's surroundings - for a picture of it
  struct organic_scan_seen_t
    {
    events::scan_organic_t scan;
    std::chrono::sys_seconds timestamp;
    std::string system_name;
    std::string body_name;
    std::optional<bio::surface_point_t> point;
    ///\brief which of the three samples of the species this was
    uint32_t sample{};
    };

  ///\brief the body the surface scanner last spoke of - the one its view shows while it is open
  std::string scanner_body_;

  ///\brief counts the scans seen live, so whoever is interested can tell a new one came
  uint64_t organic_scans_seen_{};
  organic_scan_seen_t last_organic_scan_;

  events::fsd_jump_t jump_info;
  events::fsd_target_t next_target;
  
  std::vector<events::event_holder_t> event_buffer_;
  std::mutex buffer_mtx_;
  
  std::vector<info::route_item_t> route_;
  uint64_t current_system_address_{};
  ///\brief the settlement we are in - collected micro resources get its market_id
  uint64_t settlement_market_id_{};

  ///\brief how far the journal had been read into this database when the tool last ran
  ///\detail everything up to here has already been written down, so on the way back to the present
  /// it is walked past rather than through - except for the handful of events that say where the
  /// ship is and what it is flying, which is what the tool would otherwise start out blind about
  std::chrono::sys_seconds resume_from_{};
  ///\brief the newest journal stamp seen, which is what gets remembered
  std::chrono::sys_seconds last_event_{};
  ///\brief true until the reader reaches the present; skipping is only ever done while it is
  ///\detail a game clock that stepped backwards would otherwise make live events look old enough
  /// to throw away, and live events are the ones that matter
  bool catching_up_{true};
  std::chrono::steady_clock::time_point progress_written_{};
  ///\brief how the way back to the present was spent, said once when it is reached
  uint64_t events_walked_past_{};
  uint64_t events_handled_{};

  ///\brief the account this database belongs to, read at startup
  ///\detail if someone logged into a second account from this game profile, their missions and finds
  /// must not land in somebody else's career - the world we still take, because the galaxy is shared
  std::string owner_fid_;
  ///\brief the commander of the session being read - a colonisation claim is theirs
  std::string commander_name_;
  ///\brief counts changes of construction sites, so the overlay reads them again at once
  uint64_t construction_changes_{};
  ///\brief counts carriers' moves, so the overlay and the window read them again at once
  uint64_t carrier_changes_{};
  ///\brief docked at one of our carriers: the hold as it was on docking - what differs on leaving is
  /// what was left on the carrier or taken off it, since the game does not tell it for a squadron's carrier
  struct carrier_visit_t
    {
    uint64_t carrier_id{};
    std::map<std::string, std::pair<std::string, int64_t>> hold;
    };
  std::optional<carrier_visit_t> carrier_visit_;
  ///\brief applies the balance of a visit to the carrier - the hold at docking against the hold now, or,
  /// leaving by escape pod, the whole hold as it was at docking
  auto close_carrier_visit(std::chrono::sys_seconds when, bool escaped, std::string_view source) -> void;
  bool personal_{true};

  current_state_t(main_window_t * p, std::string db_path, std::string journal_path) : generic_state_t{journal_path}, parent{p}, db_{db_path} {}

  void handle(std::chrono::sys_seconds timestamp, events::event_holder_t && event) override;

  ///\brief every journal line as written, with whether it is happening now or being replayed
  std::function<void(std::string_view, bool)> raw_line_listener_;
  ///\brief counts what lowers the sum a death would cost - a sale, a hand-in, the death itself - so the
  /// overlay counts it again at once rather than at its next minute
  uint64_t at_risk_changes_{};
  ///\brief counts kills in conflict zones and bond hand-ins, so the war block counts its bonds again at once
  uint64_t bond_changes_{};
  ///\brief which settlement a kill on foot is made at
  ground_cz_tracker_t ground_cz_;
  auto raw_line(std::string_view line) -> void override
    {
    if(raw_line_listener_)
      raw_line_listener_(line, not catching_up_);
    if(
      not catching_up_
      and (line.contains("\"SellOrganicData\"") or line.contains("SellExplorationData\"")
           or line.contains("\"RedeemVoucher\"") or line.contains("\"event\":\"Died\"")
           or line.contains("\"event\":\"CommitCrime\"") or line.contains("\"event\":\"PayBounties\""))
    )
      ++at_risk_changes_;
    if(not catching_up_ and (line.contains("\"FactionKillBond\"") or line.contains("\"CombatBond\"")))
      ++bond_changes_;
    }
  
  void route_system_visited(uint64_t system_address);

  ///\brief writes down how far the journal has been read, so the next start can skip it
  void remember_progress();

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
