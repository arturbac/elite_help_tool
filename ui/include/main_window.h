#pragma once
#include "logic.h"
#include <journal_log.h>
#include <system_window.h>
#include <ship_loadout.h>

#include <simple_enum/simple_enum.hpp>
#include <qmainwindow.h>
#include <qmdiarea.h>
#include <thread>
#include <stop_token>
#include <qpointer.h>
#include <file_io.h>

enum struct window_type_e
  {
  none,
  system,
  journal_log,
  ship
  };

consteval auto adl_enum_bounds(window_type_e)
  {
  using enum window_type_e;
  return simple_enum::adl_info{none, ship};
  }

class main_window_t : public QMainWindow
  {
  Q_OBJECT

public:
  journal_log_window_t * jlw_{};
  current_state_t state_;
  std::jthread worker_thread_;
  QPointer<system_window_t> system_view_;
  QPointer<ship_loadout_window_t> ship_view_;
  fs::path file_to_monitor{};

  QMdiArea * mdi_area_{nullptr};

  [[nodiscard]]
  explicit main_window_t(std::string db_path, QWidget * parent = nullptr);

  auto closeEvent(QCloseEvent * event) -> void override;

private:
  auto background_worker(std::stop_token stoken) -> void;

  auto setup_ui() -> void;

  auto setup_toolbox() -> void;

  [[nodiscard]]
  auto create_tool_window(QString const & title) -> QMdiSubWindow *;

  auto save_settings() -> void;

  auto load_settings() -> void;
  };
