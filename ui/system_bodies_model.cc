#include "system_bodies_model.h"
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

  // 2. Pobierz nazwę (column 0, DisplayRole)
  auto const name = sourceModel()->data(idx, Qt::DisplayRole).toString();

  // 3. Logika filtrowania: odrzuć jeśli zawiera "Belt Cluster"
  if(name.contains("Belt Cluster", Qt::CaseSensitive))
    return false;

  // 4. Opcjonalnie: Jeśli chcesz, aby rodzic był widoczny tylko gdy
  // którekolwiek z jego dzieci pasuje do filtra, tutaj musiałbyś
  // dodać rekurencyjne sprawdzanie dzieci.
  // Ale dla "Belt Cluster" proste wycięcie wiersza wystarczy.
  return true;
  }

system_bodies_model_t::system_bodies_model_t(std::vector<body_t> const * bodies, QObject * parent) :
    QAbstractItemModel(parent),
    bodies_(bodies)
  {
  rebuild_index();
  }

auto system_bodies_model_t::index(int row, int column, QModelIndex const & parent) const -> QModelIndex
  {
  // 1. Podstawowa walidacja zakresu kolumn
  if(column < 0 || column >= columnCount())
    return {};

  // 2. Pobieramy listę dzieci, z której chcemy wyciągnąć wiersz
  // Jeśli parent jest inwalidą -> bierzemy root_nodes_
  // Jeśli parent jest poprawny -> bierzemy dzieci tego noda
  auto const & children_list
    = parent.isValid() ? static_cast<body_info_t *>(parent.internalPointer())->children : root_nodes_;

  // 3. Sprawdzamy czy wiersz mieści się w zakresie tej listy
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

  // Rodzic noda to body_info_t. Musimy znaleźć, który to wiersz
  // w hierarchii wyżej (u jego rodzica lub w root_nodes).
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
  if(!index.isValid() /*|| role != Qt::DisplayRole*/)
    return {};

  auto const * node = static_cast<body_info_t *>(index.internalPointer());
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
            case 1:  return QString::fromStdString(details.planet_class);
            case 2:  return QString("%1 g").arg(details.surface_gravity, 0, 'f', 2);
            case 4:  return QString("%1 Cr").arg(b.value.value);
            case 3:  return QString("%1").arg(details.mass_em);
            default: return {};
            }
          }
        else
          {
          switch(index.column())
            {
            case 0:  return QString::fromStdString(b.name);
            case 1:  return QString::fromStdString(details.star_type);
            case 2:  return {};
            case 3:  return QString("%1 Cr").arg(b.value.value);
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
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
    static constexpr std::array const headers
      = {"Body Name", "Class", "Gravity", "MassEM", "Value", "Terraformable", "Mapped", "Was Discovered", "Was Mapped"};
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

  for(auto const & b: *bodies_)
    nodes_[b.body_id] = std::make_unique<body_info_t>(&b);

  // int linked_count = 0;
  for(auto const & b: *bodies_)
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
        // linked_count++;
        }
      else
        {
        // To jest podejrzane - rodzic istnieje w danych, ale nie w mapie
        // spdlog::warn("Model: Body {} has parent_id {} but it's missing in nodes_", b.name, parent_id);
        current_node->row_in_parent = static_cast<int>(root_nodes_.size());
        root_nodes_.push_back(current_node);
        }
      }
    }

  // spdlog::info("Model rebuilt: total={}, roots={}, linked={}", raw_bodies_.size(), root_nodes_.size(), linked_count);
  endResetModel();
  }

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
      std::format("Next: {} [{}] {}", state_.next_target.Name, state_.next_target.StarClass, model_->bodies_->size())
    )
  );
  planet_value_e const value{exploration::system_approx_value(state_.system.star_type, state_.system.name)};
  system_label_->setText(QString::fromStdString(state_.system.name));
  set_label_color(system_label_, value);

  fss_label_->setText(state_.fss_complete ? "COMPLETE" : "INCOMPLETE");
  }

system_window_t::system_window_t(journal_state_t const & state, QWidget * parent) : QMdiSubWindow(parent), state_(state)
  {
  setup_ui();
  connect(model_, &QAbstractItemModel::modelReset, tree_view, [ tv = tree_view]{
    tv->expandAll();
});
  connect(proxy_model_, &QAbstractItemModel::modelReset, tree_view, [ tv = tree_view]{
    tv->expandAll();
});
  }

// Funkcja do wywołania z main_window_t, gdy state_ zostanie zaktualizowany
auto system_window_t::refresh_ui() -> void
  {
  // Używamy invokeMethod, aby zapewnić bezpieczeństwo wątkowe (z jthread)
  // update_labels();
  // if(model_) [[likely]]
  //   model_->layoutChanged();  // Informuje TreeView o nowych danych w vectorze
  model_->bodies_ = &state_.system.bodies;
  model_->rebuild_index();
  QMetaObject::invokeMethod(tree_view, "expandAll", Qt::QueuedConnection);
  QMetaObject::invokeMethod(
    this,
    [this]
    {
      update_labels();
      if(model_) [[likely]]
        model_->layoutChanged();  // Informuje TreeView o nowych danych w vectorze
    },
    Qt::QueuedConnection
  );
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
  model_ = new system_bodies_model_t(&state_.system.bodies, this);
  proxy_model_ = new system_bodies_filter_proxy_t(this);
  proxy_model_->setSourceModel(model_);

  // tree_view->setModel(model_);
  tree_view->setModel(proxy_model_);

  tree_view->setAlternatingRowColors(true);
  auto * header = tree_view->header();
  header->setSectionResizeMode(QHeaderView::Interactive);
  header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  header->setSectionResizeMode(2, QHeaderView::Stretch);  // Ostatnia kolumna wypełnia okno

  layout->addWidget(new QLabel("System Bodies:"));
  layout->addWidget(tree_view);

  setWidget(central_widget);
  update_labels();  // Pierwsze wypełnienie
  setAttribute(Qt::WA_DeleteOnClose);
  }

