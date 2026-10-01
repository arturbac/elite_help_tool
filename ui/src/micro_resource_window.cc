#include <micro_resource_window.h>
#include <data/carrier.h>
#include <data/micro_resources.h>
#include <data/progress.h>
#include <bar_sales.h>
#include <biology.h>
#include <cmath>
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
#include <format_credits.h>

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

  // --- what one's own bar sold ---
  auto * bar_page = new QWidget(tabs);
  auto * bar_layout = new QVBoxLayout(bar_page);
  auto * bar_row = new QHBoxLayout();
  bar_row->addWidget(new QLabel("Carrier:", bar_page));
  bar_carrier_ = new QComboBox(bar_page);
  bar_row->addWidget(bar_carrier_, 1);
  bar_row->addWidget(new QLabel("Period:", bar_page));
  bar_period_ = period_choice(bar_page);
  bar_row->addWidget(bar_period_);
  bar_layout->addLayout(bar_row);
  bar_totals_ = new QLabel(bar_page);
  bar_totals_->setWordWrap(true);
  bar_layout->addWidget(bar_totals_);
  for(size_t ix{}; char const * heading: {"Data", "Goods", "Assets"})
    {
    bar_headings_[ix] = new QLabel(QString{"<b>%1</b>"}.arg(heading), bar_page);
    bar_layout->addWidget(bar_headings_[ix]);
    auto * view = numbers_table(
      bar_page,
      {"Item", "Price", "Stock", "Sold", "Revenue", "Absences with a sale", "Port price", "Price / port", "Bought in"}
    );
    view->setToolTip(
      "Read from the shelf: a fall in stock between two bartender readings is a sale, at the price shown before it;\n"
      "a rise is what you added. Read the bar on arriving, before adding anything, and again after.\n"
      "Absences with a sale: in how many of the absences the item lay on the shelf it sold at all - absences, not pieces.\n"
      "Port price: what a port's bartender pays, worked out from your own sales at ports - blank when never sold there"
    );
    bar_layout->addWidget(view, 1);
    bar_views_[ix++] = view;
    }
  tabs->addTab(bar_page, "Bar sales");

  // --- which missions pay best ---
  auto * mission_page = new QWidget(tabs);
  auto * mission_layout = new QVBoxLayout(mission_page);
  auto * mission_row = new QHBoxLayout();
  mission_row->addWidget(new QLabel("Missions completed:", mission_page));
  mission_period_ = period_choice(mission_page);
  mission_row->addWidget(mission_period_);
  mission_row->addStretch(1);
  mission_layout->addLayout(mission_row);
  mission_note_ = new QLabel(mission_page);
  mission_note_->setWordWrap(true);
  mission_layout->addWidget(mission_note_);
  // assets are left out - they sell for next to nothing and lie on the bar as a lure
  for(size_t ix{}; char const * heading: {"Data", "Goods"})
    {
    mission_layout->addWidget(new QLabel(QString{"<b>%1</b>"}.arg(heading), mission_page));
    auto * view = numbers_table(
      mission_page,
      {"Mission type", "Missions", "Sold / mission", "Sold, all", "At the bar / mission", "At the bar, all",
       "Unvalued kinds", "Rewards given most"}
    );
    view->setToolTip(
      "Sold: what the rewards have brought at your carrier's bar - each piece at what one piece put on the shelf\n"
      "has brought so far, the revenue of its sales over all the pieces put up.\n"
      "At the bar: what the material rewards fetch at your carrier's bar.\n"
      "A material reward is valued at the price on your carrier's bar times the share of absences it sold in there -\n"
      "a price nobody pays counts for little. A kind never put on the bar takes a port's price when it is known,\n"
      "otherwise it counts as nothing and is listed under Unvalued kinds"
    );
    mission_layout->addWidget(view, 1);
    mission_views_[ix++] = view;
    }
  tabs->addTab(mission_page, "Mission value");

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
      // the Bar sales and Mission value tabs list only tracked carriers - marking one here would
      // otherwise not show up there until something else caused a refresh
      show_bar_sales();
      show_mission_value();
    }
  );

  connect(only_mine_, &QCheckBox::toggled, this, [this](bool) { reload_carriers(); });

  // --- what cartographic data paid against the estimate ---
  auto * cartography_page = new QWidget(tabs);
  auto * cartography_layout = new QVBoxLayout(cartography_page);
  cartography_note_ = new QLabel(cartography_page);
  cartography_note_->setWordWrap(true);
  cartography_layout->addWidget(cartography_note_);
  cartography_view_ = new QTableWidget(cartography_page);
  cartography_view_->setColumnCount(9);
  cartography_view_->setHorizontalHeaderLabels(
    {"Sold (UTC)", "Systems", "Bodies sold", "Bodies priced", "Estimate", "Base", "Bonus", "Paid", "Paid / estimate"}
  );
  cartography_view_->verticalHeader()->setVisible(false);
  cartography_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  cartography_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  cartography_view_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  cartography_layout->addWidget(cartography_view_, 1);
  tabs->addTab(cartography_page, "Cartography");
  connect(
    tabs,
    &QTabWidget::currentChanged,
    this,
    [this, tabs, cartography_page](int index)
    {
      if(tabs->widget(index) == cartography_page)
        show_cartography(true);
    }
  );

  connect(period_combo_, &QComboBox::activated, this, [this](int) { show_acquisitions(); });
  connect(bartender_period_, &QComboBox::activated, this, [this](int) { show_bartender(); });
  connect(on_foot_period_, &QComboBox::activated, this, [this](int) { show_on_foot(); });
  connect(bar_period_, &QComboBox::activated, this, [this](int) { show_bar_sales(); });
  connect(bar_carrier_, &QComboBox::activated, this, [this](int) { show_bar_sales(); show_mission_value(); });
  connect(mission_period_, &QComboBox::activated, this, [this](int) { show_mission_value(); });

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
  show_bar_sales();
  show_mission_value();
  show_on_foot();
  show_cartography(false);
  }

auto micro_resource_window_t::show_cartography(bool now) -> void
  {
  // read only while in view, and then not at every change of the game's state
  if(not cartography_view_->isVisible() and not now)
    return;
  auto const clock{std::chrono::steady_clock::now()};
  if(not now and clock - cartography_read_ < std::chrono::minutes{1})
    return;
  cartography_read_ = clock;

  std::string fid;
  if(auto owner{db_.load_owner()}; owner and *owner)
    fid = (*owner)->fid;
  auto sales{bio::cartography_sales("journal-dir", fid)};
  std::ranges::reverse(sales);

  std::string note{
    "Each sale of cartographic data from the journals, against what EHT reckoned the bodies scanned there were worth"
    " at the moment of the sale - the same reckoning as the cartography a death would cost. The game writes one sum"
    " for all the systems of a sale and nothing for a body, so only a sale of one system gives an exact price:"
    " sell system by system to learn more."
  };
  if(auto const accuracy{bio::estimate_accuracy(sales)}; accuracy)
    note += std::format(
      "\n{} sales of one system: paid {:.2f} times the estimate (median), from {:.2f} to {:.2f}.",
      accuracy->sales,
      accuracy->median,
      accuracy->lowest,
      accuracy->highest
    );
  cartography_note_->setText(QString::fromStdString(note));

  auto const cell = [](std::string const & text, bool number) -> QTableWidgetItem *
  {
    auto * item{new QTableWidgetItem(QString::fromStdString(text))};
    if(number)
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
  };
  cartography_view_->setRowCount(int(sales.size()));
  for(auto const & [row, sale]: sales | std::views::enumerate)
    {
    int const r{int(row)};
    bool const single{sale.systems.size() == 1u};
    std::string systems{single ? sale.systems.front() : std::format("{} systems", sale.systems.size())};
    cartography_view_->setItem(r, 0, cell(std::format("{:%d.%m.%Y %H:%M}", sale.when), false));
    auto * names{cell(systems, false)};
    if(not single)
      {
      std::string all;
      for(std::string const & name: sale.systems)
        all += (all.empty() ? "" : "\n") + name;
      names->setToolTip(QString::fromStdString(all));
      }
    cartography_view_->setItem(r, 1, names);
    cartography_view_->setItem(r, 2, cell(sale.bodies != 0u ? std::format("{}", sale.bodies) : std::string{}, true));
    cartography_view_->setItem(r, 3, cell(std::format("{}", sale.priced), true));
    cartography_view_->setItem(r, 4, cell(format_credits_value(sale.estimate), true));
    cartography_view_->setItem(r, 5, cell(format_credits_value(sale.base_value), true));
    cartography_view_->setItem(r, 6, cell(format_credits_value(sale.bonus), true));
    cartography_view_->setItem(r, 7, cell(format_credits_value(sale.total), true));
    // a sale of several systems is a sum over all of them, so its ratio is shown but says less
    auto * ratio{cell(
      sale.estimate != 0u ? std::format("{:.2f}", double(sale.total) / double(sale.estimate)) : std::string{"-"}, true
    )};
    if(not single)
      ratio->setForeground(QBrush{QColor{0x8a, 0x8a, 0x8a}});
    cartography_view_->setItem(r, 8, ratio);
    }
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

  ///\brief a sum read at a glance - 12.3M rather than 12'345'678 - but sorted by the number itself
  class credits_cell_t final : public QTableWidgetItem
    {
  public:
    explicit credits_cell_t(double value) : QTableWidgetItem{human_credits(value)}
      {
      setData(Qt::UserRole, value);
      setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      }

    auto operator<(QTableWidgetItem const & other) const -> bool override
      { return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble(); }

  private:
    static auto human_credits(double value) -> QString
      {
      if(value >= 1e9)
        return qformat("{:.1f}B", value / 1e9);
      if(value >= 1e6)
        return qformat("{:.1f}M", value / 1e6);
      if(value >= 1e3)
        return qformat("{:.1f}k", value / 1e3);
      return qformat("{:.0f}", value);
      }
    };

  auto credits_cell(double value) -> QTableWidgetItem * { return new credits_cell_t{value}; }

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

auto micro_resource_window_t::show_bar_sales() -> void
  {
  // only one's own carriers - a stranger's bar is read at a single visit, which says nothing of its sales
    {
    QString const chosen{bar_carrier_->currentData().toString()};
    QSignalBlocker const block{bar_carrier_};
    bar_carrier_->clear();
    if(auto carriers{db_.load_carriers()}; carriers)
      for(info::carrier_t const & c: *carriers)
        if(c.tracked)
          bar_carrier_->addItem(qformat("{} ({})", c.carrier_name, c.carrier_id), QString::fromStdString(c.carrier_id));
    if(auto const index{bar_carrier_->findData(chosen)}; index >= 0)
      bar_carrier_->setCurrentIndex(index);
    }
  if(bar_carrier_->count() == 0)
    {
    bar_totals_->setText("No carrier of your own with a bar read yet");
    for(QTableWidget * view: bar_views_)
      view->setRowCount(0);
    return;
    }

  auto history{db_.load_carrier_history(bar_carrier_->currentData().toString().toStdString(), period_start(bar_period_))};
  auto port_rows{db_.load_port_sale_rows()};
  if(not history or not port_rows)
    {
    spdlog::error("failed to load bar sales");
    return;
    }
  auto const items{bar::sales(*history)};
  auto const port{bar::port_prices(*port_rows)};

  uint64_t revenue{};
  uint32_t sold{};
  for(bar::item_sales_t const & item: items)
    {
    revenue += item.revenue;
    sold += item.sold;
    }
  bar_totals_->setText(qformat(
    "{} sold for {} Cr over {} absences between bartender readings. An absence whose arrival was not read first "
    "mixes what sold with what was added, and shows only the difference.",
    sold,
    format_credits_value(revenue),
    bar::absences(*history)
  ));

  // the bartender's own split, in the order the bar lists them
  auto const table_of = [](std::string_view category) -> size_t
  {
    return category == "Data" ? 0u : category == "Item" ? 1u : category == "Component" ? 2u : 3u;
  };
  for(size_t table{}; QTableWidget * view: bar_views_)
    {
    view->setSortingEnabled(false);
    view->setRowCount(0);
    uint32_t table_sold{};
    uint64_t table_revenue{};
    for(bar::item_sales_t const & item: items)
      {
      if(table_of(item.category) != table)
        continue;
      table_sold += item.sold;
      table_revenue += item.revenue;
      int const ix{view->rowCount()};
      view->insertRow(ix);
      view->setItem(ix, 0, text_cell(item.localised.empty() ? item.name : item.localised));
      view->setItem(ix, 1, number_cell(item.price));
      view->setItem(ix, 2, number_cell(item.stock));
      view->setItem(ix, 3, number_cell(item.sold));
      view->setItem(ix, 4, credits_cell(double(item.revenue)));
      view->setItem(
        ix,
        5,
        text_cell(item.absences_listed == 0u ? std::string{} : std::format("{} of {}", item.absences_sold, item.absences_listed))
      );
      if(auto it{port.find(item.name)}; it != port.end() and it->second > 0.0)
        {
        view->setItem(ix, 6, number_cell(qulonglong(std::llround(it->second))));
        auto * ratio = new QTableWidgetItem;
        ratio->setData(Qt::DisplayRole, std::round(double(item.price) / it->second * 10.0) / 10.0);
        ratio->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        view->setItem(ix, 7, ratio);
        }
      else
        {
        view->setItem(ix, 6, text_cell(""));
        view->setItem(ix, 7, text_cell(""));
        }
      view->setItem(ix, 8, number_cell(item.bought_in));
      }
    view->setSortingEnabled(true);
    view->sortByColumn(4, Qt::DescendingOrder);
    bar_headings_[table]->setText(qformat(
      "<b>{}</b> - {} sold for {} Cr", std::array{"Data", "Goods", "Assets"}[table], table_sold, format_credits_value(table_revenue)
    ));
    ++table;
    }
  }

auto micro_resource_window_t::show_mission_value() -> void
  {
  // what a kind fetches is learnt from the whole history of the bar, whatever period the missions are from
  std::map<std::string, bar::item_value_t> values;
  auto port_rows{db_.load_port_sale_rows()};
  std::map<std::string, double> const port{port_rows ? bar::port_prices(*port_rows) : std::map<std::string, double>{}};
  std::string carrier_name{"no carrier of your own"};
  if(bar_carrier_->count() != 0)
    {
    carrier_name = bar_carrier_->currentText().toStdString();
    if(auto history{db_.load_carrier_history(bar_carrier_->currentData().toString().toStdString(), {})}; history)
      values = bar::item_values(bar::sales(*history), port);
    }
  else
    values = bar::item_values({}, port);

  auto rows{db_.load_mission_rewards(period_start(mission_period_))};
  auto names{db_.load_micro_resource_names()};
  if(not rows)
    {
    spdlog::error("failed to load mission rewards");
    return;
    }
  auto const readable = [&](std::string const & name) -> std::string
  {
    if(names)
      if(auto it{names->find(name)}; it != names->end() and not it->second.empty())
        return it->second;
    return name;
  };

  mission_note_->setText(qformat(
    "Sold: what a mission's rewards have brought at the bar of {0} - the revenue of each kind over all its pieces put "
    "on the shelf. At the bar: the price there times the share of absences they sold in.",
    carrier_name
  ));

  for(size_t table{}; QTableWidget * view: mission_views_)
    {
    auto const missions{bar::mission_values(*rows, values, table == 0u ? "Data" : "Item")};
    view->setSortingEnabled(false);
    view->setRowCount(0);
    for(bar::mission_value_t const & m: missions)
      {
      int const ix{view->rowCount()};
      view->insertRow(ix);
      double const per_materials{m.missions != 0u ? m.materials / m.missions : 0.0};
      std::string given;
      for(auto const & [name, count]: m.rewards | std::views::take(3))
        given += std::format("{}{} {}", given.empty() ? "" : ", ", count, readable(name));
      // only missions on foot give such rewards, the type need not say it
      view->setItem(ix, 0, text_cell(m.type.starts_with("OnFoot_") ? std::string_view{m.type}.substr(7) : std::string_view{m.type}));
      view->setItem(ix, 1, number_cell(m.missions));
      view->setItem(ix, 2, credits_cell(m.missions != 0u ? m.sold / m.missions : 0.0));
      view->setItem(ix, 3, credits_cell(m.sold));
      view->setItem(ix, 4, credits_cell(per_materials));
      view->setItem(ix, 5, credits_cell(m.materials));
      view->setItem(ix, 6, number_cell(m.unvalued_kinds));
      view->setItem(ix, 7, text_cell(given));
      }
    view->setSortingEnabled(true);
    view->sortByColumn(2, Qt::DescendingOrder);
    ++table;
    }
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
