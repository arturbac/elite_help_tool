#pragma once
#include <array>
#include "logic.h"
#include <qwidget.h>
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

///\brief managing micro resources - what sits on the carrier and where it comes from
class QTableWidget;
class QCheckBox;
class QSpinBox;

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

  ///\brief which missions pay best - credits and material rewards at what they fetch at one's own bar
  QComboBox * mission_period_{};
  QLabel * mission_note_{};
  ///\brief missions on foot and in space, one under the other
  std::array<QTableWidget *, 2> mission_views_{};

  ///\brief the consumables used up and the kills made on foot
  QComboBox * on_foot_period_{};
  QLabel * on_foot_totals_{};
  QTableWidget * consumables_view_{};
  QTableWidget * kills_view_{};

  explicit micro_resource_window_t(std::string db_path, QWidget * parent = nullptr);

  ///\brief called when the game state has changed
  auto refresh_ui() -> void;
  ///\brief the carriers' positions and jumps - read again every few seconds while the countdown runs
  auto show_carriers() -> void;
  ///\brief the cargo of the carrier chosen in the Carrier cargo tab
  auto show_carrier_cargo() -> void;
  ///\brief the commodities that can be added to the chosen carrier - read with its cargo, so that what is
  /// already on it drops out of the list
  auto fill_cargo_commodities(std::vector<info::carrier_cargo_t> const & cargo) -> void;

  auto setup_ui() -> void;

private:
  auto reload_carriers() -> void;
  auto show_stock(std::string_view carrier_id) -> void;
  [[nodiscard]]
  auto carrier_stats_line(std::string_view carrier_id) -> std::string;
  auto show_acquisitions() -> void;
  auto show_bartender() -> void;
  auto show_bar_sales() -> void;
  auto show_mission_value() -> void;
  auto show_on_foot() -> void;
  };
