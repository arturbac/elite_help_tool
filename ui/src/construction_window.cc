#include <construction_window.h>
#include <qformat.h>

#include <qboxlayout.h>
#include <qheaderview.h>

#include <spdlog/spdlog.h>

#include <algorithm>

construction_window_t::construction_window_t(current_state_t const & state, std::string db_path, QWidget * parent) :
    QMdiSubWindow(parent),
    state_{state},
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("construction window: failed to open {}", db_path);
  setup_ui();
  refresh_ui(true);
  }

auto construction_window_t::setup_ui() -> void
  {
  setWindowTitle("Construction");
  auto * central = new QWidget(this);
  auto * layout = new QVBoxLayout(central);

  auto * selector = new QHBoxLayout();
  site_combo_ = new QComboBox(central);
  // a long site name must not widen the whole tool - the box keeps a modest width and cuts the name
  site_combo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  site_combo_->setMinimumContentsLength(24);
  selector->addWidget(site_combo_, 1);
  abandon_button_ = new QPushButton("Mark abandoned", central);
  selector->addWidget(abandon_button_);
  show_abandoned_ = new QCheckBox("Show abandoned", central);
  selector->addWidget(show_abandoned_);
  layout->addLayout(selector);
  header_ = new QLabel(central);
  header_->setWordWrap(true);
  layout->addWidget(header_);

  table_ = new QTableWidget(central);
  table_->setColumnCount(8);
  table_->setHorizontalHeaderLabels(
    {"Commodity", "Left", "Required", "Provided", "In hold", "On carriers", "Here", "Price here"}
  );
  table_->verticalHeader()->setVisible(false);
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  // the commodity takes the room there is, the numbers only what they need - the last column, empty at a
  // site with no market, must not be the one stretched
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  table_->horizontalHeader()->setStretchLastSection(false);
  table_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
  layout->addWidget(table_, 1);
  setWidget(central);

  connect(site_combo_, &QComboBox::currentIndexChanged, this, [this](int) { show_site(); });
  connect(show_abandoned_, &QCheckBox::toggled, this, [this](bool) { refresh_ui(true); });
  connect(
    abandon_button_,
    &QPushButton::clicked,
    this,
    [this]
    {
      uint64_t const market{selected_market()};
      if(market == 0u)
        return;
      auto const it{
        std::ranges::find(sites_, market, [](info::construction_site_t const & s) { return s.depot.market_id; })
      };
      bool const abandoned{it == sites_.end() or not it->abandoned};
      if(
        auto res{db_.mark_construction_abandoned(
          market, abandoned, std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())
        )};
        not res
      )
        spdlog::error("construction window: failed to mark {}", market);
      refresh_ui(true);
    }
  );
  }

auto construction_window_t::selected_market() const -> uint64_t
  { return site_combo_->currentIndex() < 0 ? 0u : site_combo_->currentData().toULongLong(); }

auto construction_window_t::refresh_ui(bool force) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(not force and changes_seen_ == state_.construction_changes_ + state_.carrier_changes_
     and now - read_ < std::chrono::seconds{2})
    return;
  read_ = now;
  changes_seen_ = state_.construction_changes_ + state_.carrier_changes_;

  auto loaded{db_.load_construction_sites(show_abandoned_->isChecked())};
  if(not loaded)
    {
    spdlog::error("construction window: failed to load the sites");
    return;
    }
  sites_ = std::move(*loaded);

  // the choice stays where it was; a site docked at is chosen when nothing was
  uint64_t keep{selected_market()};
  if(keep == 0u)
    keep = state_.settlement_market_id_;
  QSignalBlocker const block{site_combo_};
  site_combo_->clear();
  for(info::construction_site_t const & site: sites_)
    site_combo_->addItem(
      qformat(
        "{}  -  {}  ({:.0f}%, seen {:%Y-%m-%d}){}",
        site.name.empty() ? std::format("site {}", site.depot.market_id) : site.name,
        site.system,
        site.depot.progress * 100.0,
        site.depot.updated,
        site.abandoned ? "  [abandoned]" : ""
      ),
      QVariant::fromValue(qulonglong{site.depot.market_id})
    );
  if(auto const index{site_combo_->findData(QVariant::fromValue(qulonglong{keep}))}; index >= 0)
    site_combo_->setCurrentIndex(index);
  show_site();
  }

auto construction_window_t::show_site() -> void
  {
  table_->setRowCount(0);
  uint64_t const market{selected_market()};
  auto const it{
    std::ranges::find(sites_, market, [](info::construction_site_t const & s) { return s.depot.market_id; })
  };
  abandon_button_->setEnabled(it != sites_.end());
  abandon_button_->setText(it != sites_.end() and it->abandoned ? "Not abandoned" : "Mark abandoned");
  if(it == sites_.end())
    {
    header_->setText(
      sites_.empty() ? "No construction under way in our systems. A site is known from the first docking at it."
                     : "Choose a site"
    );
    return;
    }

  // what the hold carries and what the port we stand at sells, by the key the three sources share
  std::map<std::string, uint32_t> hold;
  for(events::cargo_item_t const & item: state_.cargo.Inventory)
    hold[info::commodity_key(item.Name)] += item.Count;
  std::map<std::string, info::market_entry_t> here;
  if(uint64_t const port{state_.settlement_market_id_}; port != 0u and port != market)
    if(auto entries{db_.load_market_entries(port)}; entries)
      for(info::market_entry_t & entry: *entries)
        here[info::commodity_key(entry.name)] = std::move(entry);

  // what our carriers hold, together - kept by hand and by every docking's balance
  std::map<std::string, int64_t> carriers;
  if(auto totals{db_.load_carrier_cargo_totals()}; totals)
    carriers = std::move(*totals);

  uint64_t left_total{};
  uint64_t required_total{};
  int done{};
  for(info::construction_need_t const & need: it->needs)
    {
    required_total += need.required;
    uint32_t const left{need.required > need.provided ? need.required - need.provided : 0u};
    left_total += left;
    if(left == 0u)
      {
      ++done;
      continue;
      }
    int const row{table_->rowCount()};
    table_->insertRow(row);
    auto const number = [](uint64_t v) { return QString::number(qulonglong(v)); };
    table_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(need.commodity)));
    table_->setItem(row, 1, new QTableWidgetItem(number(left)));
    table_->setItem(row, 2, new QTableWidgetItem(number(need.required)));
    table_->setItem(row, 3, new QTableWidgetItem(number(need.provided)));
    auto const h{hold.find(need.key)};
    table_->setItem(row, 4, new QTableWidgetItem(h == hold.end() ? QString{} : number(h->second)));
    auto const c{carriers.find(need.key)};
    table_->setItem(
      row, 5, new QTableWidgetItem(c == carriers.end() or c->second <= 0 ? QString{} : number(uint64_t(c->second)))
    );
    auto const m{here.find(need.key)};
    bool const sold{m != here.end() and m->second.stock > 0u and m->second.buy_price > 0u};
    table_->setItem(row, 6, new QTableWidgetItem(sold ? number(m->second.stock) : QString{}));
    table_->setItem(row, 7, new QTableWidgetItem(sold ? number(m->second.buy_price) : QString{}));
    }
  header_->setText(qformat(
    "{} in {}: {:.1f}% built, {} t of {} t left, {} commodities complete. Updated {:%Y-%m-%d %H:%M} UTC.",
    it->name,
    it->system,
    it->depot.progress * 100.0,
    left_total,
    required_total,
    done,
    it->depot.updated
  ));
  }
