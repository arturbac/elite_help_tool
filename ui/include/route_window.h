#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qlabel.h>

class route_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
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
  route_model_t * model_{};
  QLabel* info_label_{};
  QTableView* table_view_{};
    
  explicit route_window_t(current_state_t const & state, QWidget * parent = nullptr);

  auto refresh_ui() -> void;

  auto setup_ui() -> void;
  };
