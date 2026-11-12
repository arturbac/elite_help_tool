#include <stars_page.h>
#include <event_guard.h>
#include <galaxy_region.h>
#include <logic.h>

#include <algorithm>
#include <format>
#include <ranges>

#include <qboxlayout.h>
#include <qcombobox.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qtablewidget.h>
#include <qtimer.h>

#include <spdlog/spdlog.h>

namespace
  {
constexpr char const * explanation{
  "The stars of the systems visited - a system with a star in the database, since only a visit scans one. The arrival"
  " star is the one the jump drops the ship at; a system of several stars counts each of them under All stars."
  " The kinds still rare are the ones to set the galaxy map's star filter to."
};

auto make_table(QWidget * parent) -> QTableWidget *
  {
  auto * view{new QTableWidget(parent)};
  view->setColumnCount(5);
  view->setHorizontalHeaderLabels({"Star", "What it is", "Arrival star of", "Share", "All stars"});
  view->verticalHeader()->setVisible(false);
  view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  return view;
  }

auto fill(QTableWidget * view, star_census::census_t const & census) -> void
  {
  auto const cell = [](std::string const & text, bool number) -> QTableWidgetItem *
  {
    auto * item{new QTableWidgetItem(QString::fromStdString(text))};
    if(number)
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
  };
  view->setRowCount(int(census.rows.size()) + 1);
  uint32_t all{};
  for(auto const & [index, row]: census.rows | std::views::enumerate)
    {
    int const r{int(index)};
    all += row.all;
    view->setItem(r, 0, cell(row.key, false));
    view->setItem(r, 1, cell(std::string{star_census::description(row.type)}, false));
    view->setItem(r, 2, cell(std::format("{}", row.arrival), true));
    // the share of the systems: where it is low, the map's filter is worth switching to that class
    view->setItem(
      r,
      3,
      cell(census.systems != 0u ? std::format("{:.1f}%", 100.0 * row.arrival / census.systems) : std::string{}, true)
    );
    view->setItem(r, 4, cell(std::format("{}", row.all), true));
    }
  int const total{int(census.rows.size())};
  view->setItem(total, 0, cell("All", false));
  view->setItem(total, 1, cell({}, false));
  view->setItem(total, 2, cell(std::format("{}", census.systems), true));
  view->setItem(total, 3, cell(census.systems != 0u ? "100%" : "", true));
  view->setItem(total, 4, cell(std::format("{}", all), true));
  }
  }  // namespace

stars_page_t::stars_page_t(std::string db_path, current_state_t const & state, QWidget * parent) :
    QWidget{parent},
    db_path_{std::move(db_path)},
    state_{state}
  {
  auto * layout = new QVBoxLayout(this);
  auto * note{new QLabel(explanation, this)};
  note->setWordWrap(true);
  layout->addWidget(note);

  auto * row = new QHBoxLayout();
  row->addWidget(new QLabel("Group by", this));
  grouping_ = new QComboBox(this);
  grouping_->addItem("as the galaxy map's star filter");
  grouping_->addItem("type (G)");
  grouping_->addItem("type and subclass (G9)");
  grouping_->addItem("type and luminosity (G Vab)");
  row->addWidget(grouping_);
  row->addStretch(1);
  layout->addLayout(row);

  region_label_ = new QLabel(this);
  layout->addWidget(region_label_);
  region_view_ = make_table(this);
  layout->addWidget(region_view_, 1);
  all_label_ = new QLabel(this);
  layout->addWidget(all_label_);
  all_view_ = make_table(this);
  layout->addWidget(all_view_, 1);

  connect(
    grouping_,
    &QComboBox::currentIndexChanged,
    this,
    [this](int) { eht::event_guard("stars grouping", [this] { show_census(); }); }
  );
  poll_ = new QTimer(this);
  poll_->setInterval(std::chrono::seconds{5});
  connect(poll_, &QTimer::timeout, this, [this] { eht::event_guard("stars refresh", [this] { refresh(false); }); });
  }

auto stars_page_t::showEvent(QShowEvent * event) -> void
  {
  QWidget::showEvent(event);
  eht::event_guard("stars refresh", [this] { refresh(true); });
  poll_->start();
  }

auto stars_page_t::refresh(bool now) -> void
  {
  if(not isVisible())
    {
    poll_->stop();
    return;
    }
  // the cheap look first: another system, or a minute since the last reading, else nothing to read
  auto const clock{std::chrono::steady_clock::now()};
  uint64_t const here{state_.system.system_address};
  if(not now and here == read_at_system_ and clock - read_ < std::chrono::minutes{1})
    return;
  read_ = clock;
  read_at_system_ = here;
  // a connection of its own for each reading, as the album's; the state's belongs to the journal following thread
  database_storage_t db{db_path_};
  if(auto const opened{db.open()}; not opened)
    spdlog::error("stars: the database could not be opened: {}", opened.error().message());
  else if(auto loaded{db.load_star_census()}; loaded)
    systems_ = std::move(*loaded);
  else
    spdlog::error("stars: the systems visited could not be read: {}", loaded.error().message());
  show_census();
  }

auto stars_page_t::show_census() -> void
  {
  auto const grouping{static_cast<star_census::grouping_e>(std::max(grouping_->currentIndex(), 0))};
  std::string_view const region{galaxy_region::of_address(state_.system.system_address)};
  star_census::census_t const in_region{star_census::count(
    systems_,
    grouping,
    [region](star_census::system_t const & system)
    { return not region.empty() and galaxy_region::of_address(system.address) == region; }
  )};
  star_census::census_t const everywhere{
    star_census::count(systems_, grouping, [](star_census::system_t const &) { return true; })
  };
  region_label_->setText(QString::fromStdString(
    region.empty() ? std::string{"Region: not known yet"}
                   : std::format("{}, the region you are in: {} systems visited", region, in_region.systems)
  ));
  all_label_->setText(QString::fromStdString(std::format("All regions: {} systems visited", everywhere.systems)));
  fill(region_view_, in_region);
  fill(all_view_, everywhere);
  }
