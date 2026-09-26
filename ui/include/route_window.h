#pragma once
#include "logic.h"
#include <qwidget.h>
#include <qmdiarea.h>
#include <qmdisubwindow.h>
#include <qabstractitemmodel.h>
#include <qtableview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcheckbox.h>

class route_model_t final : public QAbstractTableModel
  {
  Q_OBJECT
  enum struct column_e : int
    {
    system,
    star_type,
    distance,
    visited,
    column_max
    };
public:
  std::vector<info::route_item_t> route_{};

  explicit route_model_t(std::vector<info::route_item_t> const & route, QObject * parent);

  [[nodiscard]]
  auto hasChildren(QModelIndex const & parent = QModelIndex()) const -> bool override;

  [[nodiscard]]
  auto index(int row, int column, QModelIndex const & parent = QModelIndex()) const -> QModelIndex override;

  [[nodiscard]]
  auto parent(QModelIndex const & index) const -> QModelIndex override;

  [[nodiscard]]
  auto rowCount(QModelIndex const & parent = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto columnCount(QModelIndex const & = QModelIndex()) const -> int override;

  [[nodiscard]]
  auto flags(QModelIndex const& index) const -> Qt::ItemFlags override;
  [[nodiscard]]
  auto data(QModelIndex const & index, int role = Qt::DisplayRole) const -> QVariant override;

  [[nodiscard]]
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;

  auto update_data(std::vector<info::route_item_t> && new_route) -> void;
  };
class route_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  current_state_t const & state_;
  /// wlasne polaczenie, db_ stanu nalezy do watku sledzacego journal
  database_storage_t db_;

  route_model_t * model_{};
  QLabel* info_label_{};
  QTableView* table_view_{};

  QPushButton * load_button_{};
  QPushButton * remember_button_{};
  QPushButton * forget_button_{};
  ///\brief plik ze spansh opisuje droge powrotna, wiec domyslnie lecimy nim od konca
  QCheckBox * reversed_box_{};

  ///\brief trasa wyznaczona na zewnatrz, w kolejnosci lotu; pusta znaczy "pokazuj trase z gry"
  std::vector<info::neutron_waypoint_t> neutron_route_;
  std::string neutron_name_;
  ///\brief czy pokazywana trasa jest ta zapamietana, czy wczytana na jeden raz
  bool remembered_{};
  ///\brief ostatni cel wrzucony do schowka - zeby wrzucac go tylko przy zmianie, a nie co odswiezenie
  std::string clipboard_target_;

  explicit route_window_t(current_state_t const & state, std::string db_path, QWidget * parent = nullptr);

  auto refresh_ui() -> void;

private:
  ///\brief wczytuje trase wyznaczona przez spansh; nie zapisuje jej nigdzie
  auto load_from_file() -> void;
  ///\brief odwraca kolejnosc i przelicza odleglosci miedzy sasiadami
  auto apply_direction(std::vector<info::neutron_waypoint_t> route) -> void;
  ///\brief pokazuje trase neutronowa gdy jest, a w przeciwnym razie trase z gry
  auto show_route() -> void;

  auto setup_ui() -> void;
  };
