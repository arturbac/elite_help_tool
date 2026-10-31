#pragma once
#include <data/carrier.h>
#include <data/micro_resources.h>
#include <array>
#include "logic.h"
#include <qwidget.h>
#include <qtimer.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qtabwidget.h>

///\brief the bartender's shelf as of the last reading
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

///\brief where micro resources come from - collected at settlements against mission rewards
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

///\brief which of the three windows the class makes - the tabs of one each
enum struct data_pages_e : uint8_t
  {
  ///\brief the commodities on the carriers, and where the carriers are
  commodities,
  ///\brief what is held of data, goods and assets, and what was used up on foot
  assets,
  ///\brief the bartender's shelf, the counters, one's own bar's sales, the missions' worth at it
  asset_trade
  };

///\brief managing micro resources - what sits on the carrier and where it comes from
class QTableWidget;
class QCheckBox;
class QSpinBox;
class QLineEdit;

class micro_resource_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;
  QTableWidget * carriers_view_{};
  ///\brief what is on each carrier - edited by hand, moved by every docking's balance
  QComboBox * cargo_carrier_{};
  QTableWidget * cargo_view_{};
  ///\brief narrows the commodities to choose from to those a colony is built from
  QCheckBox * cargo_colonisation_{};
  ///\brief the commodities of the market dictionary not yet on the carrier, the commodity key as the data
  QComboBox * cargo_commodity_{};
  QSpinBox * cargo_count_{};
  bool cargo_filling_{};

  QComboBox * carrier_combo_{};
  ///\brief the list narrowed to one's own - a stranger's bartender shows at every docking, and after a
  /// few weeks there are more of them in it than of one's own
  QCheckBox * only_mine_{};
  ///\brief marks the selected carrier as one's own - the only way to set this flag
  QCheckBox * mark_mine_{};
  QLabel * stock_header_{};
  carrier_stock_model_t * stock_model_{};
  QTableView * stock_view_{};

  QComboBox * period_combo_{};
  acquisition_model_t * acquisition_model_{};
  QTableView * acquisition_view_{};

  ///\brief what went through the counters - sold to a bartender, bought, bartered
  QComboBox * bartender_period_{};
  QLabel * bartender_totals_{};
  QTableWidget * bartender_view_{};

  ///\brief what one's own carrier's bar sold, from the falls of its shelf between readings
  QComboBox * bar_carrier_{};
  QComboBox * bar_period_{};
  QLabel * bar_totals_{};
  ///\brief Data, Goods and Assets, one under another - they sell to different buyers at different prices
  std::array<QTableWidget *, 3> bar_views_{};
  ///\brief each table's heading with its sums - the rows alone are many small numbers
  std::array<QLabel *, 3> bar_headings_{};

  ///\brief which missions pay best - credits and material rewards at what they fetch at one's own bar
  QComboBox * mission_period_{};
  QLabel * mission_note_{};
  ///\brief data and goods, one under the other - assets are left out, they are a lure on the bar
  std::array<QTableWidget *, 2> mission_views_{};

  ///\brief what is held of data, goods and assets - the backpack, the ship's locker, our carriers' stores
  ///\brief the carriers of the store columns, by market id, in their order
  std::vector<uint64_t> holdings_carriers_;
  QLineEdit * holdings_filter_{};
  ///\brief setting a kind on a carrier that is held nowhere else
  QComboBox * holdings_add_carrier_{};
  QComboBox * holdings_add_kind_{};
  QSpinBox * holdings_add_count_{};
  ///\brief the dictionary's size when the list of kinds was filled
  size_t holdings_add_known_{};
  ///\brief hides the kinds marked as junk - on at every start
  QCheckBox * holdings_no_junk_{};
  QLabel * holdings_note_{};
  std::array<QTableWidget *, 3> holdings_views_{};
  std::array<QLabel *, 3> holdings_headings_{};
  ///\brief the tables are being filled, so a changed cell is not an edit
  bool holdings_filling_{};
  ///\brief the consumables used up and the kills made on foot
  QComboBox * on_foot_period_{};
  QLabel * on_foot_totals_{};
  QTableWidget * consumables_view_{};
  QTableWidget * kills_view_{};

  micro_resource_window_t(
    std::string db_path, data_pages_e pages, current_state_t const & state, QWidget * parent = nullptr
  );

  ///\brief called when the game state has changed - read again at most once a second, and only while the
  /// window is shown; a hidden one is read when it is shown next
  ///\detail every item picked up on foot changes the state, and a reading is a dozen queries over the
  /// whole history of finds - tens to hundreds of milliseconds of the window thread each time
  auto refresh_ui() -> void;
  ///\brief the carriers' positions and jumps - read again every few seconds while the countdown runs
  auto show_carriers() -> void;
  ///\brief the cargo of the carrier chosen in the Carrier cargo tab
  auto show_carrier_cargo() -> void;
  ///\brief the commodities that can be added to the chosen carrier - read with its cargo, so that what is
  /// already on it drops out of the list
  auto fill_cargo_commodities(std::vector<info::carrier_cargo_t> const & cargo) -> void;

  auto setup_ui() -> void;

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  data_pages_e pages_;
  ///\brief the backpack and the ship's locker, read under its mutex
  current_state_t const & state_;
  auto setup_trade(QTabWidget * tabs) -> void;
  auto setup_assets(QTabWidget * tabs) -> void;
  auto setup_commodities(QTabWidget * tabs) -> void;
  ///\brief the reading refresh_ui puts off
  auto refresh_now() -> void;
  QTimer * refresh_timer_{};
  ///\brief the state changed while the window was hidden
  bool stale_{true};

  auto reload_carriers() -> void;
  auto show_stock(std::string_view carrier_id) -> void;
  [[nodiscard]]
  auto carrier_stats_line(std::string_view carrier_id) -> std::string;
  auto show_acquisitions() -> void;
  auto show_bartender() -> void;
  auto show_bar_sales() -> void;
  auto show_mission_value() -> void;
  auto show_on_foot() -> void;
  auto show_holdings() -> void;
  ///\brief the commander of this tool, whose fleet carrier is shown
  [[nodiscard]]
  auto commander_fid() -> std::string;
  };
