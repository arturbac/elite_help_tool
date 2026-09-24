#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qsortfilterproxymodel.h>
#include <qtableview.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlabel.h>

class faction_model_t final : public QAbstractTableModel
  {
  Q_OBJECT

  enum struct column_e : int
    {
    name,
    reputation,
    allegiance,
    influence,
    government,
    column_max
    };

public:
  // wartość surowa do sortowania, obok sformatowanej dla widoku
  static constexpr int sort_role = Qt::UserRole + 1;

  std::vector<info::faction_info_t> factions_{};

  explicit faction_model_t(std::vector<info::faction_info_t> factions, QObject * parent);

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

  auto update_data(std::vector<info::faction_info_t> && new_factions) -> void;
  };

class faction_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  current_state_t & state_;
  faction_model_t * model_{};
  QSortFilterProxyModel * proxy_{};
  QLineEdit * search_edit_{};
  QCheckBox * current_system_only_{};
  QLabel * info_label_{};
  QTableView * table_view_{};

  explicit faction_window_t(current_state_t & state, QWidget * parent = nullptr);

  auto refresh_ui() -> void;

  auto setup_ui() -> void;

private:
  [[nodiscard]]
  auto collect_factions() const -> std::vector<info::faction_info_t>;
  };
