#pragma once
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
  /// wlasne polaczenie, db_ stanu nalezy do watku sledzacego journal
  database_storage_t db_;

  route_model_t * model_{};
  QLabel* info_label_{};
  QTableView* table_view_{};

  QPushButton * load_button_{};
  QPushButton * remember_button_{};
  QPushButton * forget_button_{};
  ///\brief the spansh file describes the way back, so by default we fly it in reverse
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

  explicit route_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  auto refresh_ui() -> void;

private:
  ///\brief loads a route plotted by spansh; it stores it nowhere
  auto load_from_file() -> void;
  ///\brief reverses the order and recomputes the distances between neighbours
  auto apply_direction(std::vector<info::neutron_waypoint_t> route) -> void;
  ///\brief shows the neutron route when there is one, otherwise the game's route
  auto show_route() -> void;
  ///\brief sets progress to the given waypoint - for stepping back and for joining a route halfway
  auto jump_to_waypoint(int row) -> void;

  auto setup_ui() -> void;
  };
