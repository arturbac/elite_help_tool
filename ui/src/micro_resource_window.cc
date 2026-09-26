#include <micro_resource_window.h>
#include <ranges>
#include <algorithm>
#include <qformat.h>
#include <qboxlayout.h>
#include <qbrush.h>
#include <qheaderview.h>
#include <qsortfilterproxymodel.h>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace
  {
constexpr std::string_view no_data{"—"};

[[nodiscard]]
auto text_or_dash(std::string const & value) -> QVariant
  {
  return value.empty() ? QString::fromUtf8(no_data.data()) : QString::fromStdString(value);
  }

///\brief nazwa czytelna gdy ja znamy, inaczej wewnetrzna - lepsza niz pusty wiersz
[[nodiscard]]
auto display_name(std::string const & localised, std::string const & name) -> QString
  { return QString::fromStdString(localised.empty() ? name : localised); }
  }  // namespace

carrier_stock_model_t::carrier_stock_model_t(QObject * parent) : QAbstractTableModel(parent) {}

[[nodiscard]]
auto carrier_stock_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(stock_.size()); }

[[nodiscard]]
auto carrier_stock_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto carrier_stock_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(stock_.size()))
    return {};

  auto const & item = stock_[static_cast<std::size_t>(index.row())];
  auto const column{column_e(index.column())};

  if(role == Qt::TextAlignmentRole and (column == column_e::price or column == column_e::stock))
    return int(Qt::AlignRight | Qt::AlignVCenter);

  // pusta polka to pozycja ktora zeszla w calosci - warto ja widziec, a nie zgadywac
  if(role == Qt::ForegroundRole and item.stock == 0)
    return QBrush(Qt::gray);

  if(role == sort_role)
    switch(column)
      {
      case column_e::category: return text_or_dash(item.category);
      case column_e::name:     return display_name(item.localised, item.name);
      case column_e::price:    return item.price;
      case column_e::stock:    return item.stock;
      default:                 return {};
      }

  if(role != Qt::DisplayRole)
    return {};

  switch(column)
    {
    case column_e::category: return text_or_dash(item.category);
    case column_e::name:     return display_name(item.localised, item.name);
    case column_e::price:    return QString::fromStdString(format_credits_value(item.price));
    case column_e::stock:    return item.stock;
    default:                 return {};
    }
  }

[[nodiscard]]
auto carrier_stock_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::category: return "Category";
    case column_e::name:     return "Material";
    case column_e::price:    return "Price";
    case column_e::stock:    return "Stock";
    default:                 return {};
    }
  }

auto carrier_stock_model_t::update_data(std::vector<info::carrier_stock_t> && new_data) -> void
  {
  beginResetModel();
  stock_ = std::move(new_data);
  endResetModel();
  }

acquisition_model_t::acquisition_model_t(QObject * parent) : QAbstractTableModel(parent) {}

[[nodiscard]]
auto acquisition_model_t::rowCount(QModelIndex const &) const -> int
  { return static_cast<int>(rows_.size()); }

[[nodiscard]]
auto acquisition_model_t::columnCount(QModelIndex const &) const -> int
  { return int(column_e::column_max); }

[[nodiscard]]
auto acquisition_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(rows_.size()))
    return {};

  auto const & item = rows_[static_cast<std::size_t>(index.row())];
  auto const column{column_e(index.column())};
  auto const total{item.collected + item.from_missions};

  if(role == Qt::TextAlignmentRole
     and (column == column_e::collected or column == column_e::from_missions or column == column_e::total))
    return int(Qt::AlignRight | Qt::AlignVCenter);

  if(role == sort_role)
    switch(column)
      {
      case column_e::category:      return text_or_dash(item.category);
      case column_e::name:          return display_name(item.localised, item.name);
      case column_e::collected:     return item.collected;
      case column_e::from_missions: return item.from_missions;
      case column_e::total:         return total;
      case column_e::economy:       return text_or_dash(item.top_economy);
      case column_e::last_seen:     return qformat("{:%Y-%m-%d}", item.last_seen);
      default:                      return {};
      }

  if(role != Qt::DisplayRole)
    return {};

  switch(column)
    {
    case column_e::category:      return text_or_dash(item.category);
    case column_e::name:          return display_name(item.localised, item.name);
    case column_e::collected:     return item.collected;
    case column_e::from_missions: return item.from_missions;
    case column_e::total:         return total;
    case column_e::economy:       return text_or_dash(item.top_economy);
    case column_e::last_seen:     return qformat("{:%Y-%m-%d}", item.last_seen);
    default:                      return {};
    }
  }

[[nodiscard]]
auto acquisition_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::category:      return "Category";
    case column_e::name:          return "Material";
    case column_e::collected:     return "Collected";
    case column_e::from_missions: return "From missions";
    case column_e::total:         return "Total";
    case column_e::economy:       return "Mostly from";
    case column_e::last_seen:     return "Last seen";
    default:                      return {};
    }
  }

auto acquisition_model_t::update_data(std::vector<info::acquisition_summary_t> && new_data) -> void
  {
  beginResetModel();
  rows_ = std::move(new_data);
  endResetModel();
  }

micro_resource_window_t::micro_resource_window_t(std::string db_path, QWidget * parent) :
    QMdiSubWindow(parent),
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("micro resource window: failed to open {}", db_path);

  setup_ui();
  }

auto micro_resource_window_t::setup_ui() -> void
  {
  setWindowTitle("Micro resources");
  resize(900, 620);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);
  auto * tabs = new QTabWidget(central_widget);

  // --- polka flotowca ---
  auto * stock_page = new QWidget(tabs);
  auto * stock_layout = new QVBoxLayout(stock_page);

  auto * carrier_row = new QHBoxLayout();
  carrier_row->addWidget(new QLabel("Carrier:", stock_page));
  carrier_combo_ = new QComboBox(stock_page);
  carrier_row->addWidget(carrier_combo_, 1);

  mark_mine_ = new QCheckBox("mine", stock_page);
  mark_mine_->setToolTip("Mark this carrier as your own");
  only_mine_ = new QCheckBox("only mine", stock_page);
  only_mine_->setToolTip("Hide foreign carriers - their bartender shows up at every docking");
  carrier_row->addWidget(mark_mine_);
  carrier_row->addWidget(only_mine_);
  stock_layout->addLayout(carrier_row);

  stock_header_ = new QLabel(stock_page);
  stock_header_->setWordWrap(true);
  stock_layout->addWidget(stock_header_);

  stock_model_ = new carrier_stock_model_t(this);
  auto * stock_proxy = new QSortFilterProxyModel(this);
  stock_proxy->setSourceModel(stock_model_);
  stock_proxy->setSortRole(carrier_stock_model_t::sort_role);

  stock_view_ = new QTableView();
  stock_view_->setModel(stock_proxy);
  stock_view_->setSortingEnabled(true);
  stock_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  stock_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  stock_view_->horizontalHeader()->setStretchLastSection(true);
  stock_layout->addWidget(stock_view_, 1);
  tabs->addTab(stock_page, "Carrier stock");

  // --- skad to sie bierze ---
  auto * acquisition_page = new QWidget(tabs);
  auto * acquisition_layout = new QVBoxLayout(acquisition_page);

  auto * period_row = new QHBoxLayout();
  period_row->addWidget(new QLabel("Period:", acquisition_page));
  period_combo_ = new QComboBox(acquisition_page);
  period_combo_->addItem("Last 30 days", 30);
  period_combo_->addItem("Last 90 days", 90);
  period_combo_->addItem("All", 0);
  period_row->addWidget(period_combo_);
  period_row->addStretch(1);
  acquisition_layout->addLayout(period_row);

  acquisition_model_ = new acquisition_model_t(this);
  auto * acquisition_proxy = new QSortFilterProxyModel(this);
  acquisition_proxy->setSourceModel(acquisition_model_);
  acquisition_proxy->setSortRole(acquisition_model_t::sort_role);

  acquisition_view_ = new QTableView();
  acquisition_view_->setModel(acquisition_proxy);
  acquisition_view_->setSortingEnabled(true);
  acquisition_view_->sortByColumn(4, Qt::DescendingOrder);
  acquisition_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  acquisition_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
  acquisition_view_->horizontalHeader()->setStretchLastSection(true);
  acquisition_layout->addWidget(acquisition_view_, 1);
  tabs->addTab(acquisition_page, "Acquisition");

  layout->addWidget(tabs, 1);

  connect(
    carrier_combo_,
    &QComboBox::activated,
    this,
    [this](int index)
    {
      if(index < 0)
        return;

      // znacznik nalezy do wybranego flotowca, wiec idzie za wyborem
      reload_carriers();
      show_stock(carrier_combo_->itemData(index).toString().toStdString());
    }
  );

  connect(
    mark_mine_,
    &QCheckBox::toggled,
    this,
    [this](bool mine)
    {
      auto const chosen{carrier_combo_->currentData().toString().toStdString()};
      if(chosen.empty())
        return;

      if(auto res{db_.set_carrier_tracked(chosen, mine)}; not res)
        spdlog::error("failed to mark carrier {}", chosen);

      reload_carriers();
    }
  );

  connect(only_mine_, &QCheckBox::toggled, this, [this](bool) { reload_carriers(); });

  connect(period_combo_, &QComboBox::activated, this, [this](int) { show_acquisitions(); });

  setWidget(central_widget);
  refresh_ui();
  }

auto micro_resource_window_t::reload_carriers() -> void
  {
  auto res{db_.load_carriers()};
  if(not res)
    {
    spdlog::error("failed to load carriers");
    return;
    }

  // Filtr ma sens dopiero wtedy, gdy cokolwiek jest oznaczone - inaczej zostawilby pusta liste
  // i zadnej drogi powrotnej, bo oznaczyc mozna tylko flotowiec widoczny w liscie
  bool const any_mine{std::ranges::any_of(*res, [](info::carrier_t const & c) { return c.tracked; })};
  only_mine_->setEnabled(any_mine);
  if(not any_mine)
    only_mine_->setChecked(false);

  auto const previous{carrier_combo_->currentData().toString()};

  QSignalBlocker const block{carrier_combo_};
  carrier_combo_->clear();
  for(info::carrier_t const & carrier: *res)
    {
    if(only_mine_->isChecked() and not carrier.tracked)
      continue;

    carrier_combo_->addItem(
      qformat("{}{} ({})", carrier.tracked ? "* " : "", carrier.carrier_name, carrier.carrier_id),
      QString::fromStdString(carrier.carrier_id)
    );
    }

  if(auto const index{carrier_combo_->findData(previous)}; index >= 0)
    carrier_combo_->setCurrentIndex(index);

  // znacznik dotyczy tego, co akurat wybrane
  auto const chosen{carrier_combo_->currentData().toString().toStdString()};
  QSignalBlocker const quiet{mark_mine_};
  mark_mine_->setEnabled(not chosen.empty());
  mark_mine_->setChecked(std::ranges::any_of(
    *res, [&chosen](info::carrier_t const & c) { return c.carrier_id == chosen and c.tracked; }
  ));
  }

///\brief stan flotowca z ostatniego CarrierStats, gotowy do doklejenia pod naglowek
///
/// Stan i polka bartendera to dwa niezalezne zrodla, przychodzace w roznych chwilach - flotowiec
/// obcej eskadry potrafi miec stan bez ani jednego odczytu polki, wiec jedno nie moze warunkowac
/// drugiego. Stad wlasny znacznik czasu: bez niego nie wiadomo, czy paliwo jest sprzed minuty
/// czy sprzed tygodnia
auto micro_resource_window_t::carrier_stats_line(std::string_view carrier_id) -> std::string
  {
  auto carrier{db_.load_carrier(carrier_id)};
  if(not carrier or not *carrier or (*carrier)->stats_seen.time_since_epoch().count() == 0)
    return {};

  info::carrier_t const & stats{**carrier};
  return qformat(
           "\nfuel {} t | free {} of {} t | balance {} Cr (available {}) | jump {:.0f} of {:.0f} ly"
           " | {} | as of {:%Y-%m-%d %H:%M} UTC",
           stats.fuel_level,
           stats.free_space,
           stats.total_capacity,
           QLocale{}.toString(qulonglong{stats.balance}).toStdString(),
           QLocale{}.toString(qulonglong{stats.available_balance}).toStdString(),
           stats.jump_range_curr,
           stats.jump_range_max,
           stats.docking_access.empty() ? std::string{"access unknown"} : stats.docking_access,
           stats.stats_seen
  )
    .toStdString();
  }

auto micro_resource_window_t::refresh_ui() -> void
  {
  reload_carriers();

  if(carrier_combo_->count() == 0)
    {
    stock_model_->update_data({});
    // stan polki znamy tylko z wizyty u bartendera, wiec przed pierwsza nie ma czego pokazac
    stock_header_->setText("No bartender reading yet - dock at a carrier and open the bartender");
    }
  else
    show_stock(carrier_combo_->currentData().toString().toStdString());

  show_acquisitions();
  }

auto micro_resource_window_t::show_stock(std::string_view carrier_id) -> void
  {
  auto res{db_.load_carrier_stock(carrier_id)};
  if(not res)
    {
    spdlog::error("failed to load stock for {}", carrier_id);
    return;
    }

  if(res->empty())
    {
    stock_model_->update_data({});
    stock_header_->setText(
      QString::fromStdString("No bartender reading for this carrier yet" + carrier_stats_line(carrier_id))
    );
    return;
    }

  // czas odczytu jest tu najwazniejsza liczba - mowi jak stary jest ten obraz
  auto const seen{res->front().timestamp};
  uint32_t total{};
  for(info::carrier_stock_t const & item: *res)
    total += item.stock;

  stock_header_->setText(QString::fromStdString(
    qformat("{} items on the shelf, as known on {:%Y-%m-%d %H:%M} UTC", total, seen).toStdString()
    + carrier_stats_line(carrier_id)
  ));

  stock_model_->update_data(std::move(*res));
  stock_view_->resizeColumnsToContents();
  }

auto micro_resource_window_t::show_acquisitions() -> void
  {
  auto const days{period_combo_->currentData().toInt()};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto const since{days > 0 ? now - std::chrono::days{days} : std::chrono::sys_seconds{}};

  auto res{db_.load_acquisition_summary(since)};
  if(not res)
    {
    spdlog::error("failed to load acquisition summary");
    return;
    }

  acquisition_model_->update_data(std::move(*res));
  acquisition_view_->resizeColumnsToContents();
  }
