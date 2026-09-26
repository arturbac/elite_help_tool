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

///\brief poczatek trwajacego epizodu konfliktu tej pary frakcji, zaokraglony do doby
///\detail ta sama para moze walczyc wielokrotnie, epizod liczy sie od wpisu po ostatnim zakonczeniu,
/// a zegar 7 dni rusza dopiero gdy konflikt staje sie aktywny - pending jeszcze nie trwa
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

  // gra przestaje podawac status gdy konflikt sie skonczyl
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
///\brief kolejnosc w tabeli stacji - najpierw to gdzie zadokujesz, flotowce na koncu bo odlatuja
[[nodiscard]]
auto station_rank(std::string_view signal_type) noexcept -> int
  {
  if(signal_type == "FleetCarrier" or signal_type == "SquadronCarrier")
    return 2;
  // skaner zglasza "StationCoriolis" czy "Outpost", a stacja z journala wlasny typ - obie drogi wiodą do doku
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

///\brief plac budowy pod nazwa portu, ktory na nim stanal
///\detail skan zostawia sygnal placu jeszcze dlugo po tym, jak port zaczal przyjmowac statki
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

///\brief nazwa tak, jak czyta ja gracz
///\detail statek kolonizacyjny przychodzi z journala jako surowy token lokalizacji
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

  // flotowiec sprzed tygodnia dawno odleciał, wiec nie rzuca sie w oczy jak stacja
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
  // srednia galaktyczna jest punktem odniesienia, przy kupnie taniej jest dobrze, przy sprzedazy drozej
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
    // nazwy typu wygladaja jak Mission_OnFoot_Salvage_MB, ten sam przeklad co w oknie misji
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

  // --- wybor systemu ---
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

  // skala logarytmiczna pokazuje skoki frakcji o niskim influence, na liniowej gina przy dnie
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

  // --- przeliczenia gry, nad reszta bo wyznaczaja, do kiedy warto jeszcze oddawac misje ---
  auto * tick_group = new QGroupBox("Przeliczenia", overview);
  auto * tick_form = new QFormLayout(tick_group);

  // Dwie kolumny obok siebie zamiast piatki wierszy jedna pod druga - te same fakty schodza
  // z polowy okna do trzech linijek, a kazda wartosc miesci sie bez zawijania
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

  bgs_tick_label_ = make_tick_label(tick_left, "BGS tutaj:").second;
  bgs_galaxy_label_ = make_tick_label(tick_left, "BGS galaktyka:").second;
  std::tie(war_tick_row_label_, war_tick_label_) = make_tick_label(tick_right, "War tutaj:");
  std::tie(war_galaxy_row_label_, war_galaxy_label_) = make_tick_label(tick_right, "War galaktyka:");

  tick_columns->addLayout(tick_left, 1);
  tick_columns->addLayout(tick_right, 1);
  tick_form->addRow(tick_columns);

  std::tie(war_countdown_row_label_, war_countdown_label_) = make_tick_label(tick_form, "Do rozstrzygniecia:");

  overview_layout->addWidget(tick_group);

  // --- informacje o systemie ---
  auto * info_group = new QGroupBox("System", overview);
  auto * info_layout = new QHBoxLayout(info_group);
  auto * column_left = new QVBoxLayout();
  auto * column_right = new QVBoxLayout();

  // Bez podpisow - "Industrial / Agriculture" nie potrzebuje slowa "Economy" przed soba, zeby bylo
  // wiadomo czym jest, a osiem wierszy formularza schodzi do czterech linijek
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

  // --- tabele i wykres w splitterze ---
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
  // domyslny motyw wykresu jest jasny, w ciemnym ui swieci bielą
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

  // --- zakladka ze stacjami ---
  auto * stations_page = new QWidget(tabs);
  auto * stations_layout = new QVBoxLayout(stations_page);

  auto * stations_filter = new QHBoxLayout();
  hide_carriers_ = new QCheckBox("Hide carriers", stations_page);
  // instalacji jest duzo i nie da sie w nich zadokowac, wiec domyslnie nie zaslaniaja stacji
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

  // --- zakladka z rynkiem klikietej stacji ---
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
    // najlepsze okazje na gorze - kupno im tansze wzgledem sredniej tym lepiej, sprzedaz odwrotnie
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

  // --- zakladka z misjami ---
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
    // reczny wybor systemu wychodzi ze sledzenia biezacego
    follow_current_->setChecked(false);
    show_system(system_combo_->itemData(index).value<qulonglong>());
  };

  connect(system_combo_, &QComboBox::activated, this, select_index);

  connect(range_combo_, &QComboBox::activated, this, [this](int) { update_chart(); });

  connect(scale_combo_, &QComboBox::activated, this, [this](int) { update_chart(); });

  // wpisanie nazwy i enter nie emituje activated, trzeba samemu odnalezc pozycje
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

    // ustawiamy combo na pokazywany system bez wywolywania sygnalu
    {
    QSignalBlocker const block{system_combo_};
    auto const index{system_combo_->findData(QVariant::fromValue(qulonglong{system_address}))};
    if(index >= 0)
      system_combo_->setCurrentIndex(index);
    }

  update_system_info(system_address);

  auto res{db_.load_influence_history(system_address)};
  if(not res)
    {
    spdlog::error("failed to load influence history for {}", system_address);
    return;
    }

  history_ = std::move(*res);

  // frakcja, ktora wyleciala z systemu, przestaje pojawiac sie w odczytach, ale jej ostatni wpis
  // influence zostaje - dlatego liste zawezamy do tych widzianych przy najswiezszym odczycie
  std::set<int64_t> present;
  if(auto refs{db_.load_present_factions(system_address)}; refs)
    for(info::faction_ref_t const & ref: *refs)
      present.insert(ref.faction_oid);

  // ostatni wpis kazdej frakcji to jej obecny stan w systemie
  std::map<int64_t, info::faction_influence_t const *> latest;
  for(info::faction_influence_t const & entry: history_)
    {
    // pusty zbior znaczy ze dla tego systemu nie mamy jeszcze sladu obecnosci - wtedy pokazujemy wszystko
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
      // ActiveStates jest pelna lista, FactionState tylko jednym stanem
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

  // zapisujemy dopiero przy zmianie stanu, wiec para frakcji ma tu caly swoj przebieg
  std::map<std::pair<std::string, std::string>, std::vector<info::conflict_t const *>> by_pair;
  for(info::conflict_t const & conflict: *res)
    by_pair[{conflict.faction1, conflict.faction2}].push_back(&conflict);

  // zakonczony konflikt zostaje w widoku tylko dobe od chwili gdy zobaczylismy jego koniec
  constexpr auto keep_finished{std::chrono::hours{24}};
  // konflikt trwa najwyzej 7 dni od dnia rozpoczecia, po tym czasie jest po nim
  // niezaleznie od tego czy zdazylismy zobaczyc jego koniec
  constexpr auto max_duration{std::chrono::days{7}};
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

  // Pusta tabela z samym naglowkiem zabierala caly panel splittera, zeby nie pokazac niczego -
  // przy braku konfliktu zostaje sama notka, a miejsce wraca do listy frakcji
  bool const anything{not current.empty()};
  conflicts_caption_->setVisible(anything);
  conflicts_view_->setVisible(anything);

  // Splitter trzyma raz nadany podzial, wiec samo zwiniecie zawartosci zostawiloby pusty panel.
  // Gorna granica wysokosci oddaje to miejsce sasiadowi, czyli liscie frakcji
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

  // tabela dostaje dokladnie tyle wysokosci, ile ma wierszy - reszta panelu nie jest jej potrzebna
  auto const table{conflicts_view_->horizontalHeader()->height() + rows * row + 4};
  conflicts_view_->setMaximumHeight(table);
  conflicts_container_->setMaximumHeight(table + 2 * line + 16);
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

  // nazwa stacji, w ktorej kiedykolwiek stanelismy - sygnal o niej jest juz tylko powtorzeniem
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
    // stacje znamy z sygnalu systemu, rynek tylko z wizyty przy wlaczonej aplikacji
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

  // czas odczytu mieszka przy rynku, bo sam odczyt jest nieodtwarzalny
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
  // fala przychodzi raz na dobe, wiec czestsze pytanie niczego nowego nie powie
  constexpr std::chrono::seconds tick_refresh{60};

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
      // fala juz gdzies ruszyla, a tutaj jej jeszcze nie widzielismy - to nie znaczy, ze nie byla,
      // bo o systemie wiemy tylko tyle, ile zobaczylismy przy ostatniej wizycie
      local.append(kind == info::tick_kind_e::influence ? "  |  fala ruszyla, tu jeszcze nie widziana"
                                                        : "  |  fala ruszyla, bondy jeszcze nie przeliczone");

    here->setText(QString::fromStdString(local));
    galaxy->setText(QString::fromStdString(view.galaxy.empty() ? std::string{"-"} : view.galaxy));
  };

  describe(info::tick_kind_e::influence, bgs_tick_label_, bgs_galaxy_label_);

  // Zegar wojen ma sens tylko gdy jest o co walczyc. Liczy sie stan biezacy, a load_conflicts
  // oddaje cala historie - wojna zamknieta miesiac temu ma tam nadal wiersze ze statusem "active",
  // wiec pytanie o nia wprost wlaczaloby te wiersze na zawsze. load_war_countdown patrzy wylacznie
  // na najswiezszy odczyt kazdej pary i pomija zamkniete
  auto wars{db_.load_war_countdown(system_address)};
  bool const at_war{wars and not wars->empty()};

  for(QLabel * label:
      {war_tick_row_label_, war_tick_label_, war_galaxy_row_label_, war_galaxy_label_,
       war_countdown_row_label_, war_countdown_label_})
    label->setVisible(at_war);

  if(not at_war)
    return;

  describe(info::tick_kind_e::war, war_tick_label_, war_galaxy_label_);

  // ile jeszcze przeliczen do rozstrzygniecia - zero znaczy, ze warto miec bondy na reku
  std::string countdown;
  if(wars)
    for(info::war_countdown_t const & war: *wars)
      {
      if(not countdown.empty())
        countdown.append("\n");

      countdown.append(std::format(
        "{}{} {}:{} {} - {}",
        war.active ? "" : "(zapowiedziany) ",
        war.war_type,
        war.won_days1,
        war.won_days2,
        war.faction1,
        war.ticks_left == 0u ? std::string{"ROZSTRZYGA SIE NAJBLIZSZYM TICKIEM - bondy na reke"}
                             : std::format("jeszcze {} tickow wojny", war.ticks_left)
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

  // gra podaje "None" dla systemow bez ekonomii czy rzadu
  auto const meaningful = [](std::string const & value) -> std::string
  { return value == "None" ? std::string{} : value; };

  ///\brief sklada czesci pomijajac puste, zeby brak jednej nie zostawil wiszacego przecinka
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

  // druga ekonomia pokazywana jak na inarze, po ukosniku
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

  // influence zmienia sie raz na tick, wiec z doby zostaje ostatni pomiar
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

    // frakcja bez pomiaru w zakresie nie trafia na wykres
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
  // osie dodane recznie nie dostaja zakresu same, bez tego punkty leza poza wykresem
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
    // pelne dekady daja czytelna siatke, wartosci zerowe nie maja reprezentacji w logarytmie
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
