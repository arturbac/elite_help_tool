#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <QtCharts/qchartview.h>
#include <QtCharts/qlineseries.h>
#include <QtCharts/qdatetimeaxis.h>
#include <QtCharts/qvalueaxis.h>
#include <QtCharts/qlogvalueaxis.h>

///\brief stan frakcji w jednym systemie, ostatni znany wpis kazdej z nich
struct faction_presence_t
  {
  std::string name;
  info::government_e government;
  info::allegiance_e allegiance;
  std::string pending;  // PendingStates z ostatniego wpisu
  std::string active;   // ActiveStates, a gdy puste to FactionState
  double influence;
  };

class faction_presence_model_t final : public QAbstractTableModel
  {
  Q_OBJECT

  enum struct column_e : int
    {
    name,
    government,
    allegiance,
    pending,
    active,
    influence,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

  std::vector<faction_presence_t> factions_{};

  explicit faction_presence_model_t(QObject * parent);

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

  auto update_data(std::vector<faction_presence_t> && new_data) -> void;
  };

///\brief konflikty w systemie - wojny, wojny domowe i wybory
class system_conflict_model_t final : public QAbstractTableModel
  {
  Q_OBJECT

  enum struct column_e : int
    {
    war_type,
    status,
    faction1,
    stake1,
    won1,
    faction2,
    stake2,
    won2,
    column_max
    };

public:
  std::vector<info::conflict_t> conflicts_{};

  explicit system_conflict_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::conflict_t> && new_data) -> void;
  };

class faction_state_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  current_state_t const & state_;

  /// wlasne polaczenie, db_ stanu nalezy do watku sledzacego journal
  database_storage_t db_;

  QCheckBox * follow_current_{};
  QComboBox * system_combo_{};
  QComboBox * range_combo_{};
  QComboBox * scale_combo_{};

  QLabel * economy_label_{};
  QLabel * government_label_{};
  QLabel * allegiance_label_{};
  QLabel * security_label_{};
  QLabel * population_label_{};
  QLabel * controlling_label_{};
  QLabel * star_type_label_{};
  QLabel * coordinates_label_{};

  faction_presence_model_t * factions_model_{};
  QTableView * factions_view_{};

  system_conflict_model_t * conflicts_model_{};
  QTableView * conflicts_view_{};
  QLabel * conflicts_note_{};

  QChartView * chart_view_{};
  QChart * chart_{};

  explicit faction_state_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  ///\brief wolane gdy stan gry sie zmienil - odswieza widok jesli sledzimy biezacy system
  auto refresh_ui() -> void;

  auto setup_ui() -> void;

private:
  auto reload_system_list() -> void;
  auto show_system(uint64_t system_address) -> void;
  auto update_system_info(uint64_t system_address) -> void;
  auto update_conflicts(uint64_t system_address) -> void;

  ///\brief jeden punkt na dobe, ostatni pomiar dnia, ograniczone do wybranego zakresu
  auto update_chart() -> void;

  [[nodiscard]]
  auto faction_name(int64_t faction_oid) const -> std::string;

  uint64_t shown_system_{};

  /// trzymana zeby zmiana zakresu nie wymagala ponownego zapytania do bazy
  std::vector<info::faction_influence_t> history_{};
  };
