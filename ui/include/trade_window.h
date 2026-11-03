#pragma once
#include <data/market.h>
#include <data/station.h>
#include <databse_storage.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <qmdisubwindow.h>

struct current_state_t;
class buy_search_tab_t;
class market_model_t;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QStringListModel;
class QTabWidget;
class QTableView;
class QTreeWidget;

///\brief the markets of the systems our commanders claimed: which were never read, and which changed hands since
///\detail read again whenever the tab is shown - a claim, a docking or a war since changes it
class to_update_tab_t final : public QWidget
  {
  Q_OBJECT

public:
  to_update_tab_t(current_state_t const & state, database_storage_t & db, QWidget * parent = nullptr);

  auto refresh() -> void;

signals:
  void market_clicked(uint64_t market_id, QString system);

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  current_state_t const & state_;
  database_storage_t & db_;
  QCheckBox * space_stations_{};
  QCheckBox * outposts_{};
  QCheckBox * surface_ports_{};
  QCheckBox * settlements_{};
  QCheckBox * only_to_update_{};
  QLabel * note_{};
  QTreeWidget * view_{};
  };

///\brief trade: where to buy or sell a handful of commodities, the market of a place picked by its system, and the
/// markets of our own systems still to be read
class trade_window_t final : public QMdiSubWindow
  {
  Q_OBJECT

public:
  trade_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  ///\brief shows the market of a station of the system, by its name - clicked in the System info window
  auto show_market(uint64_t system_address, std::string_view station_name) -> void;
  ///\brief shows the market of a station of any system; the system's name, when given, heads it
  auto show_market_of(uint64_t market_id, std::string_view system) -> void;

private:
  auto show_reading(uint64_t market_id, std::string_view name, std::string_view system) -> void;
  auto clear_market() -> void;
  ///\brief reads anew the places whose market was read, for the system field to offer
  auto load_places() -> void;
  ///\brief lists the places of the system named, the one of the market id picked - nothing is shown by it
  auto list_places(QString const & system, uint64_t market_id) -> void;
  ///\brief the market of the place picked in the list
  auto show_picked() -> void;
  ///\brief the system the commander is in, and the port last docked at when it is there
  auto show_here() -> void;

  current_state_t const & state_;
  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;
  std::vector<info::market_place_t> places_;
  QTabWidget * tabs_{};
  buy_search_tab_t * search_{};
  ///\brief the market tab cannot be addressed by index - new ones appear and the numbers shift
  QWidget * market_page_{};
  QLabel * market_header_{};
  QLineEdit * system_edit_{};
  QStringListModel * systems_model_{};
  QComboBox * place_combo_{};
  market_model_t * market_sells_model_{};
  market_model_t * market_buys_model_{};
  QTableView * market_sells_view_{};
  QTableView * market_buys_view_{};
  to_update_tab_t * to_update_{};
  };
