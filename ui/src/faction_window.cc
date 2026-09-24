#include <faction_window.h>
#include <qformat.h>
#include <qbrush.h>
#include <qcolor.h>
#include <qboxlayout.h>
#include <qheaderview.h>
#include <simple_enum/simple_enum.hpp>
#include <algorithm>
#include <optional>

namespace
  {
// enum_name zwraca string_view bez gwarancji zakończenia zerem
template<typename enum_type>
[[nodiscard]]
auto enum_to_qstring(enum_type value) -> QString
  {
  auto const name{simple_enum::enum_name(value)};
  return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
  }

// barwy supermocarstw, tony środkowe - czytelne i na jasnym, i na ciemnym motywie
[[nodiscard]]
auto allegiance_color(info::allegiance_e allegiance) -> std::optional<QColor>
  {
  using enum info::allegiance_e;
  switch(allegiance)
    {
    case federation: return QColor{0xd9, 0x53, 0x4f};  // czerwony
    case empire:     return QColor{0x4a, 0x90, 0xd9};  // niebieski
    case alliance:   return QColor{0x3c, 0xb3, 0x71};  // zielony
    default:         return {};
    }
  }
  }  // namespace

faction_model_t::faction_model_t(std::vector<info::faction_info_t> factions, QObject * parent) :
    QAbstractTableModel(parent),
    factions_(std::move(factions))
  {
  }

[[nodiscard]]
auto faction_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(factions_.size()); }

[[nodiscard]]
auto faction_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto faction_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(factions_.size()))
    return {};

  auto const & item = factions_[static_cast<std::size_t>(index.row())];
  auto const column{column_e(index.column())};

  if(role == Qt::DisplayRole)
    switch(column)
      {
      case column_e::name:       return QString::fromStdString(item.name);
      case column_e::reputation: return qformat("{:.1f}%", item.reputation);
      case column_e::influence:  return qformat("{:.1f}%", item.influence * 100.);
      case column_e::allegiance: return enum_to_qstring(item.allegiance);
      case column_e::government: return enum_to_qstring(item.government);
      default:                   break;
      }

  // sortowanie po wartościach surowych, nie po sformatowanym tekście
  if(role == sort_role)
    switch(column)
      {
      case column_e::name:       return QString::fromStdString(item.name);
      case column_e::reputation: return item.reputation;
      case column_e::influence:  return item.influence;
      case column_e::allegiance: return int(item.allegiance);
      case column_e::government: return int(item.government);
      default:                   break;
      }

  if(role == Qt::TextAlignmentRole and (column == column_e::reputation or column == column_e::influence))
    return int(Qt::AlignRight | Qt::AlignVCenter);

  if(role == Qt::ForegroundRole and column == column_e::reputation)
    {
    if(item.reputation < 0.)
      return QBrush(Qt::red);
    if(item.reputation >= 90.)
      return QBrush(Qt::darkGreen);
    }

  if(role == Qt::ForegroundRole and column == column_e::name)
    if(auto const color{allegiance_color(item.allegiance)}; color)
      return QBrush(*color);

  return {};
  }

auto faction_model_t::update_data(std::vector<info::faction_info_t> && new_factions) -> void
  {
  beginResetModel();
  factions_ = std::move(new_factions);
  endResetModel();
  }

// Reszta metod (index, parent, hasChildren) - standardowa implementacja płaskiego modelu
[[nodiscard]]
auto faction_model_t::index(int r, int c, QModelIndex const & p) const -> QModelIndex
  { return hasIndex(r, c, p) ? createIndex(r, c) : QModelIndex{}; }

[[nodiscard]]
auto faction_model_t::parent(QModelIndex const &) const -> QModelIndex
  { return {}; }

[[nodiscard]]
auto faction_model_t::hasChildren(QModelIndex const & p) const -> bool
  { return !p.isValid(); }

[[nodiscard]]
auto faction_model_t::flags(QModelIndex const &) const -> Qt::ItemFlags
  { return Qt::ItemIsEnabled | Qt::ItemIsSelectable; }

[[nodiscard]]
auto faction_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::name:       return "Faction";
    case column_e::reputation: return "Reputation";
    case column_e::allegiance: return "Allegiance";
    case column_e::influence:  return "Influence";
    case column_e::government: return "Government";
    default:                   return {};
    }
  }

faction_window_t::faction_window_t(current_state_t & state, QWidget * parent) : QMdiSubWindow(parent), state_(state)
  { setup_ui(); }

auto faction_window_t::setup_ui() -> void
  {
  setWindowTitle("Factions");

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  auto * filter_layout = new QHBoxLayout();
  search_edit_ = new QLineEdit(central_widget);
  search_edit_->setPlaceholderText("Search faction...");
  search_edit_->setClearButtonEnabled(true);

  current_system_only_ = new QCheckBox("Current system only", central_widget);

  filter_layout->addWidget(search_edit_);
  filter_layout->addWidget(current_system_only_);

  info_label_ = new QLabel(central_widget);
  table_view_ = new QTableView(central_widget);

  model_ = new faction_model_t({}, this);
  proxy_ = new QSortFilterProxyModel(this);
  proxy_->setSourceModel(model_);
  proxy_->setSortRole(faction_model_t::sort_role);
  proxy_->setFilterKeyColumn(0);  // nazwa frakcji
  proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);
  table_view_->setModel(proxy_);

  table_view_->setSortingEnabled(true);
  table_view_->sortByColumn(0, Qt::AscendingOrder);
  table_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    {
    auto * header{table_view_->horizontalHeader()};
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    for(int i{0}; i < model_->columnCount() - 1; ++i)
      header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(model_->columnCount() - 1, QHeaderView::Stretch);
    }

  layout->addLayout(filter_layout);
  layout->addWidget(info_label_);
  layout->addWidget(table_view_);

  connect(
    search_edit_,
    &QLineEdit::textChanged,
    this,
    [this](QString const & text)
    {
      proxy_->setFilterFixedString(text);
      refresh_ui();
    }
  );

  connect(current_system_only_, &QCheckBox::toggled, this, [this](bool) { refresh_ui(); });

  setWidget(central_widget);
  refresh_ui();
  }

[[nodiscard]]
auto faction_window_t::collect_factions() const -> std::vector<info::faction_info_t>
  {
  if(current_system_only_->isChecked())
    return state_.system_factions;

  return state_.known_factions;
  }

auto faction_window_t::refresh_ui() -> void
  {
  model_->update_data(collect_factions());

  auto const shown{proxy_->rowCount()};
  auto const total{model_->rowCount()};
  if(shown == total)
    info_label_->setText(qformat("Factions: {}", total));
  else
    info_label_->setText(qformat("Factions: {} of {}", shown, total));
  }
