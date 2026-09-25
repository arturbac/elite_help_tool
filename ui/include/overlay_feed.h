#pragma once

#include "logic.h"

#include <overlay_ipc.h>

#include <chrono>
#include <memory>

///\brief zasila warstwe rysujaca w oknie gry
///
/// warstwa nie zna bazy ani logiki - dostaje gotowe linie. cala decyzja co pokazac zapada tutaj,
/// dzieki czemu zmiana tresci nie wymaga ruszania niczego w procesie gry
class overlay_feed_t final
  {
public:
  explicit overlay_feed_t(std::string socket_path);

  [[nodiscard]]
  auto listening() const noexcept -> bool;

  [[nodiscard]]
  auto clients() const noexcept -> unsigned;

  ///\brief buduje obraz ze stanu i wysyla go, o ile cokolwiek sie zmienilo
  auto publish(current_state_t const & state) -> void;

private:
  std::unique_ptr<overlay::server_t> server_;
  overlay::frame_t last_;
  std::chrono::steady_clock::time_point last_sent_{};
  uint64_t sequence_{};
  };
