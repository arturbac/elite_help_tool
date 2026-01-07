#include <mission_window.h>
#include <qformat.h>
#include <simple_enum/std_format.hpp>
#include <qwidget.h>
#include <qboxlayout.h>
#include <qheaderview.h>
#include <stralgo/stralgo.h>
#include <unordered_map>

mission_model_t::mission_model_t(std::vector<info::mission_t> const & missions, QObject * parent) :
    QAbstractItemModel(parent),
    missions_(missions)
  {
  }

auto mission_model_t::reload(std::vector<info::mission_t> const & missions) -> void
  {
  beginResetModel();
  missions_ = missions;
  std::ranges::sort(
    missions_,
    [](info::mission_t const & l, info::mission_t const & r) -> bool
    {
      if(l.destination_system != r.destination_system)
        return l.destination_system < r.destination_system;
      if(l.faction != r.faction)
        return l.faction < r.faction;
      return l.expiry < r.expiry;
    }
  );
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
      case column_e::id:      return static_cast<qlonglong>(mission.mission_id);
      case column_e::status:  return qformat("{}", mission.status);
      case column_e::type:    return QString::fromStdString(mission.type);
      case column_e::faction: return QString::fromStdString(mission.faction);
      case column_e::count:   return qlonglong(mission.mission_count());
      case column_e::reward:  return QString::fromStdString(format_credits_value(mission.reward));
      case column_e::destination:
        if(mission.status == info::mission_status_e::redirected)
          return QString::fromStdString(mission.redirected_system);
        else
          return QString::fromStdString(mission.destination_system);
      default: return {};
      }
    }
  else if(role == Qt::ForegroundRole)
    {
    if(column_e::status == static_cast<column_e>(index.column()))
      {
      if(mission.status == info::mission_status_e::redirected)
        return QColor(0x2f, 0xd7, 00);
      else
        return QColor(0x11, 0x36, 0xff);
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
      case column_e::count:       return QStringLiteral("Count");
      case column_e::reward:      return QStringLiteral("Reward");
      case column_e::destination: return QStringLiteral("Destination");
      default:                    return {};
      }
    }
  return {};
  }

mission_massacre_stack_model_t::mission_massacre_stack_model_t(
  std::vector<massacre_stack_mission_t> && missions, QObject * parent
) :
    QAbstractItemModel(parent),
    missions_(std::move(missions))
  {
  }

auto mission_massacre_stack_model_t::reload(std::vector<massacre_stack_mission_t> && missions) -> void
  {
  beginResetModel();
  missions_ = std::move(missions);
  std::ranges::sort(
    missions_,
    [](massacre_stack_mission_t const & l, massacre_stack_mission_t const & r) -> bool
    {
      if(l.destination_system != r.destination_system)
        return l.destination_system < r.destination_system;
      return l.faction < r.faction;
    }
  );
  endResetModel();
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::hasChildren(QModelIndex const & parent) const -> bool
  {
  if(parent.isValid())
    return false;
  return rowCount() > 0;
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::index(int row, int column, QModelIndex const & parent) const -> QModelIndex
  {
  if(!hasIndex(row, column, parent))
    return {};
  return createIndex(row, column, nullptr);
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::parent(QModelIndex const &) const -> QModelIndex
  {
  return {};
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::rowCount(QModelIndex const & parent) const -> int
  {
  if(parent.isValid() || missions_.empty())
    return 0;
  return static_cast<int>(missions_.size());
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::columnCount(QModelIndex const &) const -> int
  {
  return static_cast<int>(column_e::column_max);
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::flags(QModelIndex const & index) const -> Qt::ItemFlags
  {
  if(!index.isValid())
    return Qt::NoItemFlags;
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(!index.isValid() || index.row() >= static_cast<int>(missions_.size()))
    return {};

  auto const & mission = missions_[static_cast<std::size_t>(index.row())];

  if(role == Qt::DisplayRole)
    {
    switch(static_cast<column_e>(index.column()))
      {
      case column_e::destination:   return QString::fromStdString(mission.destination_system);
      case column_e::faction:       return QString::fromStdString(mission.faction);
      case column_e::count_pending: return qlonglong(mission.count_pending);
      case column_e::count_done:    return qlonglong(mission.count_done);
      default:                      return {};
      }
    }
  return {};
  }

[[nodiscard]]
auto mission_massacre_stack_model_t::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
  {
  if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
    {
    switch(static_cast<column_e>(section))
      {
      case column_e::destination:   return QStringLiteral("System");
      case column_e::faction:       return QStringLiteral("Faction");
      case column_e::count_pending: return QStringLiteral("Pending");
      case column_e::count_done:    return QStringLiteral("Done");
      default:                      return {};
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
  setWindowTitle(QStringLiteral("Active Missions List"));
  setAttribute(Qt::WA_DeleteOnClose);
  }

struct target_mm_t
  {
  std::string destination_system;
  std::string faction;

  [[nodiscard]]
  constexpr auto operator==(target_mm_t const &) const noexcept -> bool
    = default;
  };

template<>
struct std::hash<target_mm_t>
  {
  [[nodiscard]]
  auto operator()(target_mm_t const & s) const noexcept -> std::size_t
    {
    auto const h1 = std::hash<std::string>{}(s.destination_system);
    auto const h2 = std::hash<std::string>{}(s.faction);

    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

struct target_mm_counts_t
  {
  uint32_t count_pending{};
  uint32_t count_done{};
  };

static auto aggregate_missions(std::vector<info::mission_t> const & m) -> std::vector<massacre_stack_mission_t>
  {
  using namespace std::string_view_literals;
  std::vector<massacre_stack_mission_t> result;
  auto filtered{std::views::filter(
    m, [](info::mission_t const & obj) -> bool { return stralgo::starts_with(obj.type, "Mission_Massacre"sv); }
  )};
  std::unordered_map<target_mm_t, target_mm_counts_t> aggregate;

  for(auto const & mission: filtered)
    {
    target_mm_t tgt{.destination_system = mission.destination_system, .faction = mission.faction};

    auto & counts{aggregate[tgt]};
    if(mission.status == info::mission_status_e::redirected)
      counts.count_done += mission.kill_count;
    else
      counts.count_pending += mission.kill_count;
    }
  std::ranges::transform(
    aggregate,
    std::back_inserter(result),
    [](auto & pr) -> massacre_stack_mission_t
    {
      return massacre_stack_mission_t{
        .destination_system = std::move(pr.first.destination_system),
        .faction = std::move(pr.first.faction),
        .count_pending = pr.second.count_pending,
        .count_done = pr.second.count_done
      };
    }
  );
  return result;
  }

auto mission_window_t::setup_ui() -> void
  {
  auto * central_widget = new QWidget(this);
  auto * main_layout = new QVBoxLayout(central_widget);
  auto * tabs = new QTabWidget(central_widget);

  // --- Tab 1: Active Missions ---
  auto * mission_page = new QWidget(tabs);
  auto * mission_layout = new QVBoxLayout(mission_page);
  model_ = new mission_model_t(state_.active_missions, this);

  view_ = new QTableView(central_widget);
  view_->setModel(model_);
  view_->setAlternatingRowColors(true);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    {
    auto * header{view_->horizontalHeader()};
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for(int i{0}; i < model_->columnCount() - 1; ++i)
      header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(model_->columnCount() - 1, QHeaderView::Stretch);
    }
  mission_layout->addWidget(view_);
  tabs->addTab(mission_page, QStringLiteral("Missions"));

  // --- Tab 2: Massacre Stacks ---
  auto * massacre_page = new QWidget(tabs);
  auto * massacre_layout = new QVBoxLayout(massacre_page);

  // Placeholder: You'll need to pass the appropriate data structure here
  // e.g., &state_.massacre_data
  massacre_stack_model_ = new mission_massacre_stack_model_t(aggregate_missions(state_.active_missions), this);

  massace_view_ = new QTableView(massacre_page);
  massace_view_->setModel(massacre_stack_model_);
  massace_view_->setAlternatingRowColors(true);
  massace_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    {
    auto * header{massace_view_->horizontalHeader()};
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for(int i{0}; i < model_->columnCount(); ++i)
      header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    }
  massacre_layout->addWidget(massace_view_);
  tabs->addTab(massacre_page, QStringLiteral("Massacre Stacking"));

  // Finalize Layout
  main_layout->addWidget(tabs);
  main_layout->setContentsMargins(2, 2, 2, 2);
  setWidget(central_widget);
  }

auto mission_window_t::refresh_ui() -> void
  {
  if(model_)
    model_->reload(state_.active_missions);

  if(massacre_stack_model_)
    massacre_stack_model_->reload(aggregate_missions(state_.active_missions));
  }
