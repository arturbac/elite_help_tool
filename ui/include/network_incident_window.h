#pragma once
#include "logic.h"
#include <qlabel.h>
#include <qmdisubwindow.h>
#include <qplaintextedit.h>
#include <qtablewidget.h>
#include <qtimer.h>

#include <stop_token>
#include <thread>

///\brief disconnects and technical failures, found automatically and correlated against the local network's
/// own state - nothing here is typed in by hand
///
/// A background thread of its own reads the game's netLog history (see network_incident.h for what it looks
/// for) and the journal's own completed files (a file with no Shutdown event means the game stopped some
/// other way), the first time right back through everything on disk, later ticks catching up on what is new.
/// Each incident found is stored with net-monitor's log of the surrounding minutes already read, since
/// journald will not keep that window open forever. The window itself only shows what has been found -
/// a table and, for the row picked, the net-monitor lines behind its verdict
class network_incident_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  ///\brief a connection of its own for the GUI thread - the scanner writes through a separate one
  database_storage_t db_;

  QLabel * summary_{};
  QTableWidget * history_view_{};
  ///\brief the net-monitor lines of the row picked in the table
  QPlainTextEdit * detail_view_{};
  ///\brief the community's reading of the codes the game itself may show - a legend, nothing here feeds it
  QLabel * legend_{};
  ///\brief what history_view_ shows, in the same order - looked up again on a row's selection
  std::vector<info::network_incident_t> rows_;

  explicit network_incident_window_t(std::string db_path, std::string journal_dir, QWidget * parent = nullptr);
  network_incident_window_t(network_incident_window_t const &) = delete;
  auto operator=(network_incident_window_t const &) -> network_incident_window_t & = delete;
  ~network_incident_window_t() override;

  auto setup_ui() -> void;
  auto reload() -> void;

private:
  std::jthread scanner_;
  QTimer * refresh_timer_{};
  ///\brief the row count reload() last drew the table with - a timer tick redraws only when this moved,
  /// rather than rebuilding hundreds of table cells every few seconds whether anything changed or not
  int64_t shown_through_{-1};

  ///\brief the cheap poll a timer can afford: reloads only when the scanner has actually found something new
  auto poll() -> void;

  ///\brief the background thread's own body - its own database connection, its own pace
  static auto run_scanner(std::stop_token stoken, std::string db_path, std::string journal_dir) -> void;
  };
