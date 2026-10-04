#pragma once

#include <data/colonisation.h>
#include <data/market.h>
#include <data/navigation.h>
#include <data/station.h>
#include <data/war.h>
#include <territory.h>
#include "logic.h"
#include "overlay_exploration.h"
#include <legal.h>
#include "codex.h"
#include <surface_nav.h>
#include "scanner_sheet.h"
#include "screenshots.h"
#include "sky_album.h"
#include "planet_faces.h"
#include "approach_views.h"
#include "glare_watch.h"
#include "sensor_watch.h"
#include "netstate_watch.h"
#include <vision_shots.h>
#include "hud_reader.h"
#include <hot_drop.h>
#include <server_link.h>
#include <instance_players.h>
#include <graphics_profile.h>
#include <netstate.h>

#include <overlay_ipc.h>
#include <edworld_share.h>

#include <chrono>
#include <map>
#include <set>
#include <memory>
#include <span>
#include <vector>
#include <events/companion_files.h>

///\brief feeds the layer that draws inside the game window
///
/// the layer knows neither database nor logic - it receives finished lines. every decision about what to show is made here,
/// so changing the content requires touching nothing in the game process
class overlay_feed_t final
  {
public:
  explicit overlay_feed_t(std::string socket_path, std::string db_path);

  [[nodiscard]]
  auto listening() const noexcept -> bool;

  [[nodiscard]]
  auto clients() const noexcept -> unsigned;

  ///\brief a route plotted outside the game, together with the progress along it
  ///\detail the overlay cannot reach it on its own - it lives in the Route window, which alone knows
  /// which waypoint is already behind us
  struct plotted_route_t
    {
    std::span<info::neutron_waypoint_t const> waypoints;
    size_t reached{};
    std::string_view name;
    };

  ///\brief the construction site chosen in the Construction window while that window is the active one, 0 for none
  auto set_construction_focus(uint64_t market_id) -> void { construction_focus_ = market_id; }

  ///\brief the next step of the neutron highway as a built-in extension sees it, empty for the tool's own
  auto set_extension_hint(std::string hint) -> void { extension_hint_ = std::move(hint); }

  ///\brief the point on a body's surface chosen in the Surface window, or none
  auto set_surface_target(std::optional<nav::target_t> target) -> void { surface_target_ = std::move(target); }

  ///\brief builds the image from the state and sends it, provided anything has changed
  auto publish(current_state_t const & state, plotted_route_t const & plotted) -> void;

  ///\brief a sample was just taken and its picture not yet asked for - worth a frame before the next tick,
  /// or the countdown on the screen would start anywhere up to a tick late
  [[nodiscard]]
  auto picture_due(current_state_t const & state) const noexcept -> bool
    { return state.organic_scans_seen_ != pictured_scans_; }

private:
  ///\brief the body under the surface scanner: the one set as the destination, as the scanner needs it targeted.
  /// The journal names it only when the mapping ends - or at once for a body mapped before - so until then
  /// its last word is still of the body mapped before
  [[nodiscard]]
  auto scanner_target(current_state_t const & state) const -> std::string;

  ///\brief what the overlay shows of one system's factions
  struct system_factions_t
    {
    uint64_t system{};
    std::chrono::steady_clock::time_point loaded{};
    std::vector<overlay::line_t> lines;
    std::vector<overlay::line_t> conflicts;
    ///\brief the influence chart - empty when there is too little history
    std::vector<overlay::chart_t> charts;
    };

  ///\brief influence is not in the state, it has to come from the database - its own connection, as in the windows
  ///\param controlling the faction that controls the system, marked with a star
  auto refresh_factions(
    current_state_t const & state, uint64_t system_address, std::string_view controlling, system_factions_t & view
  ) -> void;

  ///\brief we know the market only for the station we stand in, and only until we leave
  ///\param destination the system the plotted route ends in, 0 without a route - the trades then narrow to it
  auto refresh_market(
    uint64_t market_id, uint32_t cargo_capacity, uint64_t destination, std::string_view destination_name
  ) -> void;

  ///\brief where to get the goods the open missions call for
  auto refresh_supply() -> void;

  ///\brief what the open missions can be advanced with without flying anywhere
  [[nodiscard]]
  auto build_settlement_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;
  ///\brief the system's settlements under the factions holding them - on foot at a mission board, before
  /// taking a job that sends one to a settlement, the board never says whose it is. Each one's distance
  /// from wherever we stand now is added where both bodies have been scanned
  [[nodiscard]]
  auto build_settlement_owners(star_system_t const & system) const -> std::vector<overlay::line_t>;

  ///\brief keeps neutron_route_/neutron_reached_ current with the remembered route and the system we
  /// are actually in - independent of whether the Route window happens to be open
  auto refresh_neutron_route(current_state_t const & state) -> void;
  ///\brief what to do right now, flying the Route window's neutron route or else the remembered one - empty off
  /// it, or not flying
  [[nodiscard]]
  auto build_neutron_checklist_lines(current_state_t const & state, plotted_route_t const & plotted) const
    -> std::vector<overlay::line_t>;

  ///\brief who holds the places the open missions point at - the journal does not say
  auto refresh_mission_places(current_state_t const & state) -> void;

  ///\brief the ship under the crosshairs, read off without looking away from it
  [[nodiscard]]
  auto build_target_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;

  ///\brief the hired pilot and the fighter they fly
  [[nodiscard]]
  auto build_crew_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;

  ///\brief the samples not sold yet - after a scan, and now and then for the sale at a station
  auto refresh_unsold(current_state_t const & state) -> void;
  ///\brief the wars under way in the system we are in, and the bonds not yet handed in
  auto refresh_wars(current_state_t const & state) -> void;
  ///\brief the construction sites of our systems, and the market of the port we stand at
  auto refresh_construction(current_state_t const & state) -> void;
  ///\brief what the site in view still needs: chosen in the window, docked at, or the destination in this system
  [[nodiscard]]
  auto build_construction_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;
  ///\brief one block per war: the official state of it, the bonds, and the settlements of both sides with
  /// their owner before the war and what is known of their conflict zone's intensity
  [[nodiscard]]
  auto build_war_blocks() const -> std::vector<std::vector<overlay::line_t>>;

  ///\brief the species found before, read when a scan could have added to them
  auto refresh_species_history(current_state_t const & state) -> void;

  ///\brief which interface the game has open, which only Status.json says
  auto refresh_status(current_state_t const & state) -> void;

  ///\brief the way to the surface target - an arrow turned the way to go, the distance, and how long at this pace
  [[nodiscard]]
  auto build_surface_nav_lines() const -> std::vector<overlay::line_t>;

  ///\brief the next hops of both routes - a side band holds only what comes next, not the whole list
  [[nodiscard]]
  auto build_route_lines(current_state_t const & state, plotted_route_t const & plotted) const
    -> std::vector<overlay::line_t>;

  ///\brief ships in transit and the last port - both are easy to lose track of, and both can
  /// decide where one ends up and what one flies on in
  [[nodiscard]]
  auto build_logistics_lines(std::array<double, 3> const & here) const -> std::vector<overlay::line_t>;

  ///\brief the systems of one's own factions, read while the galaxy map is open - where a trip is planned
  auto refresh_territory() -> void;
  ///\brief the territory in a side band: each faction's standing, then its systems, the nearest first
  [[nodiscard]]
  auto build_territory_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;

  ///\brief the fleet placed, read again when it changed, on a change of system, or every half minute
  auto refresh_fleet(current_state_t const & state) -> void;
  ///\brief our ships within reach of here, the nearest first - which one to go and take, or have brought
  [[nodiscard]]
  auto build_fleet_lines() const -> std::vector<overlay::line_t>;

  ///\brief joins the requirements with the hold - without it two corners of the screen have to be compared
  [[nodiscard]]
  auto build_supply_lines(
    events::cargo_file_t const & cargo, events::ship_locker_t const & locker, events::backpack_t const & backpack
  ) const -> std::vector<overlay::line_t>;

  std::unique_ptr<overlay::server_t> server_;
  database_storage_t db_;
  ///\brief the factions of the system we stand in
  system_factions_t here_;
  ///\brief and of the one the drive charges to jump to, under the game's panel of the jump
  system_factions_t jump_;
  ///\brief the system being jumped to as the database knows it - read once for each new target
  uint64_t jump_system_{};
  std::string jump_allegiance_;
  std::string jump_controlling_;

  ///\brief what the database knows of the jump's destination, told to edworld (the d3d11 proxy in the
  /// game) through `target` in the tmpfs directory of the edworld settings - once per new destination
  auto publish_edworld_target(current_state_t const & state) -> void;
  edworld::target_t * edworld_target_{};
  std::string edworld_dir_;
  uint64_t edworld_system_{};
  bool edworld_failed_{};

  uint64_t market_id_{};
  uint64_t market_destination_{};
  std::chrono::steady_clock::time_point market_loaded_{};
  std::vector<overlay::line_t> market_lines_;
  ///\brief the current market has never been opened - the side band already says so; the middle-screen
  /// reminder repeats it only for an actual ship docked there, not a taxi ride or an on-foot arrival
  bool market_unknown_{};
  ///\brief the place we are standing at, kept from the same reading the market came from
  std::string station_name_;
  std::string station_faction_;
  std::string station_type_;

  ///\brief the remembered neutron route, and how far along it we are - a live checklist for the
  /// highway's own ritual, read while flying rather than glanced at in a window that may not be open
  std::vector<info::neutron_waypoint_t> neutron_route_;
  size_t neutron_reached_{};
  uint64_t neutron_progress_system_{};
  std::chrono::steady_clock::time_point neutron_route_loaded_{};
  std::string extension_hint_;

  ///\brief the faction holding each place a mission points at, keyed by system and name
  std::map<std::pair<std::string, std::string>, std::string> place_owner_;
  ///\brief what the open missions looked like when those owners were resolved
  uint64_t missions_signature_{};

  ///\brief the interface the game has open, 0 when it is showing nothing but the cockpit
  uint32_t gui_focus_{};
  ///\brief the ship's flags from Status.json
  uint64_t status_flags_{};
  ///\brief the standing with the law in this system, from Status.json
  std::string legal_state_;
  ///\brief the commander's own flags from Status.json - on foot, in a taxi, in a hangar
  uint64_t status_flags2_{};
  ///\brief the body the game says we are at, empty away from any
  std::string status_body_;
  ///\brief where on the body we stand, and what we hold - for the samples
  overlay_exploration::surface_view_t surface_;
  ///\brief which way we face, from Status.json - absent in open space
  std::optional<double> heading_;
  nav::ground_speed_t ground_speed_;
  std::optional<nav::target_t> surface_target_;
  ///\brief every species sampled with its world, what the genera are guessed from - read again when a
  /// sample was analysed, since that is when the history grows
  std::vector<bio::species_record_t> species_history_;
  uint64_t history_scans_{~uint64_t{}};
  ///\brief the other accounts sample on their own time, so their galaxies are read again now and then
  std::chrono::steady_clock::time_point history_read_{};
  ///\brief the pictures of the samples and the page they go on
  codex_t codex_;
  ///\brief the scans already asked a picture of
  uint64_t pictured_scans_{};
  ///\brief the surface scanner's views, for finding the genera again from the ground
  scanner_sheet_t scanner_;
  sky_album_t sky_;
  planet_faces_t faces_;
  approach_views_t approach_;
  uint64_t approach_capture_id_{};
  ///\brief the hot drop: the approach to a port in space followed, the reader of the HUD's label - made once the
  /// hot drop is switched on - and the approaches written down so far, read once from hot_drop.jsonl
  hot_drop::tracker_t hot_drop_;
  std::unique_ptr<hud_reader_t> hud_reader_;
  uint64_t hot_drop_asked_ms_{};
  uint64_t hot_drop_drops_seen_{};
  bool hot_drop_loaded_{};
  bool hot_drop_unreadable_told_{};
  std::vector<hot_drop::record_t> hot_drop_records_;
  ///\brief the approach ended last, shown for a while after
  std::optional<hot_drop::record_t> hot_drop_last_;
  std::chrono::steady_clock::time_point hot_drop_last_at_{};
  ///\brief the port flown to as the hot drop needs it - none when not in supercruise towards a port in space,
  /// or in a taxi or another's ship
  [[nodiscard]]
  auto hot_drop_port(current_state_t const & state) const -> std::optional<hot_drop::context_t>;
  ///\brief follows the approach and asks for the next picture of the label - into capture_ when it is time
  auto track_hot_drop(current_state_t const & state) -> void;
  ///\brief what the approaches so far say of a hot drop here, in this ship
  [[nodiscard]]
  auto build_hot_drop_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;
  auto hot_drop_done(hot_drop::attempt_t const & attempt) -> void;

  ///\brief the whole-screen pictures for eht_vision's recorder, asked for last so any other picture goes first
  vision::shot_clock_t shot_clock_;
  ///\brief the last one asked for - still there at the next one, nobody took it, and it goes
  std::filesystem::path shot_path_;
  bool shot_untaken_told_{};
  ///\brief the system the album last saw us arrive in - 0 until the first, which is where the tool started, not a jump
  uint64_t sky_system_{};
  bool sky_started_{};
  ///\brief the jump's end, while its star is still to be asked for - the star's scan comes a moment after
  std::optional<std::chrono::steady_clock::time_point> sky_arrival_;
  ///\brief the interface open at the last frame - the scanner closing is the planet's moment
  uint32_t sky_focus_{};
  ///\brief the scanner's last moment seen - a new one is a body mapped
  std::chrono::sys_seconds sky_scanner_at_{};
  uint64_t sky_capture_id_{};
  ///\brief the screenshots the layer took at the key, filed as they come
  screenshots_t screenshots_;
  ///\brief the white glare of a settlement's rooms, written down for the evidence tool
  glare_watch_t glare_;
  uint64_t glare_capture_id_{};
  ///\brief the graphics card's and the processor's temperatures, under the layer's frame rate
  sensor_watch_t sensors_;
  ///\brief the game's network connections, written down beside the glare's markers
  netstate_watch_t netstate_;
  ///\brief where the game's netLog is, looked for now and then - the game may start after the tool
  std::filesystem::path netlog_dir_;
  std::chrono::steady_clock::time_point netlog_looked_{};
  ///\brief the link to Frontier's servers, told while it fails - netLog read as it grows, and the count of
  /// UDP datagrams received for a silence lasting now
  server_link::tail_t server_tail_;
  server_link::netlog_state_t server_state_;
  server_link::udp_silence_t udp_silence_;
  ///\brief the game's process, looked for on its own - netstate_ looks only while the evidence is gathered
  std::optional<int> server_game_;
  std::chrono::steady_clock::time_point server_game_looked_;
  ///\brief what was told last, so the log says each change once
  std::vector<std::string> server_told_;
  ///\brief the other players in the instance, from the same netLog - started again with each new file
  instance_players::state_t players_;
  std::filesystem::path players_file_;
  uint32_t players_told_{};
  ///\brief the readings the traffic log counts from - the last written, and when
  std::optional<netstate::counters_t> traffic_counters_;
  std::optional<netstate::interface_bytes_t> traffic_bytes_;
  std::chrono::steady_clock::time_point traffic_at_;
  ///\brief writes the machine's traffic down beside the players netLog names, now and then
  auto log_traffic() -> void;
  ///\brief the game's graphics file as last read, and the place it is weighed against
  graphics_profile::watch_t graphics_watch_;
  std::filesystem::path graphics_file_;
  std::filesystem::file_time_type graphics_written_;
  std::optional<graphics_profile::setting_t> graphics_setting_;
  std::chrono::steady_clock::time_point graphics_looked_;
  std::string graphics_told_;
  ///\brief the line naming the graphics set the place wants, while the game's file holds another
  [[nodiscard]]
  auto build_graphics_line(current_state_t const & state) -> std::optional<overlay::line_t>;
  ///\brief reads what is new and gives the lines to show
  [[nodiscard]]
  auto build_server_link_lines(current_state_t const & state) -> std::vector<overlay::line_t>;
  sensors::level_e gpu_level_{};
  sensors::level_e cpu_level_{};
  ///\brief the one request the frame carries - the newest of the codex's and the scanner's. Kept here so
  /// that an older one never returns to the frame, where the layer would take it for new
  overlay::capture_t capture_;
  uint64_t codex_capture_id_{};
  uint64_t scanner_capture_id_{};
  ///\brief the samples carried and not sold, read back from the journals
  bio::at_risk_t at_risk_;
  ///\brief the factions that probably hold a bounty on the commander - read back with what a death would cost
  legal_standing_t legal_;
  std::vector<territory::system_t> territory_;
  std::optional<std::chrono::sys_seconds> territory_wave_;
  std::chrono::steady_clock::time_point territory_read_{};
  std::vector<fleet::placed_ship_t> fleet_;
  uint64_t fleet_changes_{~uint64_t{}};
  uint64_t fleet_system_{~uint64_t{}};
  std::chrono::steady_clock::time_point fleet_read_{};
  std::vector<info::construction_site_t> construction_sites_;
  uint64_t construction_changes_{~uint64_t{}};
  std::chrono::steady_clock::time_point construction_read_{};
  uint64_t construction_focus_{};
  uint64_t construction_port_{};
  std::vector<info::market_entry_t> construction_port_market_;
  std::map<std::string, int64_t> carrier_cargo_;
  ///\brief each of our carriers' cargo alone, and its callsign - for the site supplied from one of them
  std::map<uint64_t, std::map<std::string, int64_t>> cargo_by_carrier_;
  std::map<uint64_t, std::string> carrier_callsigns_;
  ///\brief every commodity's type by key, for listing a site's needs by type
  std::map<std::string, std::string> commodity_categories_;
  uint64_t carrier_cargo_changes_{~uint64_t{}};
  std::vector<info::war_view_t> wars_;
  uint64_t wars_system_{};
  std::chrono::steady_clock::time_point wars_read_{};
  std::map<std::string, uint64_t> unsold_bonds_;
  uint64_t bond_changes_{~uint64_t{}};
  std::chrono::steady_clock::time_point bonds_read_{};
  uint64_t unsold_scans_{~uint64_t{}};
  std::chrono::steady_clock::time_point unsold_bounty_at_{};
  uint64_t unsold_changes_{~uint64_t{}};
  std::chrono::steady_clock::time_point unsold_read_{};
  ///\brief where the ship is set to go, as Status.json has it
  std::optional<events::status_file_t::destination_t> status_destination_;
  ///\brief the ports of the system we are in, for the picture - read again only on a change of system
  uint64_t stations_system_{};
  std::chrono::steady_clock::time_point stations_loaded_{};
  std::vector<info::station_t> stations_;
  std::chrono::steady_clock::time_point status_read_{};

  std::chrono::steady_clock::time_point supply_loaded_{};
  ///\brief raw data, because the lines also depend on the hold, which changes more often than the database
  std::vector<info::cargo_need_t> needs_;
  std::vector<info::supply_option_t> options_;
  ///\brief markets that trade the commodity, empty ones included - so emptiness is not mistaken for no source
  std::vector<info::supply_option_t> producers_;
  ///\brief every known micro resource (Data, Goods or Assets alike) by its normalised name - none of
  /// them sits in a station's commodity market, so a mission asking for one never has "no source
  /// known" to say
  std::set<std::string> micro_resource_commodities_;
  overlay::frame_t last_;
  std::chrono::steady_clock::time_point last_sent_{};
  uint64_t sequence_{};
  };
