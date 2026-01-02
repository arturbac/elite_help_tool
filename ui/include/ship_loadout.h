#pragma once
#include <elite_events.h>
#include <qmdisubwindow.h>
#include <qlabel.h>
#include <qprogressbar.h>
#include <qboxlayout.h>
#include <vector>


class ship_loadout_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
  public:
  ship_loadout_t * ship_loadout_;
  
  QLabel * ship_info_label{};
  QProgressBar * hull_bar{};
  QProgressBar * fuel_bar{};
  QProgressBar * cargo_bar{};

  // Kontener na wiersze modułów, by móc je dynamicznie czyścić/edytować
  QVBoxLayout * modules_layout{};

  struct module_row_t
    {
    QProgressBar * health;
    QLabel * prio;
    QLabel * status;
    };

  std::vector<module_row_t> module_rows;
  
  explicit ship_loadout_window_t(ship_loadout_t & state, QWidget * parent = nullptr);
  
  auto setup_ui() -> void;
  auto refresh_ui() -> void;
  };
