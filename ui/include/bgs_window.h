#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qcombobox.h>
#include <qlabel.h>

///\brief effort in pluses day by day, set against what that day gave
///
/// Days are separated by detected recalculation waves, not by a fixed hour. Population sits on every row,
/// because the game divides mission influence by the size of the system - the same effort gives, in a
/// forty-million system a fraction of what they give in a forty-thousand one, so pluses per
/// percentage point only makes sense within a single system
class bgs_effort_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    closed_by,
    system,
    population,
    faction,
    faction_state,
    missions,
    pluses,
    share,
    influence,
    rate,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;
  ///\brief the columns that give width back to the rest - names survive elision, numbers do not
  static constexpr int stretch_column = int(column_e::system);
  static constexpr int second_stretch_column = int(column_e::faction);

  std::vector<info::bgs_effort_t> rows_{};

  explicit bgs_effort_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::bgs_effort_t> && new_data) -> void;
  };

///\brief how late after the announcement the wars actually started
///
/// Both marks are bounds, not moments: the transition into the war state never reaches the journal
/// at all, so the only trace is the conflict status at the next reading of the system. The width
/// therefore also covers the time we were not there
class war_onset_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    system,
    war_type,
    window,
    faction1,
    state,
    faction2,
    pending_last,
    active_first,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;
  ///\brief newest war first by default - what is happening now is what matters
  static constexpr int default_sort_column = int(column_e::active_first);

  std::vector<info::war_onset_t> rows_{};

  explicit war_onset_model_t(QObject * parent);

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::war_onset_t> && new_data) -> void;
  };

///\brief the BGS effort window - how many pluses were handed in and what came of it
class bgs_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;

  QComboBox * period_combo_{};
  QComboBox * system_combo_{};
  ///\brief the last wave and how regularly it comes - without it the "closed" column says nothing
  QLabel * tick_header_{};
  bgs_effort_model_t * model_{};
  QTableView * view_{};

  ///\brief the spread of the war announcement's delay - shortest, median, longest
  QLabel * war_header_{};
  war_onset_model_t * war_model_{};
  QTableView * war_view_{};

  explicit bgs_window_t(std::string db_path, QWidget * parent = nullptr);

  ///\brief called when the game state has changed
  auto refresh_ui() -> void;

  auto setup_ui() -> void;

private:
  ///\brief fills the system list with the ones we really worked in
  auto reload_systems() -> void;
  auto show_effort() -> void;
  auto show_wars() -> void;
  };
