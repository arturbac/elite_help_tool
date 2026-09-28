#include <micro_resource_window.h>
#include <ranges>
#include <algorithm>
#include <qformat.h>
#include <qboxlayout.h>
#include <qbrush.h>
#include <qheaderview.h>
#include <qsortfilterproxymodel.h>
#include <qtablewidget.h>
#include <qtimer.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qabstractitemview.h>
#include <commodity_facts.h>
#include <qspinbox.h>
#include <qpushbutton.h>
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

///\brief the readable name when we know it, otherwise the internal one - better than an empty row
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

  // an empty shelf is an item that sold out entirely - worth seeing rather than guessing at
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

  // --- the carrier's shelf ---
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

  // --- where it all comes from ---
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

  auto const period_choice = [](QWidget * parent) -> QComboBox *
  {
    auto * combo = new QComboBox(parent);
    combo->addItem("Last 30 days", 30);
    combo->addItem("Last 90 days", 90);
    combo->addItem("All", 0);
    return combo;
  };
  auto const numbers_table = [](QWidget * parent, QStringList const & headers) -> QTableWidget *
  {
    auto * table = new QTableWidget(parent);
    table->setColumnCount(int(headers.size()));
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    return table;
  };

  // --- what went through the counters ---
  auto * bartender_page = new QWidget(tabs);
  auto * bartender_layout = new QVBoxLayout(bartender_page);
  auto * bartender_row = new QHBoxLayout();
  bartender_row->addWidget(new QLabel("Period:", bartender_page));
  bartender_period_ = period_choice(bartender_page);
  bartender_row->addWidget(bartender_period_);
  bartender_row->addStretch(1);
  bartender_layout->addLayout(bartender_row);
  bartender_totals_ = new QLabel(bartender_page);
  bartender_totals_->setWordWrap(true);
  bartender_layout->addWidget(bartender_totals_);
  bartender_view_ = numbers_table(
    bartender_page, {"Name", "Category", "Sold", "Bought", "Bartered away", "Bartered for"}
  );
  bartender_layout->addWidget(bartender_view_, 1);
  tabs->addTab(bartender_page, "Bartender");

  // --- on foot: what was used up, and what was killed with what ---
  auto * on_foot_page = new QWidget(tabs);
  auto * on_foot_layout = new QVBoxLayout(on_foot_page);
  auto * on_foot_row = new QHBoxLayout();
  on_foot_row->addWidget(new QLabel("Period:", on_foot_page));
  on_foot_period_ = period_choice(on_foot_page);
  on_foot_row->addWidget(on_foot_period_);
  on_foot_row->addStretch(1);
  on_foot_layout->addLayout(on_foot_row);
  on_foot_totals_ = new QLabel(on_foot_page);
  on_foot_totals_->setWordWrap(true);
  on_foot_layout->addWidget(on_foot_totals_);
  auto * on_foot_tables = new QHBoxLayout();
  consumables_view_ = numbers_table(on_foot_page, {"Consumable", "Used"});
  kills_view_ = numbers_table(on_foot_page, {"Where", "With", "Weapon in hand", "Kills"});
  kills_view_->setToolTip(
    "A kill 2-4 s after a frag grenade was thrown counts as the grenade's - the game does not say what killed.\n"
    "The weapon in hand comes from Status.json, so it is known only for kills seen live"
  );
  on_foot_tables->addWidget(consumables_view_, 1);
  on_foot_tables->addWidget(kills_view_, 2);
  on_foot_layout->addLayout(on_foot_tables, 1);
  tabs->addTab(on_foot_page, "On foot");

  // --- where the carriers are, and where they go ---
  carriers_view_ = new QTableWidget(tabs);
  carriers_view_->setColumnCount(6);
  carriers_view_->setHorizontalHeaderLabels({"Carrier", "Type", "Where", "Jumping to", "Leaves (UTC)", "Ready (UTC)"});
  carriers_view_->verticalHeader()->setVisible(false);
  carriers_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  carriers_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  carriers_view_->horizontalHeader()->setStretchLastSection(true);
  tabs->addTab(carriers_view_, "Carriers");

  // --- what is on each carrier ---
  auto * cargo_page = new QWidget(tabs);
  auto * cargo_layout = new QVBoxLayout(cargo_page);
  auto * cargo_top = new QHBoxLayout();
  cargo_top->addWidget(new QLabel("Carrier:", cargo_page));
  cargo_carrier_ = new QComboBox(cargo_page);
  cargo_top->addWidget(cargo_carrier_, 1);
  cargo_layout->addLayout(cargo_top);
  auto * cargo_note = new QLabel(
    "Changed by every docking at the carrier: what the hold has less of on leaving than on docking is added, "
    "what it has more of is taken off; leaving by escape pod leaves the whole load. Edit a count to correct it.",
    cargo_page
  );
  cargo_note->setWordWrap(true);
  cargo_layout->addWidget(cargo_note);
  cargo_view_ = new QTableWidget(cargo_page);
  cargo_view_->setColumnCount(2);
  cargo_view_->setHorizontalHeaderLabels({"Commodity", "Count (t)"});
  cargo_view_->verticalHeader()->setVisible(false);
  cargo_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  cargo_view_->horizontalHeader()->setStretchLastSection(true);
  cargo_layout->addWidget(cargo_view_, 1);
  auto * cargo_add = new QHBoxLayout();
  cargo_colonisation_ = new QCheckBox("Colonisation", cargo_page);
  cargo_colonisation_->setChecked(true);
  cargo_colonisation_->setToolTip("Only the commodities construction sites ask for");
  cargo_add->addWidget(cargo_colonisation_);
  cargo_commodity_ = new QComboBox(cargo_page);
  cargo_commodity_->setToolTip("A commodity not yet on the carrier");
  cargo_add->addWidget(cargo_commodity_, 1);
  cargo_count_ = new QSpinBox(cargo_page);
  cargo_count_->setRange(0, 1'000'000);
  cargo_add->addWidget(cargo_count_);
  auto * cargo_set = new QPushButton("Set", cargo_page);
  cargo_add->addWidget(cargo_set);
  cargo_layout->addLayout(cargo_add);
  tabs->addTab(cargo_page, "Carrier cargo");

  if(auto carriers{db_.load_carriers()}; carriers)
    for(info::carrier_t const & c: *carriers)
      if(c.tracked or c.carrier_type == "SquadronCarrier")
        cargo_carrier_->addItem(
          qformat("{} ({})", c.carrier_name, c.carrier_id), QVariant::fromValue(qulonglong{c.market_id})
        );
  connect(cargo_carrier_, &QComboBox::currentIndexChanged, this, [this](int) { show_carrier_cargo(); });
  connect(cargo_colonisation_, &QCheckBox::toggled, this, [this](bool) { show_carrier_cargo(); });
  auto const set_count = [this](QString const & key, QString const & name, int count)
  {
    uint64_t const carrier{cargo_carrier_->currentData().toULongLong()};
    if(carrier == 0u or key.isEmpty())
      return;
    if(auto res{db_.set_carrier_cargo(
         carrier,
         key.toStdString(),
         name.toStdString(),
         count,
         std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())
       )};
       not res)
      spdlog::error("data window: failed to set the cargo of {}", carrier);
    show_carrier_cargo();
  };
  connect(
    cargo_set,
    &QPushButton::clicked,
    this,
    [this, set_count]
    { set_count(cargo_commodity_->currentData().toString(), cargo_commodity_->currentText(), cargo_count_->value()); }
  );
  connect(
    cargo_view_,
    &QTableWidget::cellChanged,
    this,
    [this, set_count](int row, int column)
    {
      if(cargo_filling_ or column != 1)
        return;
      bool ok{};
      int const count{cargo_view_->item(row, 1)->text().toInt(&ok)};
      if(ok)
        set_count(
          cargo_view_->item(row, 0)->data(Qt::UserRole).toString(), cargo_view_->item(row, 0)->text(), count
        );
    }
  );
  show_carrier_cargo();
  // a countdown is in minutes; a few seconds keep it current without reading the database too often
  auto * carriers_timer = new QTimer(this);
  connect(
    carriers_timer,
    &QTimer::timeout,
    this,
    [this]
    {
      show_carriers();
      // a docking's balance may have moved the cargo - unless a count is being edited
      // nor while the list of commodities is open, which a refill would close
      QWidget const * const focused{QApplication::focusWidget()};
      if(cargo_view_ and not(focused != nullptr and cargo_view_->isAncestorOf(focused))
         and not cargo_commodity_->view()->isVisible())
        show_carrier_cargo();
    }
  );
  carriers_timer->start(5000);
  show_carriers();

  layout->addWidget(tabs, 1);

  connect(
    carrier_combo_,
    &QComboBox::activated,
    this,
    [this](int index)
    {
      if(index < 0)
        return;

      // the mark belongs to the chosen carrier, so it follows the choice
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
  connect(bartender_period_, &QComboBox::activated, this, [this](int) { show_bartender(); });
  connect(on_foot_period_, &QComboBox::activated, this, [this](int) { show_on_foot(); });

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

  // The filter only makes sense once something is marked - otherwise it would leave an empty list and no
  // way back, because a carrier can only be marked while it is visible in the list
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

  // the mark applies to whatever is selected right now
  auto const chosen{carrier_combo_->currentData().toString().toStdString()};
  QSignalBlocker const quiet{mark_mine_};
  mark_mine_->setEnabled(not chosen.empty());
  mark_mine_->setChecked(std::ranges::any_of(
    *res, [&chosen](info::carrier_t const & c) { return c.carrier_id == chosen and c.tracked; }
  ));
  }

///\brief the carrier's state from the last CarrierStats, ready to be put under the header
///
/// The state and the bartender's shelf are two independent sources arriving at different moments - a
/// stranger's squadron carrier can have a state without a single reading of its shelf, so neither may be
/// made to depend on the other. Hence a timestamp of its own: without it there is no telling whether the
/// fuel figure is a minute old or a week old
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
    // the shelf is known only from a visit to the bartender, so before the first one there is nothing to show
    stock_header_->setText("No bartender reading yet - dock at a carrier and open the bartender");
    }
  else
    show_stock(carrier_combo_->currentData().toString().toStdString());

  show_acquisitions();
  show_bartender();
  show_on_foot();
  }

namespace
  {
  auto period_start(QComboBox const * combo) -> std::chrono::sys_seconds
    {
    auto const days{combo->currentData().toInt()};
    auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
    return days > 0 ? now - std::chrono::days{days} : std::chrono::sys_seconds{};
    }

  ///\brief a cell sorted by its number, not by its text
  auto number_cell(qulonglong value) -> QTableWidgetItem *
    {
    auto * cell = new QTableWidgetItem;
    cell->setData(Qt::DisplayRole, value);
    cell->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return cell;
    }

  auto text_cell(std::string_view text) -> QTableWidgetItem *
    { return new QTableWidgetItem(QString::fromUtf8(text.data(), qsizetype(text.size()))); }

  ///\brief the bartender's words for the backpack's categories
  auto category_name(std::string_view category) -> std::string_view
    {
    if(category == "Item")
      return "Goods";
    if(category == "Component")
      return "Assets";
    return category;
    }
  }  // namespace

auto micro_resource_window_t::show_bartender() -> void
  {
  auto const since{period_start(bartender_period_)};
  auto rows{db_.load_bartender_summary(since)};
  auto totals{db_.load_bartender_totals(since)};
  if(not rows or not totals)
    {
    spdlog::error("failed to load bartender statistics");
    return;
    }

  // the counts by category, the credits by kind - a sale mixes kinds and the game prices only the whole
  std::map<std::string, std::array<uint64_t, 4>> by_category;
  for(info::bartender_summary_t const & row: *rows)
    {
    auto & sums{by_category[std::string{category_name(row.category.empty() ? "?" : row.category)}]};
    sums[0] += row.sold;
    sums[1] += row.bought;
    sums[2] += row.bartered_away;
    sums[3] += row.bartered_for;
    }
  std::string text;
  for(info::bartender_total_t const & total: *totals)
    text += std::format(
      "{}{}: {} transactions{}",
      text.empty() ? "" : "   ",
      simple_enum::enum_name(total.kind),
      total.transactions,
      total.kind == info::micro_trade_e::bartered ? std::string{} : std::format(", {} Cr", format_credits_value(total.credits))
    );
  for(auto const & [category, sums]: by_category)
    text += std::format(
      "\n{}: sold {}, bought {}, bartered away {}, bartered for {}", category, sums[0], sums[1], sums[2], sums[3]
    );
  bartender_totals_->setText(text.empty() ? QString{"Nothing went through a counter in this period"}
                                          : QString::fromStdString(text));

  bartender_view_->setSortingEnabled(false);
  bartender_view_->setRowCount(int(rows->size()));
  for(int ix{}; info::bartender_summary_t const & row: *rows)
    {
    bartender_view_->setItem(ix, 0, text_cell(row.localised.empty() ? row.name : row.localised));
    bartender_view_->setItem(ix, 1, text_cell(category_name(row.category)));
    bartender_view_->setItem(ix, 2, number_cell(row.sold));
    bartender_view_->setItem(ix, 3, number_cell(row.bought));
    bartender_view_->setItem(ix, 4, number_cell(row.bartered_away));
    bartender_view_->setItem(ix, 5, number_cell(row.bartered_for));
    ++ix;
    }
  bartender_view_->setSortingEnabled(true);
  }

auto micro_resource_window_t::show_on_foot() -> void
  {
  auto const since{period_start(on_foot_period_)};
  auto used{db_.load_consumable_summary(since)};
  auto kills{db_.load_foot_kills(since)};
  if(not used or not kills)
    {
    spdlog::error("failed to load on foot statistics");
    return;
    }

  consumables_view_->setSortingEnabled(false);
  consumables_view_->setRowCount(int(used->size()));
  for(int ix{}; info::consumable_summary_t const & row: *used)
    {
    consumables_view_->setItem(ix, 0, text_cell(row.localised.empty() ? row.name : row.localised));
    consumables_view_->setItem(ix, 1, number_cell(row.used));
    ++ix;
    }
  consumables_view_->setSortingEnabled(true);

  auto const where = [](info::foot_kill_e kind) -> std::string_view
  {
    switch(kind)
      {
      case info::foot_kill_e::conflict_zone: return "Conflict zone";
      case info::foot_kill_e::murder:        return "Settlement, murder";
      case info::foot_kill_e::bounty:        return "Settlement, bounty";
      }
    return "?";
  };

  // a conflict zone and a raid on a settlement are different work, so each gets its own sums
  std::array<std::array<uint32_t, 2>, 2> sums{};
  kills_view_->setSortingEnabled(false);
  kills_view_->setRowCount(int(kills->size()));
  for(int ix{}; info::foot_kill_summary_t const & row: *kills)
    {
    kills_view_->setItem(ix, 0, text_cell(where(row.kind)));
    kills_view_->setItem(ix, 1, text_cell(row.grenade ? "grenade?" : "weapon"));
    kills_view_->setItem(ix, 2, text_cell(row.weapon.empty() ? "unknown" : row.weapon));
    kills_view_->setItem(ix, 3, number_cell(row.kills));
    sums[row.kind == info::foot_kill_e::conflict_zone ? 0u : 1u][row.grenade ? 1u : 0u] += row.kills;
    ++ix;
    }
  kills_view_->setSortingEnabled(true);

  on_foot_totals_->setText(qformat(
    "Conflict zones: {} kills, {} of them likely by grenade.   Settlements: {} kills, {} of them likely by grenade.",
    sums[0][0] + sums[0][1],
    sums[0][1],
    sums[1][0] + sums[1][1],
    sums[1][1]
  ));
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

  // the time of the reading is the most important number here - it says how old this picture is
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

auto micro_resource_window_t::show_carriers() -> void
  {
  if(not carriers_view_ or not isVisible())
    return;
  // after a jump a carrier cannot jump again for five minutes
  constexpr std::chrono::minutes cooldown{5};
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto carriers{db_.load_carrier_states(now, cooldown)};
  if(not carriers)
    {
    spdlog::error("data window: failed to load the carriers");
    return;
    }
  carriers_view_->setRowCount(0);
  for(info::carrier_state_t const & c: *carriers)
    {
    int const row{carriers_view_->rowCount()};
    carriers_view_->insertRow(row);
    QString const who{
      c.name.empty() ? qformat("{}", c.carrier_id)
                     : (c.callsign.empty() ? QString::fromStdString(c.name) : qformat("{} ({})", c.name, c.callsign))
    };
    carriers_view_->setItem(row, 0, new QTableWidgetItem(who));
    carriers_view_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(c.carrier_type)));
    carriers_view_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(c.jumping ? c.from : c.system)));
    if(c.jumping)
      {
      carriers_view_->setItem(
        row, 3, new QTableWidgetItem(QString::fromStdString(c.to_body.empty() ? c.to : c.to_body))
      );
      carriers_view_->setItem(row, 4, new QTableWidgetItem(qformat("{:%H:%M}", c.departure)));
      carriers_view_->setItem(
        row, 5, new QTableWidgetItem(qformat("{}{:%H:%M}", c.arrived ? "" : "~", c.arrival + cooldown))
      );
      }
    }
  }

auto micro_resource_window_t::show_carrier_cargo() -> void
  {
  if(not cargo_view_)
    return;
  uint64_t const carrier{cargo_carrier_->currentData().toULongLong()};
  auto cargo{db_.load_carrier_cargo(carrier)};
  cargo_filling_ = true;
  cargo_view_->setRowCount(0);
  if(cargo)
    for(info::carrier_cargo_t const & item: *cargo)
      {
      int const row{cargo_view_->rowCount()};
      cargo_view_->insertRow(row);
      auto * name = new QTableWidgetItem(QString::fromStdString(item.commodity));
      name->setData(Qt::UserRole, QString::fromStdString(item.key));
      name->setFlags(name->flags() & ~Qt::ItemIsEditable);
      cargo_view_->setItem(row, 0, name);
      cargo_view_->setItem(row, 1, new QTableWidgetItem(QString::number(qlonglong(item.count))));
      }
  cargo_filling_ = false;
  fill_cargo_commodities(cargo ? *cargo : std::vector<info::carrier_cargo_t>{});
  }

auto micro_resource_window_t::fill_cargo_commodities(std::vector<info::carrier_cargo_t> const & cargo) -> void
  {
  if(not cargo_commodity_)
    return;
  auto names{db_.load_commodity_names()};
  if(not names)
    return;
  QString const keep{cargo_commodity_->currentData().toString()};
  bool const colonisation{cargo_colonisation_->isChecked()};
  QSignalBlocker const block{cargo_commodity_};
  cargo_commodity_->clear();
  for(auto const & [key, name]: *names)
    {
    if(colonisation and not commodity_facts::find(key))
      continue;
    if(std::ranges::any_of(cargo, [&key](info::carrier_cargo_t const & item) { return item.key == key; }))
      continue;
    cargo_commodity_->addItem(QString::fromStdString(name), QString::fromStdString(key));
    }
  if(auto const index{cargo_commodity_->findData(keep)}; index >= 0)
    cargo_commodity_->setCurrentIndex(index);
  }
