#include <eht_settings.h>
#include <bgs_window.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qsortfilterproxymodel.h>
#include <qtabwidget.h>
#include <qbrush.h>
#include <limits>
#include <spdlog/spdlog.h>

namespace
  {
///\brief how many days back each entry of the period list shows
constexpr std::array<uint32_t, 3> periods{7u, 14u, 30u};

///\brief a movement of influence below this threshold says nothing but the rounding and what other
/// players did that day - influence is zero sum, so a rate from such a day would be made up
[[nodiscard]]
auto smallest_readable_move() -> double { return eht::settings()->windows.bgs_smallest_move; }
  }  // namespace

bgs_effort_model_t::bgs_effort_model_t(QObject * parent) : QAbstractTableModel(parent) {}

auto bgs_effort_model_t::rowCount(QModelIndex const & parent) const -> int
  {
  return parent.isValid() ? 0 : int(rows_.size());
  }

auto bgs_effort_model_t::columnCount(QModelIndex const &) const -> int { return int(column_e::column_max); }

auto bgs_effort_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= int(rows_.size()))
    return {};

  info::bgs_effort_t const & row{rows_[size_t(index.row())]};
  auto const column{column_e(index.column())};

  // The cost of a point is a property of the system and the day, not of the faction: the percentages add
  // up to a hundred, so factions pushed on the same day share one gain between them
  auto const rate = [&]() -> std::optional<double>
  {
    if(not row.system_gain or row.system_pushed_up <= 0 or *row.system_gain < smallest_readable_move())
      return std::nullopt;

    return double(row.system_pushed_up) / *row.system_gain;
  }();

  ///\brief what part of all the upward work put into this system that day went to this faction
  auto const share = [&]() -> std::optional<double>
  {
    if(row.pushed_up <= 0 or row.system_pushed_up <= 0)
      return std::nullopt;

    return 100.0 * double(row.pushed_up) / double(row.system_pushed_up);
  }();

  if(role == Qt::DisplayRole)
    switch(column)
      {
      case column_e::closed_by:
        // a zero marker means the wave has not come yet and the day is still running
        return row.closed_by == std::chrono::sys_seconds{}
                 ? QString{"running"}
                 : QString::fromStdString(std::format("{:%d.%m %H:%M}", row.closed_by));
      case column_e::system:      return QString::fromStdString(row.system_name);
      case column_e::population:  return QString::fromStdString(info::format_population(row.population));
      case column_e::faction:       return QString::fromStdString(row.faction);
      case column_e::faction_state: return QString::fromStdString(row.faction_state);
      case column_e::missions:      return row.missions;
      case column_e::pluses:
        {
        // one column for both levers - "down" is empty in almost every row, yet it took as much
        // width as the rest
        if(row.pushed_up != 0 and row.pushed_down != 0)
          return QString::fromStdString(std::format("+{} -{}", row.pushed_up, row.pushed_down));
        if(row.pushed_up != 0)
          return QString::fromStdString(std::format("+{}", row.pushed_up));
        if(row.pushed_down != 0)
          return QString::fromStdString(std::format("-{}", row.pushed_down));
        return QString{"-"};
        }
      case column_e::share:
        return share ? QString::fromStdString(std::format("{:.0f}%", *share)) : QString{"-"};
      case column_e::influence:
        if(not row.influence_before or not row.influence_after)
          return QString{"-"};
        return QString::fromStdString(std::format("{:.1f} -> {:.1f}", *row.influence_before, *row.influence_after));
      case column_e::rate: return rate ? QString::fromStdString(std::format("{:.1f}", *rate)) : QString{"-"};
      case column_e::column_max: break;
      }

  if(role == sort_role)
    switch(column)
      {
      case column_e::closed_by:
        // a day not yet settled carries a zero marker, and first place is its due
        return row.closed_by == std::chrono::sys_seconds{}
                 ? std::numeric_limits<qlonglong>::max()
                 : qlonglong(row.closed_by.time_since_epoch().count());
      case column_e::system:      return QString::fromStdString(row.system_name);
      case column_e::population:  return qulonglong(row.population);
      case column_e::faction:       return QString::fromStdString(row.faction);
      case column_e::faction_state: return QString::fromStdString(row.faction_state);
      case column_e::missions:      return row.missions;
      case column_e::pluses:      return row.pushed_up + row.pushed_down;
      case column_e::share:       return share ? *share : 0.0;
      case column_e::influence:
        return row.influence_before and row.influence_after ? *row.influence_after - *row.influence_before : 0.0;
      case column_e::rate:        return rate ? *rate : 0.0;
      case column_e::column_max:  break;
      }

  if(role == Qt::ToolTipRole and column == column_e::rate)
    return QString{
      "Pluses per percentage point in THIS system and THIS day.\n"
      "Counted for the whole system, not a single faction: influence adds up\n"
      "to a hundred percent, so factions pushed the same day share one gain.\n"
      "\n"
      "It is an average over whoever was pushed that day - the same pluses buy more\n"
      "points for a faction at the bottom than for the system leader, so a day spent on\n"
      "the weaker faction comes out cheaper than the same effort put into the stronger one.\n"
      "The game also divides a mission's influence by the size of the system, so this number\n"
      "must not be compared between systems of different population."
    };

  if(role == Qt::ToolTipRole and column == column_e::share)
    return QString{
      "The share of all upward work put into this system that day\n"
      "that went to this faction.\n"
      "\n"
      "This is a share of the EFFORT, not of the gain. The same five points lift\n"
      "a faction at the bottom far more than one holding ninety\n"
      "percent, so an even split of work is not an even split of points -\n"
      "the column beside it says what level each started from."
    };

  if(role == Qt::TextAlignmentRole)
    switch(column)
      {
      case column_e::population:
      case column_e::missions:
      case column_e::pluses:
      case column_e::share:
      case column_e::rate:        return int(Qt::AlignRight | Qt::AlignVCenter);
      default:                    break;
      }

  return {};
  }

auto bgs_effort_model_t::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
  {
  if(role != Qt::DisplayRole or orientation != Qt::Horizontal)
    return {};

  switch(column_e(section))
    {
    case column_e::closed_by:   return QString{"Closed (UTC)"};
    case column_e::system:      return QString{"System"};
    case column_e::population:  return QString{"Population"};
    case column_e::faction:       return QString{"Faction"};
    case column_e::faction_state: return QString{"State"};
    case column_e::missions:    return QString{"Missions"};
    case column_e::pluses:      return QString{"Pluses"};
    case column_e::share:       return QString{"Share"};
    case column_e::influence:   return QString{"Influence"};
    case column_e::rate:        return QString{"Plus/pt"};
    case column_e::column_max:  break;
    }

  return {};
  }

auto bgs_effort_model_t::update_data(std::vector<info::bgs_effort_t> && new_data) -> void
  {
  beginResetModel();
  rows_ = std::move(new_data);
  endResetModel();
  }

war_onset_model_t::war_onset_model_t(QObject * parent) : QAbstractTableModel(parent) {}

auto war_onset_model_t::rowCount(QModelIndex const & parent) const -> int
  {
  return parent.isValid() ? 0 : int(rows_.size());
  }

auto war_onset_model_t::columnCount(QModelIndex const &) const -> int { return int(column_e::column_max); }

auto war_onset_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= int(rows_.size()))
    return {};

  info::war_onset_t const & row{rows_[size_t(index.row())]};
  auto const column{column_e(index.column())};

  auto const window_hours = [&]
  {
    return double(std::chrono::duration_cast<std::chrono::minutes>(row.active_first - row.pending_last).count())
           / 60.0;
  };

  // an empty status means the war has closed - only then is the result final
  auto const state = [&]() -> std::string
  {
    if(row.status == "pending")
      return "announced";

    return std::format("{} : {}{}", row.won_days1, row.won_days2, row.status.empty() ? "" : " running");
  };

  ///\brief green when this side leads, red when it is losing, no colour on a draw
  ///\detail the reputation table wears the same two shades - the only ones that read on a dark background
  auto const side_color = [](uint32_t mine, uint32_t theirs) -> QVariant
  {
    if(mine > theirs)
      return QBrush{QColor{0x3c, 0xb3, 0x71}};
    if(mine < theirs)
      return QBrush{QColor{0xd9, 0x53, 0x4f}};
    return {};
  };

  if(role == Qt::DisplayRole)
    switch(column)
      {
      case column_e::system:   return QString::fromStdString(row.system_name);
      case column_e::war_type: return QString::fromStdString(row.war_type);
      case column_e::window:   return QString::fromStdString(std::format("{:.1f}h", window_hours()));
      case column_e::faction1: return QString::fromStdString(row.faction1);
      case column_e::state:    return QString::fromStdString(state());
      case column_e::faction2: return QString::fromStdString(row.faction2);
      case column_e::pending_last:
        return QString::fromStdString(std::format("{:%d.%m.%Y %H:%M}", row.pending_last));
      case column_e::active_first:
        return QString::fromStdString(std::format("{:%d.%m.%Y %H:%M}", row.active_first));
      case column_e::column_max: break;
      }

  if(role == sort_role)
    switch(column)
      {
      case column_e::system:   return QString::fromStdString(row.system_name);
      case column_e::war_type: return QString::fromStdString(row.war_type);
      case column_e::window:   return window_hours();
      case column_e::faction1: return QString::fromStdString(row.faction1);
      case column_e::state:    return int(row.won_days1) - int(row.won_days2);
      case column_e::faction2: return QString::fromStdString(row.faction2);
      case column_e::pending_last:
        return qlonglong(row.pending_last.time_since_epoch().count());
      case column_e::active_first:
        return qlonglong(row.active_first.time_since_epoch().count());
      case column_e::column_max: break;
      }

  if(role == Qt::ToolTipRole)
    return QString{
      "The war started somewhere in this window - the game records that moment nowhere.\n"
      "The width also covers the time we were not in the system,\n"
      "and settlements enter the war state later than the conflict itself."
    };

  if(role == Qt::ForegroundRole and column == column_e::faction1)
    return side_color(row.won_days1, row.won_days2);

  if(role == Qt::ForegroundRole and column == column_e::faction2)
    return side_color(row.won_days2, row.won_days1);

  if(role == Qt::TextAlignmentRole and (column == column_e::window or column == column_e::state))
    return int(Qt::AlignRight | Qt::AlignVCenter);

  return {};
  }

auto war_onset_model_t::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
  {
  if(role != Qt::DisplayRole or orientation != Qt::Horizontal)
    return {};

  switch(column_e(section))
    {
    case column_e::system:       return QString{"System"};
    case column_e::war_type:     return QString{"Type"};
    case column_e::window:       return QString{"Window"};
    case column_e::faction1:     return QString{"Faction A"};
    case column_e::state:        return QString{"State"};
    case column_e::faction2:     return QString{"Faction B"};
    case column_e::pending_last: return QString{"Announced (UTC)"};
    case column_e::active_first: return QString{"Seen running (UTC)"};
    case column_e::column_max:   break;
    }

  return {};
  }

auto war_onset_model_t::update_data(std::vector<info::war_onset_t> && new_data) -> void
  {
  beginResetModel();
  rows_ = std::move(new_data);
  endResetModel();
  }

bgs_window_t::bgs_window_t(std::string db_path, QWidget * parent) : QMdiSubWindow(parent), db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("bgs window: failed to open {}", db_path);

  setup_ui();
  }

auto bgs_window_t::setup_ui() -> void
  {
  setWindowTitle("BGS");
  resize(1100, 620);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  auto * controls = new QHBoxLayout();
  period_combo_ = new QComboBox(central_widget);
  for(uint32_t const days: periods)
    period_combo_->addItem(QString::fromStdString(std::format("last {} days", days)), QVariant{days});
  period_combo_->setCurrentIndex(1);

  system_combo_ = new QComboBox(central_widget);
  controls->addWidget(new QLabel("Period:", central_widget));
  controls->addWidget(period_combo_);
  controls->addWidget(new QLabel("System:", central_widget));
  controls->addWidget(system_combo_, 1);
  layout->addLayout(controls);

  tick_header_ = new QLabel(central_widget);
  tick_header_->setWordWrap(true);

  model_ = new bgs_effort_model_t(this);
  auto * proxy = new QSortFilterProxyModel(this);
  proxy->setSourceModel(model_);
  proxy->setSortRole(bgs_effort_model_t::sort_role);

  auto * tabs = new QTabWidget(central_widget);

  // --- effort in pluses ---
  auto * effort_page = new QWidget(tabs);
  auto * effort_layout = new QVBoxLayout(effort_page);
  effort_layout->addWidget(tick_header_);

  view_ = new QTableView(effort_page);
  view_->setModel(proxy);
  view_->setSortingEnabled(true);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_->verticalHeader()->setVisible(false);
  view_->horizontalHeader()->setStretchLastSection(false);

  // The numbers alone get what they need and the names give up or take the rest - with ResizeToContents
  // on its own the table asked for more width than the window had and the last column fell out of frame
  view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  view_->horizontalHeader()->setSectionResizeMode(int(bgs_effort_model_t::stretch_column), QHeaderView::Stretch);
  view_->horizontalHeader()->setSectionResizeMode(
    int(bgs_effort_model_t::second_stretch_column), QHeaderView::Stretch
  );
  view_->setTextElideMode(Qt::ElideRight);
  effort_layout->addWidget(view_, 1);
  tabs->addTab(effort_page, "Effort");

  // --- when wars really started ---
  auto * war_page = new QWidget(tabs);
  auto * war_layout = new QVBoxLayout(war_page);

  war_header_ = new QLabel(war_page);
  war_header_->setWordWrap(true);
  war_layout->addWidget(war_header_);

  war_model_ = new war_onset_model_t(this);
  auto * war_proxy = new QSortFilterProxyModel(this);
  war_proxy->setSourceModel(war_model_);
  war_proxy->setSortRole(war_onset_model_t::sort_role);

  war_view_ = new QTableView(war_page);
  war_view_->setModel(war_proxy);
  war_view_->setSortingEnabled(true);
  war_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  war_view_->verticalHeader()->setVisible(false);
  war_view_->horizontalHeader()->setStretchLastSection(false);
  war_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  war_view_->sortByColumn(war_onset_model_t::default_sort_column, Qt::DescendingOrder);
  war_layout->addWidget(war_view_, 1);
  tabs->addTab(war_page, "War onsets");

  layout->addWidget(tabs, 1);
  setWidget(central_widget);

  connect(period_combo_, &QComboBox::currentIndexChanged, this, [this](int) { show_effort(); });
  connect(
    system_combo_,
    &QComboBox::currentIndexChanged,
    this,
    [this](int)
    {
      show_effort();
      show_wars();
    }
  );

  reload_systems();
  show_effort();
  show_wars();
  }

auto bgs_window_t::reload_systems() -> void
  {
  auto systems{db_.load_bgs_systems()};
  if(not systems)
    {
    spdlog::error("bgs window: failed to load worked systems");
    return;
    }

  // changing the list fires currentIndexChanged, and reloading while the list is being built adds nothing
  QSignalBlocker const quiet{system_combo_};
  auto const previous{system_combo_->currentData()};

  system_combo_->clear();
  system_combo_->addItem("all", QVariant{qulonglong(0)});
  for(info::system_ref_t const & system: *systems)
    system_combo_->addItem(
      QString::fromStdString(system.name.empty() ? std::format("{}", system.system_address) : system.name),
      QVariant{qulonglong(system.system_address)}
    );

  if(int const restored{system_combo_->findData(previous)}; restored >= 0)
    system_combo_->setCurrentIndex(restored);
  }

auto bgs_window_t::show_effort() -> void
  {
  auto const days{period_combo_->currentData().toUInt()};
  auto const system_address{system_combo_->currentData().toULongLong()};

  auto effort{db_.load_bgs_effort(days, uint64_t(system_address))};
  if(not effort)
    {
    spdlog::error("bgs window: failed to load effort");
    return;
    }

  model_->update_data(std::move(*effort));

  // the "closed" column gives the wave that was detected, so next to it has to stand how sure that is
  std::string header{"no recalculations observed"};
  if(auto stats{db_.load_tick_stats(info::tick_kind_e::influence, days)}; stats and stats->waves > 0u)
    header = std::format(
      "waves: {} | gap typically {:.1f}h, longest {:.1f}h | measurement window typically {} min"
      " | widest spread across the galaxy {} min",
      stats->waves,
      double(stats->typical_gap.count()) / 60.0,
      double(stats->longest_gap.count()) / 60.0,
      stats->typical_window.count(),
      stats->widest_spread.count()
    );

  tick_header_->setText(QString::fromStdString(header));
  }

auto bgs_window_t::show_wars() -> void
  {
  auto onsets{db_.load_war_onsets()};
  if(not onsets)
    {
    spdlog::error("bgs window: failed to load war onsets");
    return;
    }

  // the period does not narrow this tab - the spread counts the more surely the more wars went into it,
  // whereas the chosen system does, because it is usually what a trip is being planned around
  if(auto const chosen{system_combo_->currentData().toULongLong()}; chosen != 0u)
    std::erase_if(*onsets, [chosen](info::war_onset_t const & row) { return row.system_address != chosen; });

  std::vector<double> windows;
  windows.reserve(onsets->size());
  for(info::war_onset_t const & row: *onsets)
    windows.push_back(
      double(std::chrono::duration_cast<std::chrono::minutes>(row.active_first - row.pending_last).count()) / 60.0
    );

  std::string header{"no wars with a recorded transition from announced to running"};
  if(not windows.empty())
    {
    std::ranges::sort(windows);
    header = std::format(
      "{} wars across the whole recorded history | window shortest {:.1f}h, median {:.1f}h, longest {:.1f}h"
      " | these are upper bounds - they also cover the time we were not there,"
      " and settlements enter the war state later still",
      windows.size(),
      windows.front(),
      windows[windows.size() / 2u],
      windows.back()
    );
    }

  war_header_->setText(QString::fromStdString(header));
  war_model_->update_data(std::move(*onsets));
  }

auto bgs_window_t::refresh_ui() -> void
  {
  reload_systems();
  show_effort();
  show_wars();
  }
