#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qcombobox.h>
#include <qlabel.h>

///\brief praca w plusach doba po dobie, zestawiona z tym, co ta doba dala
///
/// Doby rozdzielaja wykryte fale przeliczen, nie stala godzina. Populacja stoi przy kazdym wierszu,
/// bo gra dzieli wplyw misji przez wielkosc systemu - ten sam wysilek daje w systemie
/// czterdziestomilionowym ulamek tego, co w czterdziestotysiecznym, wiec przelicznik plusow na
/// punkt procentowy ma sens wylacznie w obrebie jednego systemu
class bgs_effort_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    closed_by,
    system,
    population,
    faction,
    missions,
    pushed_up,
    pushed_down,
    share,
    influence,
    rate,
    column_max
    };

public:
  static constexpr int sort_role = Qt::UserRole + 1;

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

///\brief jak pozno po zapowiedzi wojny naprawde ruszaly
///
/// Oba znaczniki sa ograniczeniami, nie chwilami: przejscie w stan wojny nie trafia do journala
/// wcale, wiec jedynym sladem jest status konfliktu przy nastepnym odczycie systemu. Szerokosc
/// okna zawiera zatem takze czas, w ktorym nas tam nie bylo
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
  ///\brief domyslnie od najswiezszej wojny - interesuje to, co dzieje sie teraz
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

///\brief okno pracy BGS - ile plusow oddano i co z tego wyszlo
class bgs_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  /// wlasne polaczenie, db_ stanu nalezy do watku sledzacego journal
  database_storage_t db_;

  QComboBox * period_combo_{};
  QComboBox * system_combo_{};
  ///\brief ostatnia fala i to, jak regularnie przychodzi - bez tego kolumna "zamknieta" nic nie mowi
  QLabel * tick_header_{};
  bgs_effort_model_t * model_{};
  QTableView * view_{};

  ///\brief rozrzut opoznienia zapowiedzi wojny - najkrotszy, mediana, najdluzszy
  QLabel * war_header_{};
  war_onset_model_t * war_model_{};
  QTableView * war_view_{};

  explicit bgs_window_t(std::string db_path, QWidget * parent = nullptr);

  ///\brief wolane gdy stan gry sie zmienil
  auto refresh_ui() -> void;

  auto setup_ui() -> void;

private:
  ///\brief uzupelnia liste systemow tymi, w ktorych naprawde pracowalismy
  auto reload_systems() -> void;
  auto show_effort() -> void;
  auto show_wars() -> void;
  };
