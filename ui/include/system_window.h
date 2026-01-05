#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qlabel.h>
#include <qtreeview.h>
#include <qsortfilterproxymodel.h>

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

  // Funkcja wywoływana przed modyfikacją vectora w state_
  // auto notify_update() -> void;
  auto clear() 
  {
    bodies_ = {};
    nodes_.clear();
    root_nodes_.clear();
  }
  auto rebuild_index() -> void;
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
  QTreeView * tree_view{};
  QTreeView *signals_view{};

  explicit system_window_t(current_state_t const & state, QWidget * parent = nullptr);

  auto refresh_ui() -> void;

  auto setup_ui() -> void;

  auto update_labels() -> void;
  };

