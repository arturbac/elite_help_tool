#pragma once
#include "logic.h"
#include <journal_log.h>
#include <system_window.h>
#include <ship_loadout.h>
#include <mission_window.h>
#include <route_window.h>
#include <faction_window.h>
#include <faction_state_window.h>
#include <micro_resource_window.h>
#include <bgs_window.h>
#include <construction_window.h>
#include <network_incident_window.h>
#include <credits_window.h>
#include <ships_window.h>
#include <surface_window.h>
#include <eht_extension.h>
#include <overlay_feed.h>
#include <eddn_sender.h>

#include <simple_enum/simple_enum.hpp>
#include <chrono>
#include <qmainwindow.h>
#include <qmdiarea.h>
#include <thread>
#include <stop_token>
#include <qpointer.h>
#include <file_io.h>
#include <qtimer.h>
#include <backup.h>
#include <future>

enum struct window_type_e
  {
  ///\brief unused; kept for saved layouts from the days of placeholder windows
  none,
  system,
  journal_log,
  mission,
  route,
  ship,
  faction,
  faction_state,
  micro_resource,
  bgs,
  construction,
  ships,
  surface,
  network_incident,
  credits
  };

consteval auto adl_enum_bounds(window_type_e)
  {
  using enum window_type_e;
  return simple_enum::adl_info{none, credits};
  }

class main_window_t : public QMainWindow
  {
  Q_OBJECT

public:
  journal_log_window_t * jlw_{};
  ///\brief what goes to EDDN - declared before the journal's thread, so they outlive what feeds them
  std::unique_ptr<eddn::sender_t> eddn_sender_;
  std::unique_ptr<eddn::publisher_t> eddn_publisher_;
  ///\brief an extension built in from outside the repository, or none
  std::unique_ptr<eht::extension::extension_t> extension_;
  current_state_t state_;
  std::jthread worker_thread_;
  QPointer<system_window_t> system_view_;
  QPointer<ship_loadout_window_t> ship_view_;
  QPointer<mission_window_t> mission_view_;
  QPointer<route_window_t> route_view_;
  QPointer<faction_window_t> faction_view_;
  QPointer<faction_state_window_t> faction_state_view_;
  QPointer<micro_resource_window_t> micro_resource_view_;
  QPointer<bgs_window_t> bgs_view_;
  QPointer<construction_window_t> construction_view_;
  QPointer<ships_window_t> ships_view_;
  QPointer<surface_window_t> surface_view_;
  QPointer<network_incident_window_t> network_incident_view_;
  QPointer<credits_window_t> credits_view_;

  ///\brief feeds the overlay in the game window; it lives whether or not the game is running at all
  std::unique_ptr<overlay_feed_t> overlay_feed_;
  ///\brief keeps the image alive when nothing arrives from the journal, while docked for instance
  QTimer * overlay_timer_{};
  ///\brief the backup running in the background, if one is
  std::future<backup::summary_t> backup_;
  ///\brief what the running backup is to be marked with once it is done
  std::filesystem::path backup_destination_;
  uint64_t backup_pictures_{};
  ///\brief the game's own directory as last found - the launcher's game is found only while it runs
  std::filesystem::path backup_game_dir_;
  ///\brief starts a backup when one is due, and takes its result when it is done
  auto follow_backup() -> void;

  fs::path file_to_monitor{};

  QMdiArea * mdi_area_{nullptr};

  [[nodiscard]]
  explicit main_window_t(std::string db_path, std::string journal_path, QWidget * parent = nullptr);

  std::string db_path_;

  ///\brief starts the journal following thread, called once the database is open
  auto start_monitoring() -> void;

  auto closeEvent(QCloseEvent * event) -> void override;

  ///\brief shows a tool window and brings it to the front of the MDI
  auto activate_window(window_type_e type) -> void;

  ///\brief copies the current state over to the overlay; called from the gui thread
  auto publish_overlay() -> void;

  ///\brief opens the window that suits where we are, until the journal replay settles
  ///\detail starting the tool walks through every system the newest journal passed through before
  /// reaching the one we are actually in, so the choice has to be allowed to change its mind - and
  /// then to stop, rather than following the ship around for the rest of the evening
  auto choose_opening_window(bool inhabited) -> void;

private:
  [[nodiscard]]
  auto subwindow_for(window_type_e type) const -> QMdiSubWindow *;

  ///\brief puts a window into the MDI, marks it with its type and takes away its close button
  auto add_tool_window(QMdiSubWindow * sub, window_type_e type) -> void;

  /// the filter that stops tool windows from being closed
  QObject * close_blocker_{};

  std::chrono::steady_clock::time_point monitoring_started_{};
  ///\brief set once the replay has settled, after which the windows are the user's business
  bool opening_settled_{};

  auto background_worker(std::stop_token stoken) -> void;

  auto setup_ui() -> void;

  auto setup_toolbox() -> void;

  auto save_settings() -> void;

  auto load_settings() -> void;
  };
