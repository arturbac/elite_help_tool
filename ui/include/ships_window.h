#pragma once
#include "logic.h"
#include <qlabel.h>
#include <qmdisubwindow.h>
#include <qtablewidget.h>

#include <chrono>
#include <string>

///\brief the commander's ships, where each is and how far from here
///
/// The full list comes from the game on opening a shipyard; between two visits the swaps, purchases,
/// sales and transfers keep it up. A ship left on a carrier follows the carrier's jumps
class ships_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  explicit ships_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  ///\brief reads the fleet again when it changed, when the ship moved to another system, or once a minute -
  /// for the carriers' jumps and the transfers arriving
  auto refresh_ui(bool force = false) -> void;

private:
  current_state_t const & state_;
  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;

  QLabel * header_{};
  QTableWidget * table_{};

  std::chrono::steady_clock::time_point read_{};
  uint64_t changes_seen_{~uint64_t{}};
  uint64_t system_seen_{~uint64_t{}};

  auto setup_ui() -> void;
  };
