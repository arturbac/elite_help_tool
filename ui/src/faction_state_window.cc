#include <faction_state_window.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qformlayout.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qsortfilterproxymodel.h>
#include <qsplitter.h>
#include <qcompleter.h>
#include <qdatetime.h>
#include <qlineedit.h>
#include <simple_enum/simple_enum.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <map>
#include <limits>
#include <cmath>

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
  if(role != Qt::DisplayRole or not index.isValid() or index.row() >= static_cast<int>(conflicts_.size()))
    return {};

  auto const & item = conflicts_[static_cast<std::size_t>(index.row())];
  switch(column_e(index.column()))
    {
    case column_e::war_type: return QString::fromStdString(item.war_type);
    case column_e::status:   return QString::fromStdString(item.status);
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

auto system_conflict_model_t::update_data(std::vector<system_conflict_t> && new_data) -> void
  {
  beginResetModel();
  conflicts_ = std::move(new_data);
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
  setWindowTitle("System factions");
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

  // --- informacje o systemie ---
  auto * info_group = new QGroupBox("System", central_widget);
  auto * info_layout = new QHBoxLayout(info_group);
  auto * form_left = new QFormLayout();
  auto * form_right = new QFormLayout();

  auto make_label = [&](QFormLayout * form, char const * caption) -> QLabel *
  {
    auto * label = new QLabel(QString::fromUtf8(no_data.data()), info_group);
    form->addRow(caption, label);
    return label;
  };

  economy_label_ = make_label(form_left, "Economy:");
  government_label_ = make_label(form_left, "Government:");
  allegiance_label_ = make_label(form_left, "Allegiance:");
  security_label_ = make_label(form_left, "Security:");
  population_label_ = make_label(form_right, "Population:");
  controlling_label_ = make_label(form_right, "Controlling faction:");
  star_type_label_ = make_label(form_right, "Star type:");
  coordinates_label_ = make_label(form_right, "Coordinates:");

  info_layout->addLayout(form_left, 1);
  info_layout->addLayout(form_right, 1);
  layout->addWidget(info_group);

  // --- tabele i wykres w splitterze ---
  auto * splitter = new QSplitter(Qt::Vertical, central_widget);

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

  auto * conflicts_container = new QWidget();
  auto * conflicts_layout = new QVBoxLayout(conflicts_container);
  conflicts_layout->addWidget(new QLabel("Wars and elections:"));

  conflicts_model_ = new system_conflict_model_t(this);
  conflicts_view_ = new QTableView();
  conflicts_view_->setModel(conflicts_model_);
  conflicts_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  conflicts_view_->horizontalHeader()->setStretchLastSection(true);
  conflicts_layout->addWidget(conflicts_view_);

  conflicts_note_ = new QLabel("Conflicts from the journal are not parsed yet");
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
  layout->addWidget(splitter, 1);

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

  // ostatni wpis kazdej frakcji to jej obecny stan w systemie
  std::map<int64_t, info::faction_influence_t const *> latest;
  for(info::faction_influence_t const & entry: history_)
    latest[entry.faction_oid] = &entry;

  std::vector<faction_presence_t> presence;
  presence.reserve(latest.size());
  for(auto const & [oid, entry]: latest)
    {
    faction_presence_t item{
      .name = faction_name(oid),
      .government = info::government_e::unknown,
      .allegiance = info::allegiance_e::unknown,
      .pending = {},  // PendingStates jeszcze nie parsowane
      .active = entry->faction_state == "None" ? std::string{} : entry->faction_state,
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

  // Conflicts z journala nie sa jeszcze parsowane
  conflicts_model_->update_data({});

  update_chart();
  }

auto faction_state_window_t::update_system_info(uint64_t system_address) -> void
  {
  auto const empty{QString::fromUtf8(no_data.data())};

  // te pola sa w evencie Location/FSDJump, ale nie sa jeszcze zapisywane w bazie
  economy_label_->setText(empty);
  government_label_->setText(empty);
  allegiance_label_->setText(empty);
  security_label_->setText(empty);
  population_label_->setText(empty);
  controlling_label_->setText(empty);

  star_type_label_->setText(empty);
  coordinates_label_->setText(empty);

  auto res{db_.load_system(system_address)};
  if(not res or not *res)
    return;

  star_system_t const & system{**res};
  setWindowTitle(qformat("System factions - {}", system.name));

  if(not system.star_type.empty())
    star_type_label_->setText(QString::fromStdString(system.star_type));

  coordinates_label_->setText(
    qformat("{:.2f} / {:.2f} / {:.2f}", system.system_location[0], system.system_location[1], system.system_location[2])
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
