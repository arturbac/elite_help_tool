#pragma once

#include "logic.h"

#include <overlay_ipc.h>

#include <chrono>
#include <memory>
#include <span>
#include <vector>

///\brief zasila warstwe rysujaca w oknie gry
///
/// warstwa nie zna bazy ani logiki - dostaje gotowe linie. cala decyzja co pokazac zapada tutaj,
/// dzieki czemu zmiana tresci nie wymaga ruszania niczego w procesie gry
class overlay_feed_t final
  {
public:
  explicit overlay_feed_t(std::string socket_path, std::string db_path);

  [[nodiscard]]
  auto listening() const noexcept -> bool;

  [[nodiscard]]
  auto clients() const noexcept -> unsigned;

  ///\brief trasa wyznaczona poza gra razem z postepem
  ///\detail overlay nie siegnie po nia sam - zyje w oknie Route, ktore jedyne wie, ktory
  /// przystanek mamy juz za soba
  struct plotted_route_t
    {
    std::span<info::neutron_waypoint_t const> waypoints;
    size_t reached{};
    std::string_view name;
    };

  ///\brief buduje obraz ze stanu i wysyla go, o ile cokolwiek sie zmienilo
  auto publish(current_state_t const & state, plotted_route_t const & plotted) -> void;

private:
  ///\brief influence nie siedzi w stanie, trzeba po nie do bazy - wlasne polaczenie jak w oknach
  auto refresh_factions(current_state_t const & state) -> void;

  ///\brief rynek znamy tylko dla stacji w ktorej stoimy i tylko dopoki nie odlecimy
  auto refresh_market(uint64_t market_id, uint32_t cargo_capacity) -> void;

  ///\brief skad wziac towar wymagany przez otwarte misje
  auto refresh_supply() -> void;

  ///\brief najblizsze skoki obu tras - w pasie bocznym mieszcza sie tylko nastepne, nie cala lista
  [[nodiscard]]
  auto build_route_lines(current_state_t const & state, plotted_route_t const & plotted) const
    -> std::vector<overlay::line_t>;

  ///\brief laczy wymagania z zawartoscia ladowni - bez tego trzeba porownywac dwa rogi ekranu
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
  ///\brief surowe dane, bo linie zaleza tez od ladowni, ktora zmienia sie czesciej niz baza
  std::vector<info::cargo_need_t> needs_;
  std::vector<info::supply_option_t> options_;
  ///\brief rynki handlujace danym towarem, nawet puste - zeby nie mylic pustki z brakiem zrodla
  std::vector<info::supply_option_t> producers_;
  overlay::frame_t last_;
  std::chrono::steady_clock::time_point last_sent_{};
  uint64_t sequence_{};
  };
