#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qcombobox.h>
#include <qlabel.h>
#include <qtabwidget.h>

///\brief polka bartendera z ostatniego odczytu
class carrier_stock_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    category,
    name,
    price,
    stock,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

  std::vector<info::carrier_stock_t> stock_{};

  explicit carrier_stock_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::carrier_stock_t> && new_data) -> void;
  };

///\brief skad biora sie mikrozasoby - zebrane w osadach kontra nagrody z misji
class acquisition_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    category,
    name,
    collected,
    from_missions,
    total,
    economy,
    last_seen,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

  std::vector<info::acquisition_summary_t> rows_{};

  explicit acquisition_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::acquisition_summary_t> && new_data) -> void;
  };

///\brief zarzadzanie mikrozasobami - co lezy na flotowcu i skad to sie bierze
class micro_resource_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  /// wlasne polaczenie, db_ stanu nalezy do watku sledzacego journal
  database_storage_t db_;

  QComboBox * carrier_combo_{};
  QLabel * stock_header_{};
  carrier_stock_model_t * stock_model_{};
  QTableView * stock_view_{};

  QComboBox * period_combo_{};
  acquisition_model_t * acquisition_model_{};
  QTableView * acquisition_view_{};

  explicit micro_resource_window_t(std::string db_path, QWidget * parent = nullptr);

  ///\brief wolane gdy stan gry sie zmienil
  auto refresh_ui() -> void;

  auto setup_ui() -> void;

private:
  auto reload_carriers() -> void;
  auto show_stock(std::string_view carrier_id) -> void;
  auto show_acquisitions() -> void;
  };
