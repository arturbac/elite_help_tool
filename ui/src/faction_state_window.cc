#include <eht_settings.h>
#include <faction_state_window.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qformlayout.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qbrush.h>
#include <qsortfilterproxymodel.h>
#include <qsplitter.h>
#include <qtabwidget.h>
#include <qcompleter.h>
#include <qdatetime.h>
#include <qlineedit.h>
#include <qlocale.h>
#include <qtreewidget.h>
#include <simple_enum/simple_enum.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <map>
#include <set>
#include <limits>
#include <cmath>

using namespace std::string_view_literals;

namespace
  {
constexpr std::string_view no_data{"—"};

template<typename enum_type>
[[nodiscard]]
auto enum_to_qstring(enum_type value) -> QString
  {
  auto const name{simple_enum::enum_name(value)};
  return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
  }

[[nodiscard]]
auto to_msecs(std::chrono::sys_seconds timestamp) -> qint64
  { return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count(); }
  }  // namespace

///\brief the start of this pair of factions' running conflict episode, rounded to a day
///\detail the same pair can fight more than once; an episode is counted from the row after the last
/// ending, and the 7 day clock starts only when the conflict becomes active - pending is not running yet
[[nodiscard]]
auto conflict_deadline(std::vector<info::conflict_t const *> const & rows) -> std::chrono::sys_days
  {
  std::size_t episode_start{};
  for(std::size_t ix{}; ix != rows.size(); ++ix)
    if(rows[ix]->status.empty())
      episode_start = ix + 1;

  for(std::size_t ix{episode_start}; ix != rows.size(); ++ix)
    if(rows[ix]->status == "active")
      return std::chrono::floor<std::chrono::days>(rows[ix]->timestamp);

  return std::chrono::floor<std::chrono::days>(rows[std::min(episode_start, rows.size() - 1)]->timestamp);
  }

faction_presence_model_t::faction_presence_model_t(QObject * parent) : QAbstractTableModel(parent) {}

[[nodiscard]]
auto faction_presence_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(factions_.size()); }

[[nodiscard]]
auto faction_presence_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto faction_presence_model_t::flags(QModelIndex const &) const -> Qt::ItemFlags
  { return Qt::ItemIsEnabled | Qt::ItemIsSelectable; }

[[nodiscard]]
auto faction_presence_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(factions_.size()))
    return {};

  auto const & item = factions_[static_cast<std::size_t>(index.row())];
  auto const column{column_e(index.column())};

  if(role == Qt::DisplayRole)
    switch(column)
      {
      case column_e::name:       return QString::fromStdString(item.name);
      case column_e::government: return enum_to_qstring(item.government);
      case column_e::allegiance: return enum_to_qstring(item.allegiance);
      case column_e::pending:
        return item.pending.empty() ? QString::fromUtf8(no_data.data()) : QString::fromStdString(item.pending);
      case column_e::active:
        return item.active.empty() ? QString::fromUtf8(no_data.data()) : QString::fromStdString(item.active);
      case column_e::recovering:
        return item.recovering.empty() ? QString::fromUtf8(no_data.data()) : QString::fromStdString(item.recovering);
      case column_e::influence: return qformat("{:.1f}%", item.influence * 100.);
      default:                  break;
      }

  if(role == sort_role)
    switch(column)
      {
      case column_e::name:       return QString::fromStdString(item.name);
      case column_e::government: return int(item.government);
      case column_e::allegiance: return int(item.allegiance);
      case column_e::pending:    return QString::fromStdString(item.pending);
      case column_e::active:     return QString::fromStdString(item.active);
      case column_e::recovering: return QString::fromStdString(item.recovering);
      case column_e::influence:  return item.influence;
      default:                   break;
      }

  if(role == Qt::TextAlignmentRole and column == column_e::influence)
    return int(Qt::AlignRight | Qt::AlignVCenter);

  return {};
  }

[[nodiscard]]
auto faction_presence_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::name:       return "Faction";
    case column_e::government: return "Government";
    case column_e::allegiance: return "Allegiance";
    case column_e::pending:    return "Pending";
    case column_e::active:     return "Active";
    case column_e::recovering: return "Recovering";
    case column_e::influence:  return "Inf";
    default:                   return {};
    }
  }

auto faction_presence_model_t::update_data(std::vector<faction_presence_t> && new_data) -> void
  {
  beginResetModel();
  factions_ = std::move(new_data);
  endResetModel();
  }

system_conflict_model_t::system_conflict_model_t(QObject * parent) : QAbstractTableModel(parent) {}

[[nodiscard]]
auto system_conflict_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(conflicts_.size()); }

[[nodiscard]]
auto system_conflict_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto system_conflict_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(conflicts_.size()))
    return {};

  auto const & item = conflicts_[static_cast<std::size_t>(index.row())];

  // the game stops giving a status once the conflict is over
  if(role == Qt::ForegroundRole)
    return item.status.empty() ? QVariant{QBrush(Qt::gray)} : QVariant{};

  if(role != Qt::DisplayRole)
    return {};

  switch(column_e(index.column()))
    {
    case column_e::war_type: return QString::fromStdString(item.war_type);
    case column_e::status:   return item.status.empty() ? QString{"finished"}
                                                        : QString::fromStdString(item.status);
    case column_e::faction1: return QString::fromStdString(item.faction1);
    case column_e::stake1:   return QString::fromStdString(item.stake1);
    case column_e::won1:     return item.won_days1;
    case column_e::faction2: return QString::fromStdString(item.faction2);
    case column_e::stake2:   return QString::fromStdString(item.stake2);
    case column_e::won2:     return item.won_days2;
    default:                 return {};
    }
  }

[[nodiscard]]
auto system_conflict_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::war_type: return "Type";
    case column_e::status:   return "Status";
    case column_e::faction1: return "Faction 1";
    case column_e::stake1:   return "Stake";
    case column_e::won1:     return "Won";
    case column_e::faction2: return "Faction 2";
    case column_e::stake2:   return "Stake";
    case column_e::won2:     return "Won";
    default:                 return {};
    }
  }

auto system_conflict_model_t::update_data(std::vector<info::conflict_t> && new_data) -> void
  {
  beginResetModel();
  conflicts_ = std::move(new_data);
  endResetModel();
  }

namespace
  {
///\brief the order in the station table - first where you can dock, carriers last because they fly away
[[nodiscard]]
auto station_rank(std::string_view signal_type) noexcept -> int
  {
  if(signal_type == "FleetCarrier" or signal_type == "SquadronCarrier")
    return 2;
  // the scanner reports "StationCoriolis" or "Outpost", a station from the journal its own type - both lead to a dock
  if(signal_type.starts_with("Station") or signal_type == "Outpost")
    return 0;
  for(std::string_view type:
      {"AsteroidBase"sv,
       "Bernal"sv,
       "Coriolis"sv,
       "CraterOutpost"sv,
       "CraterPort"sv,
       "Dodec"sv,
       "MegaShip"sv,
       "Ocellus"sv,
       "OnFootSettlement"sv,
       "Orbis"sv,
       "SurfaceStation"sv})
    if(signal_type == type)
      return 0;
  return 1;
  }

///\brief a construction site under the name of the port that rose on it
///\detail a scan leaves the site's signal behind long after the port began taking ships
[[nodiscard]]
auto finished_name(std::string_view name) noexcept -> std::string_view
  {
  for(std::string_view prefix: {"Planetary Construction Site: "sv, "Orbital Construction Site: "sv})
    if(name.starts_with(prefix))
      return name.substr(prefix.size());
  return name;
  }

[[nodiscard]]
auto is_installation(std::string_view signal_type) noexcept -> bool
  { return signal_type == "Installation" or signal_type.ends_with("ConstructionDepot"); }

///\brief the name as the player reads it
///\detail the colonisation ship comes out of the journal as a raw localisation token
[[nodiscard]]
auto readable_name(std::string const & name) -> QString
  {
  constexpr std::string_view colonisation_ship{"$EXT_PANEL_ColonisationShip; "};
  if(name.starts_with(colonisation_ship))
    return QString::fromStdString("Colonisation Ship " + name.substr(colonisation_ship.size()));
  return QString::fromStdString(name);
  }
  }  // namespace

system_station_model_t::system_station_model_t(QObject * parent) : QAbstractTableModel(parent) {}

[[nodiscard]]
auto system_station_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(stations_.size()); }

[[nodiscard]]
auto system_station_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto system_station_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(stations_.size()))
    return {};

  auto const & item = stations_[static_cast<std::size_t>(index.row())];
  auto const rank{station_rank(item.signal_type)};

  // a carrier seen a week ago is long gone, so it does not stand out the way a station does
  if(role == Qt::ForegroundRole)
    return rank == 2 ? QVariant{QBrush(Qt::gray)} : QVariant{};

  if(role == system_station_model_t::sort_role)
    switch(column_e(index.column()))
      {
      case column_e::signal_type: return rank * 1000 + int(column_e::signal_type);
      case column_e::name:        return readable_name(item.name);
      default:                    return {};
      }

  if(role != Qt::DisplayRole)
    return {};

  switch(column_e(index.column()))
    {
    case column_e::signal_type: return QString::fromStdString(item.signal_type);
    case column_e::name:        return readable_name(item.name);
    default:                    return {};
    }
  }

[[nodiscard]]
auto system_station_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::signal_type: return "Type";
    case column_e::name:        return "Name";
    default:                    return {};
    }
  }

auto system_station_model_t::update_data(std::vector<system_signal_t> && new_data) -> void
  {
  beginResetModel();
  stations_ = std::move(new_data);
  endResetModel();
  }

market_model_t::market_model_t(bool station_sells, QObject * parent) :
    QAbstractTableModel(parent),
    station_sells_{station_sells}
  {
  }

[[nodiscard]]
auto market_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(entries_.size()); }

[[nodiscard]]
auto market_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto market_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(entries_.size()))
    return {};

  auto const & item = entries_[static_cast<std::size_t>(index.row())];
  auto const column{column_e(index.column())};

  auto const price{station_sells_ ? item.buy_price : item.sell_price};
  auto const quantity{station_sells_ ? item.stock : item.demand};
  // the galactic average is the reference point: buying, cheaper is good; selling, dearer is
  double const deviation{
    item.mean_price != 0 ? 100. * (double(price) - double(item.mean_price)) / double(item.mean_price) : 0.
  };

  if(role == Qt::ForegroundRole and column == column_e::deviation)
    {
    bool const good{station_sells_ ? deviation < 0. : deviation > 0.};
    return QBrush(good ? QColor{0x3c, 0xb3, 0x71} : QColor{0xd9, 0x53, 0x4f});
    }

  if(role == Qt::TextAlignmentRole and column != column_e::name and column != column_e::category)
    return int(Qt::AlignRight | Qt::AlignVCenter);

  if(role == sort_role)
    switch(column)
      {
      case column_e::name:      return QString::fromStdString(item.name);
      case column_e::category:  return QString::fromStdString(item.category);
      case column_e::price:     return price;
      case column_e::quantity:  return quantity;
      case column_e::deviation: return deviation;
      default:                  return {};
      }

  if(role != Qt::DisplayRole)
    return {};

  switch(column)
    {
    case column_e::name:      return QString::fromStdString(item.name);
    case column_e::category:  return QString::fromStdString(item.category);
    case column_e::price:     return QString::fromStdString(format_credits_value(price));
    case column_e::quantity:  return QString::fromStdString(format_credits_value(quantity));
    case column_e::deviation: return qformat("{:+.0f}%", deviation);
    default:                  return {};
    }
  }

[[nodiscard]]
auto market_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::name:      return "Commodity";
    case column_e::category:  return "Category";
    case column_e::price:     return station_sells_ ? "Buy at" : "Sell at";
    case column_e::quantity:  return station_sells_ ? "Stock" : "Demand";
    case column_e::deviation: return "vs avg";
    default:                  return {};
    }
  }

auto market_model_t::update_data(std::vector<info::market_entry_t> && new_data) -> void
  {
  beginResetModel();
  entries_ = std::move(new_data);
  endResetModel();
  }

mission_stat_model_t::mission_stat_model_t(QObject * parent) : QAbstractTableModel(parent) {}

[[nodiscard]]
auto mission_stat_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(rows_.size()); }

[[nodiscard]]
auto mission_stat_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto mission_stat_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(rows_.size()))
    return {};

  auto const & item = rows_[static_cast<std::size_t>(index.row())];
  auto const column{column_e(index.column())};

  if(role == Qt::TextAlignmentRole and (column == column_e::missions or column == column_e::rewards))
    return int(Qt::AlignRight | Qt::AlignVCenter);

  if(role == sort_role)
    switch(column)
      {
      case column_e::faction:  return QString::fromStdString(item.faction);
      case column_e::missions: return item.missions;
      case column_e::rewards:  return qulonglong{item.rewards};
      case column_e::top_type: return QString::fromStdString(item.top_type);
      default:                 return {};
      }

  if(role != Qt::DisplayRole)
    return {};

  switch(column)
    {
    case column_e::faction:  return QString::fromStdString(item.faction);
    case column_e::missions: return item.missions;
    case column_e::rewards:  return QString::fromStdString(format_credits_value(uint32_t(item.rewards / 1000)));
    // the type names look like Mission_OnFoot_Salvage_MB - the same rendering as in the mission window
    case column_e::top_type: return QString::fromStdString(info::transform_mission_name(item.top_type));
    default:                 return {};
    }
  }

[[nodiscard]]
auto mission_stat_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::faction:  return "Faction";
    case column_e::missions: return "Missions";
    case column_e::rewards:  return "Rewards [kCr]";
    case column_e::top_type: return "Mostly";
    default:                 return {};
    }
  }

auto mission_stat_model_t::update_data(std::vector<info::mission_stat_t> && new_data) -> void
  {
  beginResetModel();
  rows_ = std::move(new_data);
  endResetModel();
  }

faction_state_window_t::faction_state_window_t(current_state_t const & state, std::string db_path, QWidget * parent) :
    QMdiSubWindow(parent),
    state_(state),
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("faction state window: failed to open {}", db_path);

  setup_ui();
  }

auto faction_state_window_t::setup_ui() -> void
  {
  setWindowTitle("System info");
  resize(980, 760);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  // --- choosing the system ---
  auto * selector_layout = new QHBoxLayout();
  follow_current_ = new QCheckBox("Follow current system", central_widget);
  follow_current_->setChecked(true);

  system_combo_ = new QComboBox(central_widget);
  system_combo_->setEditable(true);
  system_combo_->setInsertPolicy(QComboBox::NoInsert);
  system_combo_->completer()->setCompletionMode(QCompleter::PopupCompletion);
  system_combo_->completer()->setCaseSensitivity(Qt::CaseInsensitive);
  system_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);

  range_combo_ = new QComboBox(central_widget);
  range_combo_->addItem("Last 30 days", 30);
  range_combo_->addItem("Last 90 days", 90);
  range_combo_->addItem("All", 0);

  // a logarithmic scale shows the jumps of low influence factions; on a linear one they vanish at the bottom
  scale_combo_ = new QComboBox(central_widget);
  scale_combo_->addItem("Linear", false);
  scale_combo_->addItem("Logarithmic", true);

  selector_layout->addWidget(follow_current_);
  selector_layout->addWidget(system_combo_, 1);
  selector_layout->addWidget(new QLabel("Range:", central_widget));
  selector_layout->addWidget(range_combo_);
  selector_layout->addWidget(new QLabel("Scale:", central_widget));
  selector_layout->addWidget(scale_combo_);
  layout->addLayout(selector_layout);

  auto * tabs = new QTabWidget(central_widget);
  auto * overview = new QWidget(tabs);
  auto * overview_layout = new QVBoxLayout(overview);

  // --- the game's recalculations, above the rest because they say how long handing missions in still pays ---
  auto * tick_group = new QGroupBox("Recalculations", overview);
  auto * tick_form = new QFormLayout(tick_group);

  // Two columns side by side instead of five rows one under another - the same facts come down from half
  // the window to three lines, and every value fits without wrapping
  auto * tick_columns = new QHBoxLayout();
  auto * tick_left = new QFormLayout();
  auto * tick_right = new QFormLayout();
  tick_left->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
  tick_right->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  auto make_tick_label = [&](QFormLayout * form, char const * caption) -> std::pair<QLabel *, QLabel *>
  {
    auto * caption_label = new QLabel(caption, tick_group);
    auto * value = new QLabel(tick_group);
    form->addRow(caption_label, value);
    return {caption_label, value};
  };

  bgs_tick_label_ = make_tick_label(tick_left, "BGS here:").second;
  bgs_galaxy_label_ = make_tick_label(tick_left, "BGS galaxy:").second;
  std::tie(war_tick_row_label_, war_tick_label_) = make_tick_label(tick_right, "War here:");
  std::tie(war_galaxy_row_label_, war_galaxy_label_) = make_tick_label(tick_right, "War galaxy:");

  tick_columns->addLayout(tick_left, 1);
  tick_columns->addLayout(tick_right, 1);
  tick_form->addRow(tick_columns);

  std::tie(war_countdown_row_label_, war_countdown_label_) = make_tick_label(tick_form, "To resolution:");

  overview_layout->addWidget(tick_group);

  // --- what is known about the system ---
  auto * info_group = new QGroupBox("System", overview);
  auto * info_layout = new QHBoxLayout(info_group);
  auto * column_left = new QVBoxLayout();
  auto * column_right = new QVBoxLayout();

  // No captions - "Industrial / Agriculture" needs no word "Economy" in front of it for anyone to know
  // what it is, and eight rows of a form come down to four lines
  auto make_label = [&](QVBoxLayout * column) -> QLabel *
  {
    auto * label = new QLabel(QString::fromUtf8(no_data.data()), info_group);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    column->addWidget(label);
    return label;
  };

  economy_label_ = make_label(column_left);
  politics_label_ = make_label(column_left);
  owner_label_ = make_label(column_right);
  star_label_ = make_label(column_right);

  info_layout->addLayout(column_left, 1);
  info_layout->addLayout(column_right, 1);
  overview_layout->addWidget(info_group);

  // --- the tables and the chart in a splitter ---
  auto * splitter = new QSplitter(Qt::Vertical, overview);
  splitter->setObjectName("system_overview");

  auto * factions_container = new QWidget();
  auto * factions_layout = new QVBoxLayout(factions_container);
  factions_layout->addWidget(new QLabel("Minor factions:"));

  factions_model_ = new faction_presence_model_t(this);
  auto * factions_proxy = new QSortFilterProxyModel(this);
  factions_proxy->setSourceModel(factions_model_);
  factions_proxy->setSortRole(faction_presence_model_t::sort_role);

  factions_view_ = new QTableView();
  factions_view_->setModel(factions_proxy);
  factions_view_->setSortingEnabled(true);
  factions_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  factions_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  factions_view_->horizontalHeader()->setStretchLastSection(true);
  factions_layout->addWidget(factions_view_);
  splitter->addWidget(factions_container);

  conflicts_container_ = new QWidget();
  auto * conflicts_container = conflicts_container_;
  auto * conflicts_layout = new QVBoxLayout(conflicts_container);
  conflicts_caption_ = new QLabel("Wars and elections:");
  conflicts_layout->addWidget(conflicts_caption_);

  conflicts_model_ = new system_conflict_model_t(this);
  conflicts_view_ = new QTableView();
  conflicts_view_->setModel(conflicts_model_);
  conflicts_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  conflicts_view_->horizontalHeader()->setStretchLastSection(true);
  conflicts_view_->verticalHeader()->setVisible(false);
  conflicts_layout->addWidget(conflicts_view_);

  conflicts_note_ = new QLabel();
  conflicts_note_->setStyleSheet("color: gray; font-style: italic;");
  conflicts_layout->addWidget(conflicts_note_);
  splitter->addWidget(conflicts_container);

  chart_ = new QChart();
  chart_->setTitle("Influence");
  chart_->legend()->setAlignment(Qt::AlignBottom);
  // the chart's default theme is light, and in a dark ui it glares white
  bool const dark_ui{palette().color(QPalette::Window).lightness() < 128};
  chart_->setTheme(dark_ui ? QChart::ChartThemeDark : QChart::ChartThemeLight);
  chart_->setBackgroundRoundness(0);
  chart_->setMargins(QMargins{4, 4, 4, 4});
  chart_view_ = new QChartView(chart_);
  chart_view_->setRenderHint(QPainter::Antialiasing);
  chart_view_->setMinimumHeight(220);
  splitter->addWidget(chart_view_);

  splitter->setStretchFactor(0, 2);
  splitter->setStretchFactor(1, 1);
  splitter->setStretchFactor(2, 3);
  overview_layout->addWidget(splitter, 1);
  tabs->addTab(overview, "Overview");

  // --- the stations tab ---
  auto * stations_page = new QWidget(tabs);
  auto * stations_layout = new QVBoxLayout(stations_page);

  auto * stations_filter = new QHBoxLayout();
  hide_carriers_ = new QCheckBox("Hide carriers", stations_page);
  // there are many installations and none can be docked at, so by default they do not hide the stations
  hide_installations_ = new QCheckBox("Hide installations", stations_page);
  hide_installations_->setChecked(true);
  stations_filter->addWidget(hide_carriers_);
  stations_filter->addWidget(hide_installations_);
  stations_filter->addStretch(1);
  stations_layout->addLayout(stations_filter);

  stations_model_ = new system_station_model_t(this);
  auto * stations_proxy = new QSortFilterProxyModel(this);
  stations_proxy->setSourceModel(stations_model_);
  stations_proxy->setSortRole(system_station_model_t::sort_role);

  stations_view_ = new QTableView();
  stations_view_->setModel(stations_proxy);
  stations_view_->setSortingEnabled(true);
  stations_view_->sortByColumn(0, Qt::AscendingOrder);
  stations_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  stations_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  stations_view_->horizontalHeader()->setStretchLastSection(true);
  stations_layout->addWidget(stations_view_);
  tabs->addTab(stations_page, "Stations");

  // --- the settlements fought over in the wars under way ---
  auto * war_page = new QWidget(tabs);
  auto * war_layout = new QVBoxLayout(war_page);
  war_settlements_note_ = new QLabel(war_page);
  war_settlements_note_->setWordWrap(true);
  war_layout->addWidget(war_settlements_note_);
  war_settlements_ = new QTreeWidget(war_page);
  war_settlements_->setColumnCount(4);
  war_settlements_->setHeaderLabels({"Settlement", "Economy", "Owner before the war", "Zone intensity"});
  war_settlements_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  war_settlements_->setRootIsDecorated(true);
  war_layout->addWidget(war_settlements_, 1);
  tabs->addTab(war_page, "Settlements at war");

  // --- the tab with the clicked station's market ---
  auto * market_page = new QWidget(tabs);
  auto * market_layout = new QVBoxLayout(market_page);

  market_header_ = new QLabel(market_page);
  market_layout->addWidget(market_header_);

  auto * market_splitter = new QSplitter(Qt::Vertical, market_page);
  market_splitter->setObjectName("system_market");

  auto make_market_side = [this, market_splitter](bool station_sells, char const * caption) -> QTableView *
  {
    auto * container = new QWidget();
    auto * side_layout = new QVBoxLayout(container);
    side_layout->addWidget(new QLabel(caption));

    auto * model = new market_model_t(station_sells, this);
    auto * proxy = new QSortFilterProxyModel(this);
    proxy->setSourceModel(model);
    proxy->setSortRole(market_model_t::sort_role);

    auto * view = new QTableView();
    view->setModel(proxy);
    view->setSortingEnabled(true);
    // the best opportunities on top - buying, the cheaper against the average the better; selling, the reverse
    view->sortByColumn(4, station_sells ? Qt::AscendingOrder : Qt::DescendingOrder);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    view->horizontalHeader()->setStretchLastSection(true);
    side_layout->addWidget(view);
    market_splitter->addWidget(container);

    (station_sells ? market_sells_model_ : market_buys_model_) = model;
    return view;
  };

  market_sells_view_ = make_market_side(true, "Station sells:");
  market_buys_view_ = make_market_side(false, "Station buys:");

  market_layout->addWidget(market_splitter, 1);
  tabs->addTab(market_page, "Market");
  market_page_ = market_page;

  tabs_ = tabs;

  connect(hide_carriers_, &QCheckBox::toggled, this, [this](bool) { update_stations(shown_system_); });
  connect(hide_installations_, &QCheckBox::toggled, this, [this](bool) { update_stations(shown_system_); });

  connect(
    stations_view_,
    &QTableView::clicked,
    this,
    [this](QModelIndex const & index)
    {
      if(not index.isValid())
        return;
      show_market(index.sibling(index.row(), 1).data().toString().toStdString());
    }
  );

  // --- the missions tab ---
  auto * missions_page = new QWidget(tabs);
  auto * missions_layout = new QVBoxLayout(missions_page);

  auto * mission_filter = new QHBoxLayout();
  mission_filter->addWidget(new QLabel("Period:", missions_page));
  mission_period_combo_ = new QComboBox(missions_page);
  mission_period_combo_->addItem("This week", 7);
  mission_period_combo_->addItem("Last 4 weeks", 28);
  mission_period_combo_->addItem("Last 3 months", 90);
  mission_period_combo_->addItem("All", 0);
  mission_period_combo_->setCurrentIndex(1);
  mission_filter->addWidget(mission_period_combo_);

  missions_this_system_ = new QCheckBox("Taken in this system", missions_page);
  missions_this_system_->setChecked(true);
  mission_filter->addWidget(missions_this_system_);
  mission_filter->addStretch(1);
  missions_layout->addLayout(mission_filter);

  mission_header_ = new QLabel(missions_page);
  missions_layout->addWidget(mission_header_);

  mission_model_ = new mission_stat_model_t(this);
  auto * mission_proxy = new QSortFilterProxyModel(this);
  mission_proxy->setSourceModel(mission_model_);
  mission_proxy->setSortRole(mission_stat_model_t::sort_role);

  mission_view_ = new QTableView();
  mission_view_->setModel(mission_proxy);
  mission_view_->setSortingEnabled(true);
  mission_view_->sortByColumn(1, Qt::DescendingOrder);
  mission_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  mission_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  mission_view_->horizontalHeader()->setStretchLastSection(true);
  missions_layout->addWidget(mission_view_, 1);
  tabs->addTab(missions_page, "Missions");

  connect(mission_period_combo_, &QComboBox::activated, this, [this](int) { update_missions(); });
  connect(missions_this_system_, &QCheckBox::toggled, this, [this](bool) { update_missions(); });

  layout->addWidget(tabs, 1);

  auto const select_index = [this](int index) -> void
  {
    if(index < 0)
      return;
    // choosing a system by hand steps out of following the current one
    follow_current_->setChecked(false);
    show_system(system_combo_->itemData(index).value<qulonglong>());
  };

  connect(system_combo_, &QComboBox::activated, this, select_index);

  connect(range_combo_, &QComboBox::activated, this, [this](int) { update_chart(); });

  connect(scale_combo_, &QComboBox::activated, this, [this](int) { update_chart(); });

  // typing a name and pressing enter emits no activated, so the entry has to be found by hand
  connect(
    system_combo_->lineEdit(),
    &QLineEdit::returnPressed,
    this,
    [this, select_index]()
    { select_index(system_combo_->findText(system_combo_->currentText(), Qt::MatchFixedString)); }
  );

  connect(
    follow_current_,
    &QCheckBox::toggled,
    this,
    [this](bool on)
    {
      if(on)
        refresh_ui();
    }
  );

  setWidget(central_widget);
  reload_system_list();
  refresh_ui();
  }

auto faction_state_window_t::reload_system_list() -> void
  {
  auto res{db_.load_systems_with_influence()};
  if(not res)
    {
    spdlog::error("failed to load systems with influence history");
    return;
    }

  QSignalBlocker const block{system_combo_};
  system_combo_->clear();
  for(info::system_ref_t const & system: *res)
    system_combo_->addItem(QString::fromStdString(system.name), QVariant::fromValue(qulonglong{system.system_address}));
  }

auto faction_state_window_t::refresh_ui() -> void
  {
  if(not follow_current_->isChecked())
    return;

  auto const current{state_.system.system_address};
  if(current == 0)
    return;

  show_system(current);
  }

auto faction_state_window_t::faction_name(int64_t faction_oid) const -> std::string
  {
  auto it{std::ranges::find(state_.known_factions, faction_oid, &info::faction_info_t::oid)};
  if(it != state_.known_factions.end())
    return it->name;
  return std::format("faction {}", faction_oid);
  }

auto faction_state_window_t::show_system(uint64_t system_address) -> void
  {
  shown_system_ = system_address;

    // set the combo to the system being shown without emitting a signal
    {
    QSignalBlocker const block{system_combo_};
    auto const index{system_combo_->findData(QVariant::fromValue(qulonglong{system_address}))};
    if(index >= 0)
      system_combo_->setCurrentIndex(index);
    }

  update_system_info(system_address);
  update_war_settlements(system_address);

  auto res{db_.load_influence_history(system_address)};
  if(not res)
    {
    spdlog::error("failed to load influence history for {}", system_address);
    return;
    }

  history_ = std::move(*res);

  // a faction thrown out of the system stops appearing in the readings, but its last influence row stays
  // behind - which is why the list is narrowed to the ones seen at the newest reading
  std::set<int64_t> present;
  if(auto refs{db_.load_present_factions(system_address)}; refs)
    for(info::faction_ref_t const & ref: *refs)
      present.insert(ref.faction_oid);

  // each faction's last row is its present state in the system
  std::map<int64_t, info::faction_influence_t const *> latest;
  for(info::faction_influence_t const & entry: history_)
    {
    // an empty set means we have no trace of presence for this system yet - then everything is shown
    if(not present.empty() and not present.contains(entry.faction_oid))
      continue;
    latest[entry.faction_oid] = &entry;
    }

  std::vector<faction_presence_t> presence;
  presence.reserve(latest.size());
  for(auto const & [oid, entry]: latest)
    {
    faction_presence_t item{
      .name = faction_name(oid),
      .government = info::government_e::unknown,
      .allegiance = info::allegiance_e::unknown,
      .pending = entry->pending_states,
      // ActiveStates is the full list, FactionState only a single state
      .active = not entry->active_states.empty() ? entry->active_states
                : entry->faction_state == "None" ? std::string{}
                                                 : entry->faction_state,
      .recovering = entry->recovering_states,
      .influence = entry->influence
    };

    if(
      auto it{std::ranges::find(state_.known_factions, oid, &info::faction_info_t::oid)};
      it != state_.known_factions.end()
    )
      {
      item.government = it->government;
      item.allegiance = it->allegiance;
      }
    presence.emplace_back(std::move(item));
    }

  std::ranges::sort(
    presence, [](faction_presence_t const & l, faction_presence_t const & r) { return l.influence > r.influence; }
  );

  factions_model_->update_data(std::move(presence));
  factions_view_->resizeColumnsToContents();

  update_conflicts(system_address);
  update_stations(system_address);
  update_missions();

  update_chart();
  }

auto faction_state_window_t::update_conflicts(uint64_t system_address) -> void
  {
  conflicts_model_->update_data({});
  conflicts_note_->clear();

  auto res{db_.load_conflicts(system_address)};
  if(not res)
    {
    spdlog::error("failed to load conflicts for {}", system_address);
    return;
    }

  // we store only on a change of state, so a pair of factions has its whole course here
  std::map<std::pair<std::string, std::string>, std::vector<info::conflict_t const *>> by_pair;
  for(info::conflict_t const & conflict: *res)
    by_pair[{conflict.faction1, conflict.faction2}].push_back(&conflict);

  // a finished conflict stays in view only for a day from the moment we saw it end
  auto const keep_finished{std::chrono::hours{eht::settings()->windows.faction_keep_finished_h}};
  // a conflict lasts at most 7 days from the day it started; after that it is over
  // whether or not we managed to see it end
  auto const max_duration{std::chrono::days{eht::settings()->windows.faction_max_duration_d}};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  std::vector<info::conflict_t> current;
  std::chrono::sys_seconds newest{};
  for(auto const & [pair, rows]: by_pair)
    {
    info::conflict_t const & last{*rows.back()};

    if(last.status.empty())
      {
      if(now - last.timestamp > keep_finished)
        continue;
      }
    else if(now > conflict_deadline(rows) + max_duration)
      continue;

    current.push_back(last);
    newest = std::max(newest, last.timestamp);
    }

  // An empty table with nothing but a header took a whole splitter panel to show nothing - with no
  // conflict only the note is left and the room goes back to the faction list
  bool const anything{not current.empty()};
  conflicts_caption_->setVisible(anything);
  conflicts_view_->setVisible(anything);

  // A splitter holds the division once given, so collapsing the contents alone would leave an empty
  // panel. An upper bound on the height gives that room to the neighbour, that is to the faction list
  auto const line{conflicts_note_->sizeHint().height()};
  auto const row{conflicts_view_->verticalHeader()->defaultSectionSize()};

  if(not anything)
    {
    conflicts_container_->setMaximumHeight(line + 12);
    conflicts_note_->setText(
      res->empty() ? "No conflicts recorded in this system" : "No conflict active in this system"
    );
    return;
    }

  conflicts_note_->setText(qformat("State as of {:%Y-%m-%d %H:%M}", newest));
  auto const rows{int(current.size())};
  conflicts_model_->update_data(std::move(current));
  conflicts_view_->resizeColumnsToContents();

  // the table is given exactly as much height as it has rows - the rest of the panel it does not need
  auto const table{conflicts_view_->horizontalHeader()->height() + rows * row + 4};
  conflicts_view_->setMaximumHeight(table);
  conflicts_container_->setMaximumHeight(table + 2 * line + 16);
  }

auto faction_state_window_t::update_war_settlements(uint64_t system_address) -> void
  {
  war_settlements_->clear();
  auto wars{db_.load_war_views(system_address)};
  if(not wars)
    {
    spdlog::error("failed to load the wars of {}", system_address);
    return;
    }
  if(wars->empty())
    {
    war_settlements_note_->setText("No war under way in this system");
    return;
    }
  // confirmed in this war, a lower bound from an earlier one - the intensity never falls - or nothing
  auto const intensity = [](info::war_settlement_t const & s) -> QString
  {
    auto const name = [](info::cz_intensity_e i) -> QString
    {
      switch(i)
        {
        case info::cz_intensity_e::low:    return "Low";
        case info::cz_intensity_e::medium: return "Medium";
        case info::cz_intensity_e::high:   return "High";
        default:                           return "unknown";
        }
    };
    if(s.now != info::cz_intensity_e::unknown)
      return name(s.now);
    if(s.before != info::cz_intensity_e::unknown)
      return name(s.before) + "?";
    return "unknown";
  };
  war_settlements_note_->setText(
    "Intensity: confirmed by a kill in this war, \"?\" when seen only in an earlier one (it never falls), "
    "unknown when never fought. Only settlements visited or flown close to are known."
  );
  for(info::war_view_t const & war: *wars)
    {
    info::conflict_t const & c{war.conflict};
    auto * top = new QTreeWidgetItem(war_settlements_);
    top->setText(0, qformat("{} vs {}", c.faction1, c.faction2));
    top->setText(1, qformat("{} : {} days, {}", c.won_days1, c.won_days2, c.status));
    top->setText(
      2, qformat("stake: {} / {}", c.stake1.empty() ? "-" : c.stake1, c.stake2.empty() ? "-" : c.stake2)
    );
    top->setText(3, qformat("since {:%Y-%m-%d %H:%M}", war.started));
    for(info::war_settlement_t const & settlement: war.settlements)
      {
      auto * item = new QTreeWidgetItem(top);
      item->setText(0, QString::fromStdString(settlement.name));
      item->setText(1, QString::fromStdString(settlement.economy));
      item->setText(2, QString::fromStdString(settlement.owner_before));
      item->setText(3, intensity(settlement));
      }
    top->setExpanded(true);
    }
  }

auto faction_state_window_t::update_stations(uint64_t system_address) -> void
  {
  auto res{db_.load_system_signals(system_address)};
  if(not res)
    {
    spdlog::error("failed to load signals for {}", system_address);
    return;
    }

  auto known{db_.load_stations(system_address)};
  if(not known)
    {
    spdlog::error("failed to load stations for {}", system_address);
    return;
    }

  bool const hide_carriers{hide_carriers_->isChecked()};
  bool const hide_installations{hide_installations_->isChecked()};

  // the name of a station we ever stood at - a signal about it is by then only a repetition
  std::set<std::string, std::less<>> recorded;
  for(info::station_t const & station: *known)
    if(not station.name.empty())
      recorded.emplace(station.name);

  std::vector<system_signal_t> stations;
  auto keep = [&](system_signal_t && entry) -> void
  {
    if(hide_carriers and station_rank(entry.signal_type) == 2)
      return;
    if(hide_installations and is_installation(entry.signal_type))
      return;
    stations.emplace_back(std::move(entry));
  };

  for(system_signal_t & signal: *res)
    {
    if(classify_signal(signal.signal_type) != signal_class_e::station)
      continue;
    if(recorded.contains(finished_name(signal.name)))
      continue;
    keep(std::move(signal));
    }

  for(info::station_t & station: *known)
    {
    if(station.name.empty())
      continue;
    keep(system_signal_t{
      .system_address = station.system_address,
      .name = std::move(station.name),
      .signal_type = std::move(station.station_type),
      .is_station = true
    });
    }

  stations_model_->update_data(std::move(stations));
  stations_view_->resizeColumnsToContents();
  }

auto faction_state_window_t::show_market(std::string_view station_name) -> void
  {
  market_sells_model_->update_data({});
  market_buys_model_->update_data({});
  if(auto const index{tabs_->indexOf(market_page_)}; index >= 0)
    tabs_->setCurrentIndex(index);

  auto station{db_.load_station(shown_system_, station_name)};
  if(not station)
    {
    spdlog::error("failed to load station {}", station_name);
    return;
    }

  if(not *station)
    {
    // stations are known from a system signal, a market only from a visit with the tool running
    market_header_->setText(qformat("{} - no market recorded, dock there with the tool running", station_name));
    return;
    }

  auto entries{db_.load_market_entries((*station)->market_id)};
  if(not entries)
    {
    spdlog::error("failed to load market {}", (*station)->market_id);
    return;
    }

  if(entries->empty())
    {
    market_header_->setText(qformat("{} - no market recorded, dock there with the tool running", station_name));
    return;
    }

  // the time of the reading lives with the market, because the reading itself cannot be rebuilt
  auto reading{db_.load_market_info((*station)->market_id)};
  if(reading and *reading)
    market_header_->setText(qformat("{} - prices as of {:%Y-%m-%d %H:%M}", (*station)->name, (*reading)->updated));
  else
    market_header_->setText(QString::fromStdString((*station)->name));

  std::vector<info::market_entry_t> sells;
  std::vector<info::market_entry_t> buys;
  for(info::market_entry_t const & entry: *entries)
    {
    if(entry.stock != 0)
      sells.push_back(entry);
    if(entry.demand != 0)
      buys.push_back(entry);
    }

  market_sells_model_->update_data(std::move(sells));
  market_buys_model_->update_data(std::move(buys));
  market_sells_view_->resizeColumnsToContents();
  market_buys_view_->resizeColumnsToContents();
  }

auto faction_state_window_t::update_missions() -> void
  {
  auto const days{mission_period_combo_->currentData().toInt()};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto const since{days > 0 ? now - std::chrono::days{days} : std::chrono::sys_seconds{}};
  auto const scope{missions_this_system_->isChecked() ? shown_system_ : uint64_t{}};

  auto res{db_.load_mission_stats(since, scope)};
  if(not res)
    {
    spdlog::error("failed to load mission stats");
    return;
    }

  uint32_t missions{};
  uint64_t rewards{};
  for(info::mission_stat_t const & row: *res)
    {
    missions += row.missions;
    rewards += row.rewards;
    }

  mission_header_->setText(
    missions == 0
      ? QString{"No missions completed in this period"}
      : qformat(
          "{} missions for {} factions, {} kCr", missions, res->size(), format_credits_value(uint32_t(rewards / 1000))
        )
  );

  mission_model_->update_data(std::move(*res));
  mission_view_->resizeColumnsToContents();
  }

auto faction_state_window_t::update_tick_labels(uint64_t system_address) -> void
  {
  // the wave comes once a day, so asking more often will say nothing new
  std::chrono::seconds const tick_refresh{eht::settings()->windows.faction_tick_refresh_s};

  auto const checked{std::chrono::steady_clock::now()};
  if(ticks_system_ == system_address and checked - ticks_loaded_ < tick_refresh)
    return;

  ticks_system_ = system_address;
  ticks_loaded_ = checked;

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};

  auto const describe = [&](info::tick_kind_e kind, QLabel * here, QLabel * galaxy) -> void
  {
    tick_view_t const view{describe_tick(db_, system_address, kind, now)};

    std::string local{view.here};
    if(view.awaiting)
      // the wave has started somewhere and we have not seen it here yet - which does not mean it has not
      // been, because of a system we know only as much as we saw at the last visit
      local.append(kind == info::tick_kind_e::influence ? "  |  wave started, not seen here yet"
                                                        : "  |  wave started, bonds not recalculated yet");

    here->setText(QString::fromStdString(local));
    galaxy->setText(QString::fromStdString(view.galaxy.empty() ? std::string{"-"} : view.galaxy));
  };

  describe(info::tick_kind_e::influence, bgs_tick_label_, bgs_galaxy_label_);

  // The war clock makes sense only when there is something to fight over. What counts is the present
  // state, and load_conflicts returns the whole history - a war closed a month ago still has rows there
  // with the status "active", so asking it directly would turn these rows on forever. load_war_countdown
  // looks at the newest reading of each pair alone and leaves the closed ones out
  auto wars{db_.load_war_countdown(system_address)};
  bool const at_war{wars and not wars->empty()};

  for(QLabel * label:
      {war_tick_row_label_, war_tick_label_, war_galaxy_row_label_, war_galaxy_label_,
       war_countdown_row_label_, war_countdown_label_})
    label->setVisible(at_war);

  if(not at_war)
    return;

  describe(info::tick_kind_e::war, war_tick_label_, war_galaxy_label_);

  // how many recalculations are left before it is settled - zero means it pays to have bonds in hand
  std::string countdown;
  if(wars)
    for(info::war_countdown_t const & war: *wars)
      {
      if(not countdown.empty())
        countdown.append("\n");

      countdown.append(std::format(
        "{}{} {}:{} {} - {}",
        war.active ? "" : "(announced) ",
        war.war_type,
        war.won_days1,
        war.won_days2,
        war.faction1,
        war.ticks_left == 0u ? std::string{"DECIDED AT THE NEXT TICK - have bonds ready"}
                             : std::format("{} more war ticks", war.ticks_left)
      ));
      }

  war_countdown_label_->setText(QString::fromStdString(countdown.empty() ? std::string{"-"} : countdown));
  }

auto faction_state_window_t::update_system_info(uint64_t system_address) -> void
  {
  update_tick_labels(system_address);

  auto const empty{QString::fromUtf8(no_data.data())};

  for(QLabel * label: {economy_label_, politics_label_, owner_label_, star_label_})
    label->setText(empty);

  auto res{db_.load_system(system_address)};
  if(not res or not *res)
    return;

  star_system_t const & system{**res};
  setWindowTitle(qformat("System info - {}", system.name));

  // the game gives "None" for systems with no economy or government
  auto const meaningful = [](std::string const & value) -> std::string
  { return value == "None" ? std::string{} : value; };

  ///\brief joins the parts, skipping the empty ones, so that a missing one leaves no dangling comma
  auto const join = [](std::string_view separator, std::initializer_list<std::string> parts) -> std::string
  {
    std::string out;
    for(std::string const & part: parts)
      {
      if(part.empty())
        continue;
      if(not out.empty())
        out.append(separator);
      out.append(part);
      }
    return out;
  };

  auto const set_text = [&empty](QLabel * label, std::string const & value) -> void
  { label->setText(value.empty() ? empty : QString::fromStdString(value)); };

  // the second economy shown as inara shows it, after a slash
  set_text(economy_label_, join(" / ", {meaningful(system.economy), meaningful(system.second_economy)}));

  set_text(
    politics_label_,
    join(" - ", {meaningful(system.allegiance), join(", ", {meaningful(system.government), meaningful(system.security)})})
  );

  std::string const population{
    system.population != 0 ? QLocale{}.toString(qulonglong{system.population}).toStdString() : std::string{}
  };
  set_text(owner_label_, join("   ", {system.controlling_faction, population}));

  std::string const star{system.star_type.empty() ? std::string{} : "Star " + system.star_type};
  set_text(
    star_label_,
    join(
      "   ",
      {star,
       qformat(
         "{:.2f} / {:.2f} / {:.2f}", system.system_location[0], system.system_location[1], system.system_location[2]
       )
         .toStdString()}
    )
  );
  }

auto faction_state_window_t::update_chart() -> void
  {
  chart_->removeAllSeries();
  for(QAbstractAxis * axis: chart_->axes())
    chart_->removeAxis(axis);

  if(history_.empty())
    return;

  using days_t = std::chrono::sys_days;

  // influence changes once per tick, so of a day the last measurement is kept
  std::map<int64_t, std::map<days_t, double>> daily;
  days_t newest{};
  for(info::faction_influence_t const & entry: history_)
    {
    auto const day{std::chrono::floor<std::chrono::days>(entry.timestamp)};
    daily[entry.faction_oid][day] = entry.influence * 100.;
    newest = std::max(newest, day);
    }

  auto const range_days{range_combo_->currentData().toInt()};
  days_t const first_day{range_days > 0 ? newest - std::chrono::days{range_days - 1} : days_t{}};

  double max_influence{};
  double min_positive{std::numeric_limits<double>::max()};
  std::vector<QLineSeries *> series_list;

  for(auto const & [oid, by_day]: daily)
    {
    auto * series = new QLineSeries(chart_);
    series->setName(QString::fromStdString(faction_name(oid)));
    series->setPointsVisible(true);

    for(auto const & [day, influence]: by_day)
      {
      if(day < first_day)
        continue;
      series->append(static_cast<qreal>(to_msecs(day)), influence);
      max_influence = std::max(max_influence, influence);
      if(influence > 0.)
        min_positive = std::min(min_positive, influence);
      }

    // a faction with no measurement in the range does not reach the chart
    if(series->count() == 0)
      {
      delete series;
      continue;
      }
    series_list.push_back(series);
    chart_->addSeries(series);
    }

  if(series_list.empty())
    return;

  auto * axis_x = new QDateTimeAxis(chart_);
  axis_x->setFormat("dd.MM");
  axis_x->setTitleText("Date");
  axis_x->setTickCount(std::min(12, std::max(2, range_days > 0 ? range_days : 12)));
  // axes added by hand get no range of their own; without this the points lie outside the chart
  auto const first_shown{
    range_days > 0 ? first_day : std::chrono::floor<std::chrono::days>(history_.front().timestamp)
  };
  auto const last_shown{newest + std::chrono::days{1}};
  axis_x->setRange(
    QDateTime::fromMSecsSinceEpoch(to_msecs(first_shown)), QDateTime::fromMSecsSinceEpoch(to_msecs(last_shown))
  );
  chart_->addAxis(axis_x, Qt::AlignBottom);

  bool const log_scale{scale_combo_->currentData().toBool() and min_positive <= max_influence};

  QAbstractAxis * axis_y{};
  if(log_scale)
    {
    // whole decades give a readable grid; zero values have no representation in a logarithm
    auto const low{std::max(0.01, std::pow(10., std::floor(std::log10(min_positive))))};
    auto const high{std::max(low * 10., std::pow(10., std::ceil(std::log10(max_influence))))};

    auto * log_axis = new QLogValueAxis(chart_);
    log_axis->setBase(10.);
    log_axis->setLabelFormat("%g");
    log_axis->setTitleText("Influence [%] log");
    log_axis->setRange(low, high);
    axis_y = log_axis;
    }
  else
    {
    auto * value_axis = new QValueAxis(chart_);
    value_axis->setTitleText("Influence [%]");
    value_axis->setRange(0., std::max(10., std::ceil(max_influence / 10.) * 10.));
    axis_y = value_axis;
    }
  chart_->addAxis(axis_y, Qt::AlignLeft);

  for(QLineSeries * series: series_list)
    {
    series->attachAxis(axis_x);
    series->attachAxis(axis_y);
    }
  }
