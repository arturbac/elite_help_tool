#pragma once

#include "logic.h"

#include <overlay_ipc.h>

#include <chrono>
#include <memory>
#include <span>
#include <vector>

///\brief zasila warstwe rysujaca w oknie gry
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

  ///\brief skad wziac towar wymagany przez otwarte misje
  auto refresh_supply() -> void;

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

  uint64_t market_id_{};
  std::chrono::steady_clock::time_point market_loaded_{};
  std::vector<overlay::line_t> market_lines_;

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
