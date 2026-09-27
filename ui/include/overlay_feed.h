#pragma once

#include "logic.h"
#include "overlay_exploration.h"
#include <legal.h>
#include "codex.h"
#include "scanner_sheet.h"
#include "screenshots.h"

#include <overlay_ipc.h>

#include <chrono>
#include <map>
#include <memory>
#include <span>
#include <vector>

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

  ///\brief builds the image from the state and sends it, provided anything has changed
  auto publish(current_state_t const & state, plotted_route_t const & plotted) -> void;

  ///\brief a sample was just taken and its picture not yet asked for - worth a frame before the next tick,
  /// or the countdown on the screen would start anywhere up to a tick late
  [[nodiscard]]
  auto picture_due(current_state_t const & state) const noexcept -> bool
    { return state.organic_scans_seen_ != pictured_scans_; }

private:
  ///\brief influence is not in the state, it has to come from the database - its own connection, as in the windows
  auto refresh_factions(current_state_t const & state) -> void;

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
  /// taking a job that sends one to a settlement, the board never says whose it is
  [[nodiscard]]
  auto build_settlement_owners() const -> std::vector<overlay::line_t>;

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

  ///\brief the next hops of both routes - a side band holds only what comes next, not the whole list
  [[nodiscard]]
  auto build_route_lines(current_state_t const & state, plotted_route_t const & plotted) const
    -> std::vector<overlay::line_t>;

  ///\brief ships in transit and the last port - both are easy to lose track of, and both can
  /// decide where one ends up and what one flies on in
  [[nodiscard]]
  auto build_logistics_lines() const -> std::vector<overlay::line_t>;

  ///\brief joins the requirements with the hold - without it two corners of the screen have to be compared
  [[nodiscard]]
  auto build_supply_lines(events::cargo_file_t const & cargo) const -> std::vector<overlay::line_t>;

  std::unique_ptr<overlay::server_t> server_;
  database_storage_t db_;
  uint64_t factions_system_{};
  std::chrono::steady_clock::time_point factions_loaded_{};
  std::vector<overlay::line_t> faction_lines_;
  std::vector<overlay::line_t> conflict_lines_;
  ///\brief the influence chart of the system we stand in - empty when there is too little history
  std::vector<overlay::chart_t> faction_charts_;

  uint64_t market_id_{};
  uint64_t market_destination_{};
  std::chrono::steady_clock::time_point market_loaded_{};
  std::vector<overlay::line_t> market_lines_;
  ///\brief the place we are standing at, kept from the same reading the market came from
  std::string station_name_;
  std::string station_faction_;
  std::string station_type_;

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
  ///\brief the screenshots the layer took at the key, filed as they come
  screenshots_t screenshots_;
  ///\brief the one request the frame carries - the newest of the codex's and the scanner's. Kept here so
  /// that an older one never returns to the frame, where the layer would take it for new
  overlay::capture_t capture_;
  uint64_t codex_capture_id_{};
  uint64_t scanner_capture_id_{};
  ///\brief the samples carried and not sold, read back from the journals
  bio::at_risk_t at_risk_;
  ///\brief the factions that probably hold a bounty on the commander - read back with what a death would cost
  legal_standing_t legal_;
  std::vector<info::construction_site_t> construction_sites_;
  uint64_t construction_changes_{~uint64_t{}};
  std::chrono::steady_clock::time_point construction_read_{};
  uint64_t construction_focus_{};
  uint64_t construction_port_{};
  std::vector<info::market_entry_t> construction_port_market_;
  std::map<std::string, int64_t> carrier_cargo_;
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
  overlay::frame_t last_;
  std::chrono::steady_clock::time_point last_sent_{};
  uint64_t sequence_{};
  };
