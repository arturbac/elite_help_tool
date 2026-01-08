#include <route_window.h>
#include <qbrush.h>
#include <qboxlayout.h>
#include <ranges>
#include <algorithm>

route_model_t::route_model_t(std::vector<info::route_item_t> const & route, QObject * parent) :
    QAbstractTableModel(parent),
    route_(route)
  {
  }

[[nodiscard]]
auto route_model_t::rowCount(QModelIndex const &) const -> int
  {
  return static_cast<int>(route_.size());
  }

[[nodiscard]]
auto route_model_t::columnCount(QModelIndex const &) const -> int
  {
  return 3;  // System, Class, Status
  }

[[nodiscard]]
auto route_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(!index.isValid() || index.row() >= static_cast<int>(route_.size()))
    return {};

  auto const & item = route_[static_cast<std::size_t>(index.row())];

  if(role == Qt::DisplayRole)
    {
    switch(index.column())
      {
      case 0: return QString::fromStdString(item.system);
      case 1:
          {
          static constexpr std::string_view kgbfoam = "KGBFOAM";
          bool is_scoopable = item.star_class.length() == 1 && kgbfoam.contains(item.star_class[0]);
          // Unicode fuel pump: \u26FD
          return is_scoopable ? QString::fromUtf8("\u26FD ") + QString::fromStdString(item.star_class)
                              : QString::fromStdString(item.star_class);
          }
      case 2: return item.visited ? "Visited" : "Pending";
      }
    }

  if(role == Qt::ForegroundRole && item.visited)
    return QBrush(Qt::gray);

  return {};
  }

auto route_model_t::update_data(std::vector<info::route_item_t> && new_route) -> void
  {
  beginResetModel();
  route_ = std::move(new_route);
  endResetModel();
  }

// Reszta metod (index, parent, hasChildren) - standardowa implementacja płaskiego modelu
[[nodiscard]]
auto route_model_t::index(int r, int c, QModelIndex const & p) const -> QModelIndex
  {
  return hasIndex(r, c, p) ? createIndex(r, c) : QModelIndex{};
  }

[[nodiscard]]
auto route_model_t::parent(QModelIndex const &) const -> QModelIndex
  {
  return {};
  }

[[nodiscard]]
auto route_model_t::hasChildren(QModelIndex const & p) const -> bool
  {
  return !p.isValid();
  }

[[nodiscard]]
auto route_model_t::flags(QModelIndex const & i) const -> Qt::ItemFlags
  {
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

[[nodiscard]]
auto route_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(s)
    {
    case 0:  return "System";
    case 1:  return "Class";
    case 2:  return "Status";
    default: return {};
    }
  }

route_window_t::route_window_t(current_state_t const & state, QWidget * parent) : QMdiSubWindow(parent), state_(state)
  {
  setup_ui();
  }

auto route_window_t::setup_ui() -> void
  {
  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  info_label_ = new QLabel(central_widget);
  table_view_ = new QTableView(central_widget);

  model_ = new route_model_t({}, this);
  table_view_->setModel(model_);

  layout->addWidget(info_label_);
  layout->addWidget(table_view_);

  setWidget(central_widget);
  refresh_ui();
  }

auto route_window_t::refresh_ui() -> void
  {
  auto current_route = state_.route_;

  auto remaining = current_route | std::views::filter([](auto const & item) { return !item.visited; });
  auto remaining_count = std::ranges::distance(remaining);

  QString next_system = "Destination Reached";
  if(remaining_count > 0)
    next_system = QString::fromStdString(remaining.front().system);

  info_label_->setText(QString("Jumps remaining: %1 | Next: %2").arg(remaining_count).arg(next_system));

  model_->update_data(std::move(current_route));
  }
