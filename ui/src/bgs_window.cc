#include <bgs_window.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qgroupbox.h>
#include <qheaderview.h>
#include <qsortfilterproxymodel.h>
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
      case column_e::population:  return QLocale{}.toString(qulonglong(row.population));
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
  layout->addWidget(tick_header_);

  model_ = new bgs_effort_model_t(this);
  auto * proxy = new QSortFilterProxyModel(this);
  proxy->setSourceModel(model_);
  proxy->setSortRole(bgs_effort_model_t::sort_role);

  view_ = new QTableView(central_widget);
  view_->setModel(proxy);
  view_->setSortingEnabled(true);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_->verticalHeader()->setVisible(false);
  view_->horizontalHeader()->setStretchLastSection(false);
  view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  layout->addWidget(view_, 1);

  setWidget(central_widget);

  connect(period_combo_, &QComboBox::currentIndexChanged, this, [this](int) { show_effort(); });
  connect(system_combo_, &QComboBox::currentIndexChanged, this, [this](int) { show_effort(); });

  reload_systems();
  show_effort();
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

auto bgs_window_t::refresh_ui() -> void
  {
  reload_systems();
  show_effort();
  }
