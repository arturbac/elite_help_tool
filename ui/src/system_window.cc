#include <system_window.h>
#include <qabstractitemmodel.h>
#include <qboxlayout.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qformlayout.h>
#include <qtreeview.h>
#include "qformat.h"

auto system_bodies_filter_proxy_t::filterAcceptsRow(int source_row, QModelIndex const & source_parent) const -> bool
  {
  // 1. Pobierz indeks wiersza w oryginalnym modelu
  auto const idx = sourceModel()->index(source_row, 0, source_parent);
  if(!idx.isValid())
    return false;

  auto const name = sourceModel()->data(idx, Qt::DisplayRole).toString();

  if(name.contains("Belt Cluster", Qt::CaseSensitive))
    return false;

  return true;
  }

system_bodies_model_t::system_bodies_model_t(std::vector<body_t> const & bodies, QObject * parent) :
    QAbstractItemModel(parent),
    bodies_(bodies)
  {
  rebuild_index();
  }

auto system_bodies_model_t::index(int row, int column, QModelIndex const & parent) const -> QModelIndex
  {
  if(column < 0 || column >= columnCount())
    return {};

  auto const & children_list
    = parent.isValid() ? static_cast<body_info_t *>(parent.internalPointer())->children : root_nodes_;

  if(row >= 0 && row < static_cast<int>(children_list.size())) [[likely]]
    return createIndex(row, column, children_list[row]);

  return {};
  }

auto system_bodies_model_t::parent(QModelIndex const & index) const -> QModelIndex
  {
  if(!index.isValid())
    return {};

  auto const * node = static_cast<body_info_t *>(index.internalPointer());
  if(!node || !node->parent)
    return {};

  return createIndex(node->parent->row_in_parent, 0, node->parent);
  }

auto system_bodies_model_t::hasChildren(QModelIndex const & parent) const -> bool
  {
  if(!parent.isValid())
    return !root_nodes_.empty();

  auto const * node = static_cast<body_info_t *>(parent.internalPointer());
  return node && !node->children.empty();
  }

auto system_bodies_model_t::rowCount(QModelIndex const & parent) const -> int
  {
  // W modelach tabelarycznych/drzewiastych dzieci liczymy tylko dla pierwszej kolumny
  if(parent.column() > 0)
    return 0;

  // Jeśli root (parent invalid) -> zwróć liczbę gwiazd/głównych ciał
  if(!parent.isValid())
    return static_cast<int>(root_nodes_.size());

  // Jeśli element drzewa -> zwróć liczbę jego dzieci
  auto const * node = static_cast<body_info_t *>(parent.internalPointer());
  return node ? static_cast<int>(node->children.size()) : 0;
  }

auto system_bodies_model_t::columnCount(QModelIndex const &) const -> int { return 9; }

auto system_bodies_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(!index.isValid())
    return {};

  auto const * node = static_cast<body_info_t *>(index.internalPointer());
  if(not node)
    return {};

  auto const & b = *node->data;

  if(role == Qt::CheckStateRole)
    {
    if(index.column() == 8)  // Mapped
      if(b.body_type() == body_type_e::star)
        return {};
      else
        return std::get<planet_details_t>(b.details).was_mapped ? Qt::Checked : Qt::Unchecked;
    else if(index.column() == 7)  // Discovered
      return b.was_discovered ? Qt::Checked : Qt::Unchecked;
    else if(index.column() == 5)
      {
      if(b.body_type() == body_type_e::star)
        return {};
      else
        return std::get<planet_details_t>(b.details).terraform_state != events::terraform_state_e::none ? Qt::Checked
                                                                                                        : Qt::Unchecked;
      }
    else if(index.column() == 6)
      {
      if(b.body_type() == body_type_e::star)
        return {};
      else
        return std::get<planet_details_t>(b.details).mapped ? Qt::Checked : Qt::Unchecked;
      }
    return {};
    }

  if(role == Qt::ForegroundRole)
    {
    if(index.column() >= 8)
      return QColor(0xff, 0x33, 0x22);
    if(index.column() >= 5)
      return QColor(0x22, 0xAA, 0x22);

    switch(value_class(b.value))
      {
      using enum planet_value_e;
      case high:   return QColor(0x11, 0x66, 0xff);
      case medium: return QColor(0xff, 0xd7, 00);
      // case low:    return QColor(160, 160, 160);  // Szary
      default: return {};
      }
    }
  if(role == Qt::DisplayRole)
    {
    if(index.column() > 4)
      return {};

    return std::visit(
      [&b, &index]<typename T>(T const & details) -> QVariant
      {
        if constexpr(std::same_as<T, planet_details_t>)
          {
          switch(index.column())
            {
            case 0:  return QString::fromStdString(b.name);
            case 1:  return qformat("{} {}", exploration::get_planet_icon(details.planet_class), details.planet_class);
            case 2:  return QString("%1 g").arg(details.surface_gravity, 0, 'f', 2);
            case 4:  return QString::fromStdString(format_credits_value(b.value));
            case 3:  return QString("%1").arg(details.mass_em);
            default: return {};
            }
          }
        else
          {
          switch(index.column())
            {
            case 0:  return QString::fromStdString(b.name);
            case 1:  return qformat("{} {}", exploration::get_star_icon(details.star_type), details.star_type);
            case 2:  return {};
            case 3:  return QString::fromStdString(format_credits_value(b.value));
            default: return {};
            }
          }
      },
      b.details
    );
    }

  return {};
  }

auto system_bodies_model_t::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
  {
  if(orientation == Qt::Horizontal and role == Qt::DisplayRole)
    {
    static constexpr std::array const headers
      = {"Body", "Class", "Gravity", "MassEM", "Value", "Terraformable", "Mapped", "Was Discovered", "Was Mapped"};
    return headers.at(static_cast<size_t>(section));
    }
  return {};
  }

auto system_bodies_model_t::flags(QModelIndex const & index) const -> Qt::ItemFlags
  {
  auto const default_flags = QAbstractItemModel::flags(index);
  if(!index.isValid()) [[unlikely]]
    return default_flags;

  if(index.column() >= 5)
    return default_flags | Qt::ItemIsUserCheckable;

  return default_flags;
  }

auto system_bodies_model_t::rebuild_index() -> void
  {
  beginResetModel();
  nodes_.clear();
  root_nodes_.clear();

  for(auto const & b: bodies_)
    nodes_[b.body_id] = std::make_unique<body_info_t>(&b);

  for(auto const & b: bodies_)
    {
    auto * current_node = nodes_[b.body_id].get();
    if(b.body_type() == body_type_e::star or not std::get<planet_details_t>(b.details).parent_planet
       or not std::get<planet_details_t>(b.details).parent_star)
      {
      current_node->row_in_parent = static_cast<int>(root_nodes_.size());
      root_nodes_.push_back(current_node);
      }
    else
      {
      events::body_id_t parent_id;
      planet_details_t const & details{std::get<planet_details_t>(b.details)};
      if(details.parent_planet)
        parent_id = *details.parent_planet;
      else
        parent_id = *details.parent_star;
      if(auto it = nodes_.find(parent_id); it != nodes_.end())
        {
        auto * parent_node = it->second.get();
        current_node->parent = parent_node;
        current_node->row_in_parent = static_cast<int>(parent_node->children.size());
        parent_node->children.push_back(current_node);
        }
      else
        {
        current_node->row_in_parent = static_cast<int>(root_nodes_.size());
        root_nodes_.push_back(current_node);
        }
      }
    }

  endResetModel();
  }

// -----------------------------------------------------------------------------------

system_bodies_signals_model_t::system_bodies_signals_model_t(body_signals_t const & bodies, QObject * parent) :
    QAbstractItemModel(parent),
    body_signals_(bodies)
  {
  }

[[nodiscard]]
auto system_bodies_signals_model_t::pack_id(internal_id_t id) const noexcept -> quintptr
  {
  // Pakowanie 3 wartości w 64-bitowy identyfikator dla QModelIndex
  uint64_t packed = 0;
  packed |= (static_cast<uint64_t>(id.body_idx) & 0xFFFFFFFF);
  packed |= (static_cast<uint64_t>(id.type) << 32);
  packed |= (static_cast<uint64_t>(id.item_idx) << 40);
  return static_cast<quintptr>(packed);
  }

[[nodiscard]]
auto system_bodies_signals_model_t::unpack_id(quintptr id) const noexcept -> internal_id_t
  {
  uint64_t packed = static_cast<uint64_t>(id);
  return {
    .body_idx = static_cast<int32_t>(packed & 0xFFFFFFFF),
    .type = static_cast<node_type_t>((packed >> 32) & 0xFF),
    .item_idx = static_cast<int32_t>(packed >> 40)
  };
  }

[[nodiscard]]
auto system_bodies_signals_model_t::rowCount(QModelIndex const & parent) const -> int
  {
  if(parent.column() > 0) [[unlikely]]
    return 0;

  if(!parent.isValid())
    return static_cast<int>(body_signals_.size());

  auto const id = unpack_id(parent.internalId());
  auto const & body = body_signals_[id.body_idx];

  switch(id.type)
    {
    case node_type_t::body:
        {
        int count = 0;
        if(!body.signals_.empty())
          ++count;
        if(!body.genuses_.empty())
          ++count;
        return count;
        }
    case node_type_t::category_signals: return static_cast<int>(body_signals_[id.body_idx].signals_.size());
    case node_type_t::category_genuses: return static_cast<int>(body_signals_[id.body_idx].genuses_.size());
    default:                            return 0;
    }
  }

[[nodiscard]]
auto system_bodies_signals_model_t::index(int row, int column, QModelIndex const & parent) const -> QModelIndex
  {
  if(!hasIndex(row, column, parent))
    return {};
  if(!parent.isValid())
    return createIndex(row, column, pack_id({row, node_type_t::body, -1}));

  auto const id = unpack_id(parent.internalId());
  auto const & body = body_signals_[id.body_idx];

  if(id.type == node_type_t::body)
    {
    // Dynamiczne mapowanie wiersza na typ kategorii
    if(!body.signals_.empty())
      {
      if(row == 0)
        return createIndex(row, column, pack_id({id.body_idx, node_type_t::category_signals, -1}));
      // Jeśli sygnały istnieją, to row 1 musi być rodzajami (genus)
      return createIndex(row, column, pack_id({id.body_idx, node_type_t::category_genuses, -1}));
      }
    // Jeśli nie ma sygnałów, row 0 to rodzaje (genus)
    return createIndex(row, column, pack_id({id.body_idx, node_type_t::category_genuses, -1}));
    }

  // Reszta bez zmian (leaf nodes)
  auto const type = (id.type == node_type_t::category_signals) ? node_type_t::signal_item : node_type_t::genus_item;
  return createIndex(row, column, pack_id({id.body_idx, type, row}));
  }

[[nodiscard]]
auto system_bodies_signals_model_t::parent(QModelIndex const & index) const -> QModelIndex
  {
  if(!index.isValid())
    return {};
  auto const id = unpack_id(index.internalId());
  if(id.type == node_type_t::body)
    return {};

  auto const & body = body_signals_[id.body_idx];

  if(id.type == node_type_t::category_signals || id.type == node_type_t::category_genuses)
    return createIndex(id.body_idx, 0, pack_id({id.body_idx, node_type_t::body, -1}));

  // Dla elementów liści (signal_item / genus_item)
  if(id.type == node_type_t::signal_item)
    return createIndex(0, 0, pack_id({id.body_idx, node_type_t::category_signals, -1}));

  if(id.type == node_type_t::genus_item)
    {
    // Wiersz rodzica zależy od tego, czy istnieją sygnały
    int parent_row = body.signals_.empty() ? 0 : 1;
    return createIndex(parent_row, 0, pack_id({id.body_idx, node_type_t::category_genuses, -1}));
    }

  return {};
  }

[[nodiscard]]
auto system_bodies_signals_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(!index.isValid() || role != Qt::DisplayRole)
    return {};

  auto const id = unpack_id(index.internalId());
  auto const & body = body_signals_[id.body_idx];

  switch(id.type)
    {
    case node_type_t::body:             return QString::fromStdString(body.name);
    case node_type_t::category_signals: return QString("Signals (%1)").arg(body.signals_.size());
    case node_type_t::category_genuses: return QString("Genuses (%1)").arg(body.genuses_.size());
    case node_type_t::signal_item:
        {
        auto const & s = body.signals_[id.item_idx];
        return QString("%1: %2").arg(QString::fromStdString(s.Type_Localised)).arg(s.Count);
        }
    case node_type_t::genus_item: return QString::fromStdString(body.genuses_[id.item_idx].Genus_Localised);
    }
  return {};
  }

void system_bodies_signals_model_t::refresh(body_signals_t && new_data)
  {
  beginResetModel();
  body_signals_ = std::move(new_data);
  endResetModel();
  // Zawsze rozwinięte TreeView obsługuje się w widoku (QTreeView::expandAll()),
  // ale wywołanie tego po każdym restarcie modelu jest kluczowe.
  }

// Pozostałe metody standardowe
auto system_bodies_signals_model_t::hasChildren(QModelIndex const & parent) const -> bool
  {
  return rowCount(parent) > 0;
  }

auto system_bodies_signals_model_t::columnCount(QModelIndex const &) const -> int { return 1; }

auto system_bodies_signals_model_t::flags(QModelIndex const & index) const -> Qt::ItemFlags
  {
  return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
  }

auto system_bodies_signals_model_t::headerData(int, Qt::Orientation, int role) const -> QVariant
  {
  return (role == Qt::DisplayRole) ? "Body / Signals / Genus" : QVariant{};
  }

// -----------------------------------------------------------------------------------
static auto set_label_color(QLabel * label, planet_value_e val) -> void
  {
  if(!label or planet_value_e::low == val) [[unlikely]]
    return;

  std::string_view color;
  switch(val)
    {
    // case planet_value_e::low:    color = "#808080"; break;  // Szary
    case planet_value_e::medium: color = "#FFD700"; break;
    case planet_value_e::high:   color = "#1144AA"; break;
    }

  label->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color.data()));
  }

auto system_window_t::update_labels() -> void
  {
  target_label_->setText(
    QString::fromStdString(
      std::format("Next: {} [{}] {}", state_.next_target.Name, state_.next_target.StarClass, model_->bodies_.size())
    )
  );
  planet_value_e const value{exploration::system_approx_value(state_.system.star_type, state_.system.name)};
  system_label_->setText(QString::fromStdString(state_.system.name));
  set_label_color(system_label_, value);

  fss_label_->setText(state_.system.fss_complete ? "COMPLETE" : "INCOMPLETE");
  }

system_window_t::system_window_t(current_state_t const & state, QWidget * parent) : QMdiSubWindow(parent), state_(state)
  {
  setup_ui();
  connect(model_, &QAbstractItemModel::modelReset, tree_view, [tv = tree_view] { tv->expandAll(); });
  connect(proxy_model_, &QAbstractItemModel::modelReset, tree_view, [tv = tree_view] { tv->expandAll(); });

  connect(signals_model_, &QAbstractItemModel::modelReset, signals_view, [tv = signals_view] { tv->expandAll(); });
  }

// Funkcja do wywołania z main_window_t, gdy state_ zostanie zaktualizowany
auto system_window_t::refresh_ui() -> void
  {
  model_->bodies_ = state_.system.bodies;
  model_->rebuild_index();
  tree_view->expandAll();
  update_labels();
  if(model_) [[likely]]
    model_->layoutChanged();

  body_signals_t signal_data;
  for(body_t const & body: state_.system.bodies)
    std::visit(
      [&signal_data, &body]<typename T>(T const & details)
      {
        if constexpr(std::same_as<T, planet_details_t>)
          if(not details.signals_.empty() or not details.genuses_.empty())
            signal_data.emplace_back(
              body_signal_t{
                .body_id = body.body_id, .name = body.name, .signals_ = details.signals_, .genuses_ = details.genuses_
              }
            );
      },
      body.details
    );

  signals_model_->refresh(std::move(signal_data));
  }

auto system_window_t::setup_ui() -> void
  {
  auto * central_widget = new QWidget();
  auto * layout = new QVBoxLayout(central_widget);

  // Sekcja górna: Labele
  auto * info_group = new QGroupBox("Status");
  auto * form = new QFormLayout(info_group);

  target_label_ = new QLabel();
  system_label_ = new QLabel();
  fss_label_ = new QLabel();

  form->addRow("Next Target:", target_label_);
  form->addRow("Current System:", system_label_);
  form->addRow("FSS Status:", fss_label_);
  layout->addWidget(info_group);

  // Sekcja dolna: TreeView
  tree_view = new QTreeView();
  model_ = new system_bodies_model_t(state_.system.bodies, this);
  proxy_model_ = new system_bodies_filter_proxy_t(this);
  proxy_model_->setSourceModel(model_);

  tree_view->setModel(proxy_model_);

  tree_view->setAlternatingRowColors(true);
  auto * header = tree_view->header();
  header->setSectionResizeMode(QHeaderView::Interactive);
  header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  for(int i{2}; i != model_->columnCount(); ++i)
    header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(1, QHeaderView::Stretch);  // Ostatnia kolumna wypełnia okno

  layout->addWidget(new QLabel("System Bodies:"));
  layout->addWidget(tree_view);

  layout->addWidget(new QLabel("Body Signals & Genuses:"));
  signals_view = new QTreeView();
  signals_model_ = new system_bodies_signals_model_t({}, this);  // Inicjalizacja pustym wektorem
  signals_view->setModel(signals_model_);
  signals_view->setAlternatingRowColors(true);
  signals_view->header()->setSectionResizeMode(QHeaderView::Stretch);

  // Połączenie automatycznego rozwijania dla sygnałów
  connect(signals_model_, &QAbstractItemModel::modelReset, signals_view, [&] { signals_view->expandAll(); });
  layout->addWidget(signals_view);

  setWidget(central_widget);
  update_labels();  // Pierwsze wypełnienie
  setAttribute(Qt::WA_DeleteOnClose);
  }

