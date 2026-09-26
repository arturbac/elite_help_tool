#pragma once

#include "logic.h"

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

  ///\brief builds the image from the state and sends it, provided anything has changed
  auto publish(current_state_t const & state, plotted_route_t const & plotted) -> void;

private:
  ///\brief influence is not in the state, it has to come from the database - its own connection, as in the windows
  auto refresh_factions(current_state_t const & state) -> void;

  ///\brief we know the market only for the station we stand in, and only until we leave
  auto refresh_market(uint64_t market_id, uint32_t cargo_capacity) -> void;

  ///\brief where to get the goods the open missions call for
  auto refresh_supply() -> void;

  ///\brief what the open missions can be advanced with without flying anywhere
  [[nodiscard]]
  auto build_settlement_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;

  ///\brief who holds the places the open missions point at - the journal does not say
  auto refresh_mission_places(current_state_t const & state) -> void;

  ///\brief the ship under the crosshairs, read off without looking away from it
  [[nodiscard]]
  auto build_target_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;

  ///\brief the hired pilot and the fighter they fly
  [[nodiscard]]
  auto build_crew_lines(current_state_t const & state) const -> std::vector<overlay::line_t>;

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
