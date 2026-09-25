#pragma once

#include "logic.h"

#include <overlay_ipc.h>

#include <chrono>
#include <memory>
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

  ///\brief buduje obraz ze stanu i wysyla go, o ile cokolwiek sie zmienilo
  auto publish(current_state_t const & state) -> void;

private:
  ///\brief influence nie siedzi w stanie, trzeba po nie do bazy - wlasne polaczenie jak w oknach
  auto refresh_factions(current_state_t const & state) -> void;

  std::unique_ptr<overlay::server_t> server_;
  database_storage_t db_;
  uint64_t factions_system_{};
  std::chrono::steady_clock::time_point factions_loaded_{};
  std::vector<overlay::line_t> faction_lines_;
  overlay::frame_t last_;
  std::chrono::steady_clock::time_point last_sent_{};
  uint64_t sequence_{};
  };
