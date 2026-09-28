#pragma once

#include <functional>
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcheckbox.h>

class route_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    system,
    star_type,
    distance,
    visited,
    column_max
    };
public:
  std::vector<info::route_item_t> route_{};

  explicit route_model_t(std::vector<info::route_item_t> const & route, QObject * parent);

  [[nodiscard]]
  auto hasChildren(QModelIndex const & parent = QModelIndex()) const -> bool override;

  [[nodiscard]]
  auto index(int row, int column, QModelIndex const & parent = QModelIndex()) const -> QModelIndex override;

  [[nodiscard]]
  auto parent(QModelIndex const & index) const -> QModelIndex override;

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto flags(QModelIndex const& index) const -> Qt::ItemFlags override;
  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::route_item_t> && new_route) -> void;
  };
class route_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  current_state_t const & state_;
  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;

  route_model_t * model_{};
  QLabel* info_label_{};
  QTableView* table_view_{};

  QPushButton * load_button_{};
  QPushButton * remember_button_{};
  QPushButton * forget_button_{};
  ///\brief drops a route loaded for a single trip; the remembered one, if any, comes back
  QPushButton * clear_button_{};
  ///\brief the next waypoint as the destination - to the clipboard, or to whatever set_destination_ does
  QPushButton * destination_button_{};
  std::function<void(std::string const &)> set_destination_;
  ///\brief a spansh file can be read either way; chosen before loading or changed after
  QCheckBox * reversed_box_{};

  ///\brief the externally plotted route in flight order; empty means "show the game's route"
  std::vector<info::neutron_waypoint_t> neutron_route_;
  std::string neutron_name_;
  ///\brief whether the shown route is the remembered one or loaded for a single trip
  bool remembered_{};
  ///\brief the last target put in the clipboard - so it is put there on change only, not on every refresh
  std::string clipboard_target_;

  ///\brief how many waypoints of the route are behind us
  ///
  /// Progress only ever moves forward. The game plots the course to the next waypoint itself and can
  /// lead through systems that are not on this list - looking the current system up every
  /// time would then reset progress to zero and the clipboard would get the first waypoint instead of
  /// the next one
  size_t reached_{};
  ///\brief the system progress was last taken from - a route can pass the same system more than once,
  /// so progress moves at an arrival only, and to the first match of the system from the next waypoint on
  uint64_t progress_system_{};

  explicit route_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  auto refresh_ui() -> void;

  ///\brief what the destination button does instead of copying the next waypoint to the clipboard
  auto set_destination_handler(std::function<void(std::string const &)> handler) -> void;

private:
  ///\brief loads a route plotted by spansh; it stores it nowhere
  auto load_from_file() -> void;
  ///\brief reverses the order and recomputes the distances between neighbours
  auto apply_direction(std::vector<info::neutron_waypoint_t> route) -> void;
  ///\brief empties the window of the neutron route, back to the game's route
  auto drop_route() -> void;
  ///\brief shows the neutron route when there is one, otherwise the game's route
  auto show_route() -> void;
  ///\brief sets progress to the given waypoint - for stepping back and for joining a route halfway
  auto jump_to_waypoint(int row) -> void;

  auto setup_ui() -> void;
  };
