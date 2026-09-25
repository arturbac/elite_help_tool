#pragma once
#include "logic.h"
#include <journal_log.h>
#include <system_window.h>
#include <ship_loadout.h>
#include <mission_window.h>
#include <route_window.h>
#include <faction_window.h>
#include <faction_state_window.h>

#include <simple_enum/simple_enum.hpp>
#include <qmainwindow.h>
#include <qmdiarea.h>
#include <thread>
#include <stop_token>
#include <qpointer.h>
#include <file_io.h>

enum struct window_type_e
  {
  ///\brief nieuzywane, zostaje dla zapisanych ukladow z czasow okien zastepczych
  none,
  system,
  journal_log,
  mission,
  route,
  ship,
  faction,
  faction_state
  };

consteval auto adl_enum_bounds(window_type_e)
  {
  using enum window_type_e;
  return simple_enum::adl_info{none, faction_state};
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
  QPointer<mission_window_t> mission_view_;
  QPointer<route_window_t> route_view_;
  QPointer<faction_window_t> faction_view_;
  QPointer<faction_state_window_t> faction_state_view_;
  
  fs::path file_to_monitor{};

  QMdiArea * mdi_area_{nullptr};

  [[nodiscard]]
  explicit main_window_t(std::string db_path, std::string journal_path, QWidget * parent = nullptr);

  std::string db_path_;

  ///\brief startuje watek sledzacy journal, wolane po otwarciu bazy
  auto start_monitoring() -> void;

  auto closeEvent(QCloseEvent * event) -> void override;

  ///\brief pokazuje okno narzedziowe i wyciaga je na wierzch MDI
  auto activate_window(window_type_e type) -> void;

private:
  [[nodiscard]]
  auto subwindow_for(window_type_e type) const -> QMdiSubWindow *;

  ///\brief wpina okno w MDI, znakuje typem i odbiera mu mozliwosc zamkniecia
  auto add_tool_window(QMdiSubWindow * sub, window_type_e type) -> void;

  /// filtr blokujacy zamykanie okien narzedziowych
  QObject * close_blocker_{};

  auto background_worker(std::stop_token stoken) -> void;

  auto setup_ui() -> void;

  auto setup_toolbox() -> void;

  auto save_settings() -> void;

  auto load_settings() -> void;
  };
