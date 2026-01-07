#pragma once
#include "logic.h"
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>

class mission_model_t final : public QAbstractItemModel
  {
  Q_OBJECT
public:
  std::vector<info::mission_t> missions_{};
  enum struct column_e : int
    {
    id,
    destination,
    status,
    faction,
    reward,
    type,
    count_max
    };
  explicit mission_model_t(std::vector<info::mission_t> const & m, QObject * parent);

  auto reload(std::vector<info::mission_t> const &) -> void;

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
  auto flags(QModelIndex const & index) const -> Qt::ItemFlags override;
  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;
  };

class mission_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  current_state_t const & state_;
  mission_model_t * model_{};
  QTableView * view_{};

  explicit mission_window_t(current_state_t const & state, QWidget * parent = nullptr);

  /// called when state_ has new data for missions
  auto refresh_ui() -> void;

  auto setup_ui() -> void;
  };
