#pragma once
#include "logic.h"
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qmdisubwindow.h>
#include <qtablewidget.h>

#include <chrono>
#include <map>
#include <string>
#include <vector>

///\brief the carrier a site is supplied from, as chosen in the Construction window; 0 for none
[[nodiscard]]
auto construction_supplier(uint64_t site_market) -> uint64_t;

///\brief the construction sites under way in our commanders' systems, and what each still needs
///
/// What a site needs comes from the game itself - it writes the site's whole state on every docking
/// there, and each delivery as it is handed in. Beside every commodity: how much of it is in the hold,
/// and how much the port we stand at sells, and for how much. With this window the active one, the
/// overlay shows the site chosen here, wherever the flight is going.
class construction_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  explicit construction_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  ///\brief reads the sites again - at most every couple of seconds unless forced, or when they changed
  auto refresh_ui(bool force = false) -> void;

  ///\brief the site chosen in the window, 0 when there is none
  [[nodiscard]]
  auto selected_market() const -> uint64_t;

private:
  current_state_t const & state_;
  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;

  QComboBox * site_combo_{};
  ///\brief the game never says a site lapsed unless it is visited - giving one up is the commander's call
  QPushButton * abandon_button_{};
  QCheckBox * show_abandoned_{};
  ///\brief the carrier the site is supplied from, remembered for each site; none means all our carriers
  QComboBox * carrier_combo_{};
  QLabel * header_{};
  QTableWidget * table_{};

  std::vector<info::construction_site_t> sites_;
  std::chrono::steady_clock::time_point read_{};
  uint64_t changes_seen_{~uint64_t{}};

  auto setup_ui() -> void;
  auto show_site() -> void;
  ///\brief puts the carrier remembered for the chosen site back into the box
  auto restore_carrier() -> void;
  };
