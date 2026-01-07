#include <mission_window.h>
#include <qformat.h>
#include <simple_enum/std_format.hpp>
#include <qwidget.h>
#include <qboxlayout.h>
#include <qheaderview.h>

mission_model_t::mission_model_t(std::vector<info::mission_t> const & missions, QObject * parent) :
    QAbstractItemModel(parent),
    missions_(missions)
  {
  }

auto mission_model_t::reload(std::vector<info::mission_t> const & missions) -> void
  {
  beginResetModel();
  missions_ = missions;
  endResetModel();
  }

[[nodiscard]]
auto mission_model_t::hasChildren(QModelIndex const & parent) const -> bool
  {
  if(parent.isValid())
    return false;
  return rowCount() > 0;
  }

[[nodiscard]]
auto mission_model_t::index(int row, int column, QModelIndex const & parent) const -> QModelIndex
  {
  if(!hasIndex(row, column, parent))
    return {};
  return createIndex(row, column, nullptr);
  }

[[nodiscard]]
auto mission_model_t::parent(QModelIndex const &) const -> QModelIndex
  {
  return {};
  }

[[nodiscard]]
auto mission_model_t::rowCount(QModelIndex const & parent) const -> int
  {
  if(parent.isValid() || missions_.empty())
    return 0;
  return static_cast<int>(missions_.size());
  }

[[nodiscard]]
auto mission_model_t::columnCount(QModelIndex const &) const -> int
  {
  return static_cast<int>(column_e::count_max);
  }

[[nodiscard]]
auto mission_model_t::flags(QModelIndex const & index) const -> Qt::ItemFlags
  {
  if(!index.isValid())
    return Qt::NoItemFlags;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

[[nodiscard]]
auto mission_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(!index.isValid() || index.row() >= static_cast<int>(missions_.size()))
    return {};

  auto const & mission = missions_[static_cast<std::size_t>(index.row())];

  if(role == Qt::DisplayRole)
    {
    switch(static_cast<column_e>(index.column()))
      {
      case column_e::id:          return static_cast<qlonglong>(mission.mission_id);
      case column_e::status:      return qformat("{}", mission.status);
      case column_e::type:        return QString::fromStdString(mission.type);
      case column_e::faction:     return QString::fromStdString(mission.faction);
      case column_e::reward:      return QString::fromStdString(format_credits_value(mission.reward));
      case column_e::destination: return QString::fromStdString(mission.destination_system);
      default:                    return {};
      }
    }
  return {};
  }

[[nodiscard]]
auto mission_model_t::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
  {
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
    switch(static_cast<column_e>(section))
      {
      case column_e::id:          return QStringLiteral("ID");
      case column_e::status:      return QStringLiteral("Status");
      case column_e::type:        return QStringLiteral("Type");
      case column_e::faction:     return QStringLiteral("Faction");
      case column_e::reward:      return QStringLiteral("Reward");
      case column_e::destination: return QStringLiteral("Destination");
      default:                    return {};
      }
    }
  return {};
  }

mission_window_t::mission_window_t(current_state_t const & state, QWidget * parent) :
    QMdiSubWindow(parent),
    state_(state)
  {
  setup_ui();

  // Ustawiamy tytuł okna MDI
  setWindowTitle(QStringLiteral("Missions List"));
  setAttribute(Qt::WA_DeleteOnClose);
  }

auto mission_window_t::setup_ui() -> void
  {
  // Ponieważ QMdiSubWindow to kontener, potrzebujemy centralnego widgetu
  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  model_ = new mission_model_t(state_.active_missions, this);

  view_ = new QTableView(central_widget);
  view_->setModel(model_);
  view_->setAlternatingRowColors(true);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  auto * header{view_->horizontalHeader()};
  header->setStretchLastSection(true);
  header->setSectionResizeMode(QHeaderView::Interactive);
  header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  for(int i{0}; i < model_->columnCount() - 1; ++i)
    header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(model_->columnCount() - 1, QHeaderView::Stretch);

  layout->addWidget(view_);
  layout->setContentsMargins(2, 2, 2, 2);

  setWidget(central_widget);
  }

auto mission_window_t::refresh_ui() -> void
  {
  if(model_)
    model_->reload(state_.active_missions);
  }
