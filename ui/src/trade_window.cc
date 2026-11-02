#include <trade_window.h>

#include <buy_search.h>
#include <buy_search_tab.h>
#include <data/market.h>
#include <data/station.h>
#include <faction_state_window.h>
#include <logic.h>
#include <qformat.h>

#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qsortfilterproxymodel.h>
#include <qsplitter.h>
#include <qtableview.h>
#include <qtabwidget.h>
#include <qtreewidget.h>

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <optional>

namespace
  {
constexpr int sort_role{Qt::UserRole + 1};
constexpr int market_role{Qt::UserRole + 2};
constexpr int system_role{Qt::UserRole + 3};

enum struct column_e : int
  {
  place,
  ly,
  type,
  pad,
  ls,
  market,
  read,
  column_max
  };

///\brief sorts by the value of the column - a system by its light years from here, a place within it by its Ls -
/// and puts what has none after what has one
class sorted_item_t final : public QTreeWidgetItem
  {
public:
  using QTreeWidgetItem::QTreeWidgetItem;

  auto operator<(QTreeWidgetItem const & other) const -> bool override
    {
    int const column{treeWidget() != nullptr ? treeWidget()->sortColumn() : 0};
    QVariant const a{data(column, sort_role)};
    QVariant const b{other.data(column, sort_role)};
    if(a.isValid() and b.isValid())
      return QVariant::compare(a, b) == QPartialOrdering::Less;
    if(a.isValid() or b.isValid())
      return a.isValid();
    return QTreeWidgetItem::operator<(other);
    }
  };

[[nodiscard]]
auto pad_name(std::optional<buy_search::pad_e> pad) -> QString
  {
  if(not pad)
    return "?";
  switch(*pad)
    {
    case buy_search::pad_e::large:  return "L";
    case buy_search::pad_e::medium: return "M";
    case buy_search::pad_e::small:  return "S";
    case buy_search::pad_e::none:   return "-";
    }
  return "?";
  }
  }  // namespace

to_update_tab_t::to_update_tab_t(current_state_t const & state, database_storage_t & db, QWidget * parent) :
    QWidget{parent},
    state_{state},
    db_{db}
  {
  auto * layout{new QVBoxLayout(this)};
  auto * filter_row{new QHBoxLayout()};
  auto const check = [&](char const * text, bool on)
  {
    auto * box{new QCheckBox(text)};
    box->setChecked(on);
    filter_row->addWidget(box);
    connect(box, &QCheckBox::toggled, this, [this](bool) { refresh(); });
    return box;
  };
  space_stations_ = check("Space stations", true);
  outposts_ = check("Outposts", true);
  surface_ports_ = check("Surface ports", true);
  settlements_ = check("Settlements", true);
  filter_row->addSpacing(12);
  only_to_update_ = check("Only to update", true);
  only_to_update_->setToolTip("Leave out the markets read since the present owner took the place");
  filter_row->addStretch(1);
  layout->addLayout(filter_row);

  note_ = new QLabel();
  note_->setWordWrap(true);
  layout->addWidget(note_);

  view_ = new QTreeWidget();
  view_->setColumnCount(int(column_e::column_max));
  view_->setHeaderLabels({"System | Place", "ly", "Type", "Pad", "Ls", "Market", "Read"});
  view_->setSortingEnabled(true);
  // the nearest system first, and within it the place nearest the arrival star
  view_->sortByColumn(int(column_e::ly), Qt::AscendingOrder);
  layout->addWidget(view_, 1);

  connect(
    view_,
    &QTreeWidget::itemDoubleClicked,
    this,
    [this](QTreeWidgetItem * item, int)
    {
      if(item != nullptr and item->data(0, market_role).isValid())
        emit market_clicked(item->data(0, market_role).toULongLong(), item->data(0, system_role).toString());
    }
  );
  }

auto to_update_tab_t::showEvent(QShowEvent * event) -> void
  {
  QWidget::showEvent(event);
  refresh();
  }

auto to_update_tab_t::refresh() -> void
  {
  view_->clear();
  auto rows{db_.load_markets_to_update()};
  if(not rows)
    {
    spdlog::error("to update: failed to load the places of our systems");
    note_->setText("The places of our systems could not be read - see the log.");
    return;
    }

  using enum buy_search::place_e;
  std::map<buy_search::place_e, QCheckBox *> const kinds{
    {space_station, space_stations_}, {outpost, outposts_}, {surface_port, surface_ports_}, {settlement, settlements_}
  };
  std::optional<std::array<double, 3>> here;
  {
  std::scoped_lock const lock{state_.mutex_};
  if(state_.system.system_address != 0u)
    here = state_.system.system_location;
  }
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  view_->setSortingEnabled(false);
  std::map<uint64_t, QTreeWidgetItem *> systems;
  std::map<uint64_t, std::pair<unsigned, unsigned>> counts;
  unsigned never{};
  unsigned changed{};
  for(info::market_to_update_t const & row: *rows)
    {
    if(not buy_search::worth_reading(row))
      continue;
    auto const kind{kinds.find(buy_search::place_of(row.station_type))};
    if(kind == kinds.end() or not kind->second->isChecked())
      continue;
    buy_search::market_state_e const state{buy_search::market_state(row)};
    ++counts[row.system_address].second;
    if(state != buy_search::market_state_e::read)
      ++counts[row.system_address].first;
    if(state == buy_search::market_state_e::never_read)
      ++never;
    else if(state == buy_search::market_state_e::owner_changed)
      ++changed;
    if(only_to_update_->isChecked() and state == buy_search::market_state_e::read)
      continue;

    auto & system{systems[row.system_address]};
    if(system == nullptr)
      {
      system = new sorted_item_t(view_);
      system->setExpanded(true);
      if(here and row.loc_x and row.loc_y and row.loc_z)
        {
        double const ly{std::hypot((*here)[0] - *row.loc_x, (*here)[1] - *row.loc_y, (*here)[2] - *row.loc_z)};
        system->setText(int(column_e::ly), qformat("{:.1f}", ly));
        system->setData(int(column_e::ly), sort_role, ly);
        }
      }
    auto * item{new sorted_item_t(system)};
    item->setText(int(column_e::place), QString::fromStdString(row.station));
    item->setText(int(column_e::type), QString::fromStdString(row.station_type));
    item->setText(
      int(column_e::pad), pad_name(buy_search::largest_pad(row.pads_small, row.pads_medium, row.pads_large))
    );
    item->setText(int(column_e::ls), qformat("{:.0f}", row.dist_from_star_ls));
    item->setData(int(column_e::ls), sort_role, row.dist_from_star_ls);
    // under a system the places go by their distance from the arrival star whichever way the systems are sorted
    item->setData(int(column_e::ly), sort_role, row.dist_from_star_ls);
    switch(state)
      {
      case buy_search::market_state_e::never_read: item->setText(int(column_e::market), "never read"); break;
      case buy_search::market_state_e::owner_changed:
        item->setText(int(column_e::market), qformat("owner changed since - now {}", row.owner));
        break;
      case buy_search::market_state_e::read: item->setText(int(column_e::market), "read"); break;
      }
    if(row.updated)
      {
      auto const days{std::chrono::floor<std::chrono::days>(now - *row.updated).count()};
      item->setText(int(column_e::read), qformat("{:%Y-%m-%d} ({} d)", *row.updated, days));
      item->setData(int(column_e::read), sort_role, qlonglong(row.updated->time_since_epoch().count()));
      }
    item->setData(0, market_role, qulonglong(row.market_id));
    item->setData(0, system_role, QString::fromStdString(row.system));
    system->setData(0, Qt::UserRole, QString::fromStdString(row.system));
    }
  for(auto const & [address, system]: systems)
    {
    auto const [to_update, all]{counts[address]};
    system->setText(
      int(column_e::place),
      qformat("{} - {} of {} to update", system->data(0, Qt::UserRole).toString().toStdString(), to_update, all)
    );
    system->setData(int(column_e::place), sort_role, system->data(0, Qt::UserRole));
    }
  view_->setSortingEnabled(true);
  note_->setText(qformat(
    "{} never read, {} changed hands since the reading - in the systems our commanders claimed and still hold. "
    "A place seen in no journal is not known here.",
    never,
    changed
  ));
  for(int c{}; c != view_->columnCount(); ++c)
    view_->resizeColumnToContents(c);
  }

trade_window_t::trade_window_t(current_state_t const & state, std::string db_path, QWidget * parent) :
    QMdiSubWindow{parent},
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("trade window: failed to open {}", db_path);

  auto * root{new QWidget(this)};
  auto * layout{new QVBoxLayout(root)};
  tabs_ = new QTabWidget(root);
  layout->addWidget(tabs_);

  search_ = new buy_search_tab_t(state, db_, tabs_);
  tabs_->addTab(search_, "Search");

  // --- the market of one station: clicked in the System info window, in Search or in To update ---
  market_page_ = new QWidget(tabs_);
  auto * market_layout{new QVBoxLayout(market_page_)};
  market_header_ = new QLabel("Click a station in System info, Search or To update to see its market.");
  market_layout->addWidget(market_header_);
  auto * market_splitter{new QSplitter(Qt::Vertical, market_page_)};
  market_splitter->setObjectName("trade_market");
  auto make_market_side = [this, market_splitter](bool station_sells, char const * caption) -> QTableView *
  {
    auto * container{new QWidget()};
    auto * side_layout{new QVBoxLayout(container)};
    side_layout->addWidget(new QLabel(caption));

    auto * model{new market_model_t(station_sells, this)};
    auto * proxy{new QSortFilterProxyModel(this)};
    proxy->setSourceModel(model);
    proxy->setSortRole(market_model_t::sort_role);

    auto * view{new QTableView()};
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
  tabs_->addTab(market_page_, "Current Market");

  to_update_ = new to_update_tab_t(state, db_, tabs_);
  tabs_->addTab(to_update_, "To update");

  auto const open_market = [this](uint64_t market_id, QString const & system)
  { show_market_of(market_id, system.toStdString()); };
  connect(search_, &buy_search_tab_t::market_clicked, this, open_market);
  connect(to_update_, &to_update_tab_t::market_clicked, this, open_market);

  setWidget(root);
  setWindowTitle("Trade");
  resize(980, 760);
  }

auto trade_window_t::clear_market() -> void
  {
  market_sells_model_->update_data({});
  market_buys_model_->update_data({});
  if(auto const index{tabs_->indexOf(market_page_)}; index >= 0)
    tabs_->setCurrentIndex(index);
  }

auto trade_window_t::show_market(uint64_t system_address, std::string_view station_name) -> void
  {
  clear_market();
  auto station{db_.load_station(system_address, station_name)};
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
  show_market_of(**station, {});
  }

auto trade_window_t::show_market_of(uint64_t market_id, std::string_view system) -> void
  {
  clear_market();
  auto station{db_.load_station(market_id)};
  if(not station or not *station)
    {
    spdlog::error("failed to load station {}", market_id);
    return;
    }
  show_market_of(**station, system);
  }

auto trade_window_t::show_market_of(info::station_t const & station, std::string_view system) -> void
  {
  auto entries{db_.load_market_entries(station.market_id)};
  if(not entries)
    {
    spdlog::error("failed to load market {}", station.market_id);
    return;
    }

  if(entries->empty())
    {
    market_header_->setText(qformat("{} - no market recorded, dock there with the tool running", station.name));
    return;
    }

  // a station reached from another system's list says which system it is in
  std::string const name{system.empty() ? station.name : std::format("{} | {}", system, station.name)};

  // the time of the reading lives with the market, because the reading itself cannot be rebuilt
  auto reading{db_.load_market_info(station.market_id)};
  if(reading and *reading)
    market_header_->setText(qformat("{} - prices as of {:%Y-%m-%d %H:%M}", name, (*reading)->updated));
  else
    market_header_->setText(QString::fromStdString(name));

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
