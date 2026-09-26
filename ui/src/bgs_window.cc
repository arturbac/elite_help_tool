#include <bgs_window.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qsortfilterproxymodel.h>
#include <qtabwidget.h>
#include <spdlog/spdlog.h>

namespace
  {
///\brief ile dni wstecz pokazuje kazda pozycja listy okresow
constexpr std::array<uint32_t, 3> periods{7u, 14u, 30u};

///\brief ruch wplywow ponizej tego progu mowi juz tylko o zaokragleniu i o tym, co tej doby
/// zrobili inni gracze - wplyw jest suma zerowa, wiec przelicznik z takiej doby bylby zmyslony
constexpr double smallest_readable_move{0.3};
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

  // przelicznik ma sens tylko gdy praca szla w gore i wplyw naprawde sie ruszyl
  auto const rate = [&]() -> std::optional<double>
  {
    if(not row.influence_before or not row.influence_after or row.pushed_up <= 0)
      return std::nullopt;

    double const moved{*row.influence_after - *row.influence_before};
    if(moved < smallest_readable_move)
      return std::nullopt;

    return double(row.pushed_up) / moved;
  }();

  if(role == Qt::DisplayRole)
    switch(column)
      {
      case column_e::closed_by:
        // zerowy znacznik znaczy, ze fala jeszcze nie przyszla i doba trwa
        return row.closed_by == std::chrono::sys_seconds{}
                 ? QString{"trwa"}
                 : QString::fromStdString(std::format("{:%d.%m %H:%M}", row.closed_by));
      case column_e::system:      return QString::fromStdString(row.system_name);
      case column_e::population:  return QString::fromStdString(info::format_population(row.population));
      case column_e::faction:     return QString::fromStdString(row.faction);
      case column_e::missions:    return row.missions;
      case column_e::pushed_up:   return row.pushed_up != 0 ? QVariant{row.pushed_up} : QVariant{QString{"-"}};
      case column_e::pushed_down: return row.pushed_down != 0 ? QVariant{row.pushed_down} : QVariant{QString{"-"}};
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
      case column_e::closed_by:   return qlonglong(row.closed_by.time_since_epoch().count());
      case column_e::system:      return QString::fromStdString(row.system_name);
      case column_e::population:  return qulonglong(row.population);
      case column_e::faction:     return QString::fromStdString(row.faction);
      case column_e::missions:    return row.missions;
      case column_e::pushed_up:   return row.pushed_up;
      case column_e::pushed_down: return row.pushed_down;
      case column_e::influence:
        return row.influence_before and row.influence_after ? *row.influence_after - *row.influence_before : 0.0;
      case column_e::rate:        return rate ? *rate : 0.0;
      case column_e::column_max:  break;
      }

  if(role == Qt::ToolTipRole and column == column_e::rate)
    return QString{
      "Plusow na jeden punkt procentowy w TYM systemie.\n"
      "Gra dzieli wplyw misji przez wielkosc systemu, wiec tej liczby\n"
      "nie wolno porownywac miedzy systemami o roznej populacji."
    };

  if(role == Qt::TextAlignmentRole)
    switch(column)
      {
      case column_e::population:
      case column_e::missions:
      case column_e::pushed_up:
      case column_e::pushed_down:
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
    case column_e::closed_by:   return QString{"Zamknieta"};
    case column_e::system:      return QString{"System"};
    case column_e::population:  return QString{"Populacja"};
    case column_e::faction:     return QString{"Frakcja"};
    case column_e::missions:    return QString{"Misje"};
    case column_e::pushed_up:   return QString{"W gore"};
    case column_e::pushed_down: return QString{"W dol"};
    case column_e::influence:   return QString{"Wplyw przed -> po"};
    case column_e::rate:        return QString{"Plus/pp"};
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

  // pusty status znaczy, ze wojna sie zamknela - dopiero wtedy wynik jest ostateczny
  auto const outcome = [&]() -> std::string
  {
    if(not row.status.empty())
      return row.status == "pending" ? "zapowiedziana" : "trwa";
    if(row.won_days1 > row.won_days2)
      return row.faction1;
    if(row.won_days2 > row.won_days1)
      return row.faction2;
    return "remis";
  };

  if(role == Qt::DisplayRole)
    switch(column)
      {
      case column_e::system:   return QString::fromStdString(row.system_name);
      case column_e::war_type: return QString::fromStdString(row.war_type);
      case column_e::sides:    return QString::fromStdString(std::format("{} / {}", row.faction1, row.faction2));
      case column_e::score:
        return QString::fromStdString(std::format("{} : {}", row.won_days1, row.won_days2));
      case column_e::outcome: return QString::fromStdString(outcome());
      case column_e::pending_last:
        return QString::fromStdString(std::format("{:%d.%m.%Y %H:%M}", row.pending_last));
      case column_e::active_first:
        return QString::fromStdString(std::format("{:%d.%m.%Y %H:%M}", row.active_first));
      case column_e::window:     return QString::fromStdString(std::format("{:.1f}h", window_hours()));
      case column_e::column_max: break;
      }

  if(role == sort_role)
    switch(column)
      {
      case column_e::system:   return QString::fromStdString(row.system_name);
      case column_e::war_type: return QString::fromStdString(row.war_type);
      case column_e::sides:    return QString::fromStdString(std::format("{} / {}", row.faction1, row.faction2));
      case column_e::score:    return int(row.won_days1) - int(row.won_days2);
      case column_e::outcome:  return QString::fromStdString(outcome());
      case column_e::pending_last:
        return qlonglong(row.pending_last.time_since_epoch().count());
      case column_e::active_first:
        return qlonglong(row.active_first.time_since_epoch().count());
      case column_e::window:     return window_hours();
      case column_e::column_max: break;
      }

  if(role == Qt::ToolTipRole)
    return QString{
      "Wojna ruszyla gdzies w tym oknie - gra nie zapisuje tego momentu nigdzie.\n"
      "Szerokosc okna zawiera takze czas, w ktorym nie bylo nas w systemie,\n"
      "a osady wchodza w stan wojny jeszcze pozniej niz sam konflikt."
    };

  if(role == Qt::TextAlignmentRole and (column == column_e::window or column == column_e::score))
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
    case column_e::war_type:     return QString{"Typ"};
    case column_e::sides:        return QString{"Strony"};
    case column_e::score:        return QString{"Dni"};
    case column_e::outcome:      return QString{"Wygrala"};
    case column_e::pending_last: return QString{"Zapowiedziana"};
    case column_e::active_first: return QString{"Zauwazona jako trwajaca"};
    case column_e::window:       return QString{"Okno"};
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
    period_combo_->addItem(QString::fromStdString(std::format("ostatnie {} dni", days)), QVariant{days});
  period_combo_->setCurrentIndex(1);

  system_combo_ = new QComboBox(central_widget);
  controls->addWidget(new QLabel("Okres:", central_widget));
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

  // --- praca w plusach ---
  auto * effort_page = new QWidget(tabs);
  auto * effort_layout = new QVBoxLayout(effort_page);
  effort_layout->addWidget(tick_header_);

  view_ = new QTableView(effort_page);
  view_->setModel(proxy);
  view_->setSortingEnabled(true);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_->verticalHeader()->setVisible(false);
  view_->horizontalHeader()->setStretchLastSection(false);
  view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  effort_layout->addWidget(view_, 1);
  tabs->addTab(effort_page, "Praca");

  // --- kiedy wojny naprawde ruszaly ---
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
  tabs->addTab(war_page, "Poczatki wojen");

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

  // zmiana listy odpala currentIndexChanged, a przeladowanie w trakcie budowy listy nic nie wnosi
  QSignalBlocker const quiet{system_combo_};
  auto const previous{system_combo_->currentData()};

  system_combo_->clear();
  system_combo_->addItem("wszystkie", QVariant{qulonglong(0)});
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

  // kolumna "zamknieta" podaje wykryta fale, wiec obok musi stac to, jak pewna ta wiedza jest
  std::string header{"brak zaobserwowanych przeliczen"};
  if(auto stats{db_.load_tick_stats(info::tick_kind_e::influence, days)}; stats and stats->waves > 0u)
    header = std::format(
      "fal przeliczen: {} | przerwa typowo {:.1f}h, najdluzej {:.1f}h | okno pomiaru typowo {} min"
      " | najszersza propagacja po galaktyce {} min",
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

  // okres nie zaweza tej zakladki - rozrzut liczy sie tym pewniej, im wiecej wojen go zlozylo,
  // a wybrany system owszem, bo o niego zwykle chodzi przy planowaniu wyprawy
  if(auto const chosen{system_combo_->currentData().toULongLong()}; chosen != 0u)
    std::erase_if(*onsets, [chosen](info::war_onset_t const & row) { return row.system_address != chosen; });

  std::vector<double> windows;
  windows.reserve(onsets->size());
  for(info::war_onset_t const & row: *onsets)
    windows.push_back(
      double(std::chrono::duration_cast<std::chrono::minutes>(row.active_first - row.pending_last).count()) / 60.0
    );

  std::string header{"brak wojen z zapisanym przejsciem z zapowiedzi w stan wojny"};
  if(not windows.empty())
    {
    std::ranges::sort(windows);
    header = std::format(
      "{} wojen z calej zapisanej historii | okno najkrotsze {:.1f}h, mediana {:.1f}h, najdluzsze {:.1f}h"
      " | to gorne ograniczenia - zawieraja tez czas, w ktorym nas tam nie bylo,"
      " a osady wchodza w stan wojny jeszcze pozniej",
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
