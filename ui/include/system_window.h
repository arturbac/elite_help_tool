#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qlabel.h>
#include <qtreeview.h>
#include <qsortfilterproxymodel.h>
#include <qtabwidget.h>
#include <orbit.h>
#include <set>

struct body_info_t
  {
  body_t const * data;
  body_info_t * parent = nullptr;
  std::vector<body_info_t *> children;
  int row_in_parent = 0;
  };

class system_bodies_filter_proxy_t final : public QSortFilterProxyModel
  {
  Q_OBJECT

protected:
  using QSortFilterProxyModel::QSortFilterProxyModel;
  
  [[nodiscard]]
  auto filterAcceptsRow(int source_row, QModelIndex const & source_parent) const -> bool override;
  };

class system_bodies_model_t final : public QAbstractItemModel
  {
  Q_OBJECT
public:
  std::vector<body_t> bodies_{};
  std::unordered_map<events::body_id_t, std::unique_ptr<body_info_t>> nodes_;
  std::vector<body_info_t *> root_nodes_;
  ///\brief the column of the tick for a body to visit; the flags of the body follow it
  static constexpr int visit_column{5};
  ///\brief the bodies ticked to visit, kept through the rebuilds that every scan brings
  std::set<events::body_id_t> visits_;
  ///\brief the place of each ticked body on the calculated route, counted from 1
  std::unordered_map<events::body_id_t, size_t> route_places_;

  explicit system_bodies_model_t(std::vector<body_t> const & bodies, QObject * parent);

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

  auto setData(QModelIndex const & index, QVariant const & value, int role = Qt::EditRole) -> bool override;

  // the function called before the vector in state_ is modified
  // auto notify_update() -> void;
  auto clear() 
  {
    bodies_ = {};
    nodes_.clear();
    root_nodes_.clear();
  }
  auto rebuild_index() -> void;
  ///\brief writes the places of a calculated route into column V; no stops clears them
  auto show_route(std::span<visit_stop_t const> route) -> void;
  ///\brief the ticks and the route belong to one system
  auto forget_visits() -> void;

signals:
  ///\brief a body ticked or unticked - a route calculated before no longer holds
  void visits_changed();
  };

struct body_signal_t
{
  events::body_id_t body_id;
  std::string name;
  
  std::vector<events::signal_t> signals_;
  std::vector<events::genus_t> genuses_;
};
using body_signals_t = std::vector<body_signal_t>;

class system_bodies_signals_model_t final : public QAbstractItemModel
  {
  Q_OBJECT
  
  enum class node_type_t : int8_t { body, category_signals, category_genuses, signal_item, genus_item };

  struct internal_id_t {
    int32_t body_idx;
    node_type_t type;
    int32_t item_idx;
  };

  [[nodiscard]]
  auto pack_id(internal_id_t id) const noexcept -> quintptr;
  
  [[nodiscard]]
  auto unpack_id(quintptr id) const noexcept -> internal_id_t;
  
public:
  body_signals_t body_signals_{};
  
  explicit system_bodies_signals_model_t(body_signals_t const & bodies, QObject * parent);

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
  
  void refresh(body_signals_t && new_data);
  };
  
class system_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  current_state_t const & state_;
  system_bodies_model_t * model_{};
  system_bodies_filter_proxy_t * proxy_model_;
  
  system_bodies_signals_model_t * signals_model_;
  
  QLabel * target_label_{};
  QLabel * system_label_{};
  QLabel * fss_label_{};
  /// phenomena and landmarks from the FSS, stations excluded - those are in the system window
  QLabel * poi_label_{};
  QLabel * phenomena_label_{};
  QTreeView * tree_view{};
  QTreeView *signals_view{};
  ///\brief the calculated route under the bodies
  QLabel * route_label_{};
  ///\brief what the route is calculated from, copied with the bodies when the state is refreshed
  std::vector<bary_centre_t> bary_centres_;
  std::string system_name_;
  ///\brief the system as the first tab; the exploration's rewards beside it
  QTabWidget * tabs_{};

  explicit system_window_t(current_state_t const & state, QWidget * parent = nullptr);

  ///\brief a page beside the system's, built elsewhere - one that needs what this window does not hold
  auto add_tab(QWidget * page, QString const & title) -> void;

  auto refresh_ui() -> void;

  auto setup_ui() -> void;

  auto update_labels() -> void;

  ///\brief orders the ticked bodies from where the commander is and shows the route
  auto calculate_route() -> void;
  };

