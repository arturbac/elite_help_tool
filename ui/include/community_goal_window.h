#pragma once
#include "logic.h"
#include <qlabel.h>
#include <qmdisubwindow.h>
#include <qtablewidget.h>

#include <chrono>

///\brief the community goals the commander takes part in, and which reward bracket their part reaches
///
/// Nothing is stored: the game writes the whole list at login and again on opening a goal's panel, so the
/// window shows the last of those readings - the band a delivery moved to is known after the panel is opened
class community_goal_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  explicit community_goal_window_t(current_state_t const & state, QWidget * parent = nullptr);

  ///\brief draws the goals again when a new reading came, and once a minute for the time left
  auto refresh_ui(bool force = false) -> void;

private:
  current_state_t const & state_;

  QLabel * header_{};
  QTableWidget * table_{};

  std::chrono::steady_clock::time_point drawn_{};
  uint64_t changes_seen_{~uint64_t{}};

  auto setup_ui() -> void;
  };
