#pragma once
#include <buy_search.h>

#include <cstdint>
#include <string>
#include <vector>

#include <qwidget.h>

struct current_state_t;
struct database_storage_t;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTreeWidget;

///\brief where a handful of commodities can be bought, or sold, among the markets read - in the Trade window
///\detail the places with the most of them in the wanted amounts come first; when none has them all, the pairs that
/// do are shown above. The search runs again at every change of what is wanted or of a filter, and when shown
class buy_search_tab_t final : public QWidget
  {
  Q_OBJECT

public:
  ///\param db the window's own connection - the state's belongs to the journal following thread
  buy_search_tab_t(current_state_t const & state, database_storage_t & db, QWidget * parent = nullptr);

  auto search() -> void;

signals:
  ///\brief a place was clicked - its market is to be shown
  void market_clicked(uint64_t market_id, QString system);

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  ///\brief adds a commodity to the list, or sets the amount of one already there
  auto add_wanted(QString const & commodity, uint32_t tonnes) -> void;
  auto add_from_missions() -> void;

  [[nodiscard]]
  auto wanted() const -> std::vector<buy_search::wanted_t>;
  [[nodiscard]]
  auto filter() const -> buy_search::filter_t;

  auto show_places(std::vector<buy_search::place_t> const & ranked, std::vector<buy_search::wanted_t> const & wanted)
    -> void;
  auto show_pairs(std::vector<buy_search::place_t> const & ranked, std::vector<buy_search::wanted_t> const & wanted)
    -> void;

  current_state_t const & state_;
  database_storage_t & db_;

  QLineEdit * commodity_edit_{};
  QTableWidget * wanted_view_{};
  QCheckBox * space_stations_{};
  QCheckBox * outposts_{};
  QCheckBox * surface_ports_{};
  QCheckBox * settlements_{};
  QCheckBox * carriers_{};
  QComboBox * min_pad_{};
  QCheckBox * unknown_pads_{};
  QCheckBox * eddn_prices_{};
  ///\brief the stations buying the commodities instead of the ones selling them
  QCheckBox * selling_{};
  QSpinBox * max_age_{};
  QSpinBox * max_ly_{};
  QSpinBox * reward_{};
  QLabel * note_{};
  QLabel * pairs_caption_{};
  QTreeWidget * pairs_view_{};
  QTreeWidget * places_view_{};
  float jump_range_{};
  };
