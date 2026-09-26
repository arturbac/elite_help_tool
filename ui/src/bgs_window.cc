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

  // Koszt punktu jest wielkoscia systemu i doby, nie frakcji: procenty sumuja sie do stu, wiec
  // frakcje pchane tego samego dnia dziela miedzy siebie jeden przyrost
  auto const rate = [&]() -> std::optional<double>
  {
    if(not row.system_gain or row.system_pushed_up <= 0 or *row.system_gain < smallest_readable_move)
      return std::nullopt;

    return double(row.system_pushed_up) / *row.system_gain;
  }();

  ///\brief jaka czesc calej pracy w gore wlozonej tej doby w ten system poszla na te frakcje
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
        // zerowy znacznik znaczy, ze fala jeszcze nie przyszla i doba trwa
        return row.closed_by == std::chrono::sys_seconds{}
                 ? QString{"trwa"}
                 : QString::fromStdString(std::format("{:%d.%m %H:%M}", row.closed_by));
      case column_e::system:      return QString::fromStdString(row.system_name);
      case column_e::population:  return QString::fromStdString(info::format_population(row.population));
      case column_e::faction:       return QString::fromStdString(row.faction);
      case column_e::faction_state: return QString::fromStdString(row.faction_state);
      case column_e::missions:      return row.missions;
      case column_e::pluses:
        {
        // jedna kolumna na obie dzwignie - "w dol" bywa puste w niemal kazdym wierszu, a zabieralo
        // tyle samo szerokosci co reszta
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
        // doba jeszcze nierozliczona ma znacznik zerowy, a nalezy jej sie pierwsze miejsce
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
      "Plusow na jeden punkt procentowy w TYM systemie i TEJ dobie.\n"
      "Liczone dla calego systemu, nie dla pojedynczej frakcji: wplywy sumuja sie\n"
      "do stu procent, wiec frakcje pchane tego samego dnia dziela jeden przyrost.\n"
      "\n"
      "To srednia po tym, kogo akurat tej doby pchano - te same plusy kupuja wiecej\n"
      "punktow frakcji z dolu stawki niz liderowi systemu, wiec dzien pracy dla\n"
      "slabszej frakcji wyjdzie taniej niz ten sam wysilek wlozony w silniejsza.\n"
      "Gra dzieli tez wplyw misji przez wielkosc systemu, wiec tej liczby\n"
      "nie wolno porownywac miedzy systemami o roznej populacji."
    };

  if(role == Qt::ToolTipRole and column == column_e::share)
    return QString{
      "Czesc calej pracy w gore wlozonej tej doby w ten system,\n"
      "ktora poszla wlasnie na te frakcje.\n"
      "\n"
      "To udzial w WYSILKU, nie w przyroscie. Te same piec punktow podnosi\n"
      "frakcje lezaca na dnie znacznie mocniej niz taka z dziewiecdziesiecioma\n"
      "procentami, wiec rowny podzial pracy nie daje rownego podzialu punktow -\n"
      "kolumna obok mowi, z jakiego poziomu kazda z nich startowala."
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
    case column_e::closed_by:   return QString{"Zamknieta (UTC)"};
    case column_e::system:      return QString{"System"};
    case column_e::population:  return QString{"Populacja"};
    case column_e::faction:       return QString{"Frakcja"};
    case column_e::faction_state: return QString{"Stan"};
    case column_e::missions:    return QString{"Misje"};
    case column_e::pluses:      return QString{"Plusy"};
    case column_e::share:       return QString{"Udzial"};
    case column_e::influence:   return QString{"Wplyw"};
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
  auto const state = [&]() -> std::string
  {
    if(row.status == "pending")
      return "zapowiedziana";

    return std::format("{} : {}{}", row.won_days1, row.won_days2, row.status.empty() ? "" : " trwa");
  };

  ///\brief zielony gdy ta strona prowadzi, czerwony gdy przegrywa, bez koloru przy remisie
  ///\detail te same dwa odcienie nosi tabela reputacji - jedyne, ktore czytaja sie na ciemnym tle
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
      "Wojna ruszyla gdzies w tym oknie - gra nie zapisuje tego momentu nigdzie.\n"
      "Szerokosc okna zawiera takze czas, w ktorym nie bylo nas w systemie,\n"
      "a osady wchodza w stan wojny jeszcze pozniej niz sam konflikt."
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
    case column_e::war_type:     return QString{"Typ"};
    case column_e::window:       return QString{"Okno"};
    case column_e::faction1:     return QString{"Frakcja A"};
    case column_e::state:        return QString{"Stan"};
    case column_e::faction2:     return QString{"Frakcja B"};
    case column_e::pending_last: return QString{"Zapowiedziana (UTC)"};
    case column_e::active_first: return QString{"Zauwazona (UTC)"};
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

  // Same liczby dostaja tyle, ile potrzebuja, a nazwy oddaja lub biora reszte - przy samym
  // ResizeToContents tabela zadala wiecej szerokosci niz okno i ostatnia kolumna wypadala poza kadr
  view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  view_->horizontalHeader()->setSectionResizeMode(int(bgs_effort_model_t::stretch_column), QHeaderView::Stretch);
  view_->horizontalHeader()->setSectionResizeMode(
    int(bgs_effort_model_t::second_stretch_column), QHeaderView::Stretch
  );
  view_->setTextElideMode(Qt::ElideRight);
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
