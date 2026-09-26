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

///\brief the state of the factions in one system, the last known row of each
struct faction_presence_t
  {
  std::string name;
  info::government_e government;
  info::allegiance_e allegiance;
  std::string pending;     // PendingStates z ostatniego wpisu
  std::string active;      // ActiveStates, a gdy puste to FactionState
  std::string recovering;  // RecoveringStates - stany z ktorych frakcja wychodzi
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
    recovering,
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

///\brief conflicts in the system - wars, civil wars and elections
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

///\brief stations, installations and carriers in the system
class system_station_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    signal_type,
    name,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

  std::vector<system_signal_t> stations_{};

  explicit system_station_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<system_signal_t> && new_data) -> void;
  };

///\brief one side of a market - what the station sells, or what it buys
class market_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    name,
    category,
    price,
    quantity,
    deviation,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

  ///\brief true for goods the station sells, false for the ones it buys
  bool station_sells_{};
  std::vector<info::market_entry_t> entries_{};

  explicit market_model_t(bool station_sells, QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::market_entry_t> && new_data) -> void;
  };

///\brief how many missions, and of what kind, were done for each faction
class mission_stat_model_t final : public QAbstractTableModel
  {
  Q_OBJECT

  enum struct column_e : int
    {
    faction,
    missions,
    rewards,
    top_type,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

  std::vector<info::mission_stat_t> rows_{};

  explicit mission_stat_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::mission_stat_t> && new_data) -> void;
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

  ///\brief when this system and the galaxy last recalculated - right at the top, because it decides
  /// whether a mission handed in now still counts towards this day
  QLabel * bgs_tick_label_{};
  ///\brief the span the last wave took across the galaxy, and how regularly it comes
  QLabel * bgs_galaxy_label_{};
  ///\brief a separate clock, shown only while a conflict is running in the system
  QLabel * war_tick_label_{};
  QLabel * war_tick_row_label_{};
  QLabel * war_galaxy_label_{};
  QLabel * war_galaxy_row_label_{};
  ///\brief ile jeszcze przeliczen do rozstrzygniecia trwajacych wojen
  QLabel * war_countdown_label_{};
  QLabel * war_countdown_row_label_{};

  ///\brief when the clocks were last worked out, and for which system
  ///\detail working them out costs several queries with correlated subqueries, and the window refreshes on
  /// every change of game state - without this it would be hundreds of queries a minute for data that
  /// raz na dobe
  std::chrono::steady_clock::time_point ticks_loaded_{};
  uint64_t ticks_system_{};

  ///\brief the system described in four lines, without captions - the value says what it is
  ///\detail "Industrial / Agriculture", "Federation - Anarchy, Anarchy" on the left, the owner
  /// with the population and the star with its coordinates on the right
  QLabel * economy_label_{};
  QLabel * politics_label_{};
  QLabel * owner_label_{};
  QLabel * star_label_{};

  faction_presence_model_t * factions_model_{};
  QTableView * factions_view_{};

  system_conflict_model_t * conflicts_model_{};
  QTableView * conflicts_view_{};
  QLabel * conflicts_note_{};
  ///\brief the caption above the conflict table - hidden along with it when there is nothing to show
  QLabel * conflicts_caption_{};
  ///\brief the splitter panel holding the conflicts - its top edge gives room back to the faction list
  QWidget * conflicts_container_{};

  QChartView * chart_view_{};
  QChart * chart_{};

  system_station_model_t * stations_model_{};
  QTableView * stations_view_{};
  QCheckBox * hide_carriers_{};
  QCheckBox * hide_installations_{};

  QComboBox * mission_period_combo_{};
  QCheckBox * missions_this_system_{};
  QLabel * mission_header_{};
  mission_stat_model_t * mission_model_{};
  QTableView * mission_view_{};

  QTabWidget * tabs_{};
  ///\brief the market tab cannot be addressed by index - new ones appear and the numbers shift
  QWidget * market_page_{};
  QLabel * market_header_{};
  market_model_t * market_sells_model_{};
  market_model_t * market_buys_model_{};
  QTableView * market_sells_view_{};
  QTableView * market_buys_view_{};

  explicit faction_state_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  ///\brief called when the game state has changed - odswieza widok jesli sledzimy biezacy system
  auto refresh_ui() -> void;

  auto setup_ui() -> void;

private:
  auto reload_system_list() -> void;
  auto show_system(uint64_t system_address) -> void;
  auto update_system_info(uint64_t system_address) -> void;

  ///\brief the top two lines - the influence recalculation and, during a conflict, the war one
  auto update_tick_labels(uint64_t system_address) -> void;
  auto update_conflicts(uint64_t system_address) -> void;
  auto update_stations(uint64_t system_address) -> void;

  ///\brief shows the market of the station clicked in the list and switches to its tab
  auto show_market(std::string_view station_name) -> void;

  auto update_missions() -> void;

  ///\brief jeden punkt na dobe, ostatni pomiar dnia, ograniczone do wybranego zakresu
  auto update_chart() -> void;

  [[nodiscard]]
  auto faction_name(int64_t faction_oid) const -> std::string;

  uint64_t shown_system_{};

  /// kept so that changing the range needs no fresh query to the database
  std::vector<info::faction_influence_t> history_{};
  };
