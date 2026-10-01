#include <construction_window.h>
#include <data/carrier.h>
#include <data/colonisation.h>
#include <data/market.h>
#include <qformat.h>
#include <commodity_facts.h>

#include <qicon.h>
#include <qpainter.h>
#include <qpixmap.h>

#include <qboxlayout.h>
#include <qheaderview.h>
#include <qsettings.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <tuple>

namespace
  {
[[nodiscard]]
auto supplier_setting(uint64_t site_market) -> QString
  { return QString::fromStdString(std::format("construction/carrier/{}", site_market)); }
  }  // namespace

auto construction_supplier(uint64_t site_market) -> uint64_t
  {
  if(site_market == 0u)
    return 0u;
  return QSettings{"ebasoft", "EliteHelpTool"}.value(supplier_setting(site_market)).toULongLong();
  }

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
  auto * supply = new QHBoxLayout();
  supply->addWidget(new QLabel("Supplied from:", central));
  carrier_combo_ = new QComboBox(central);
  carrier_combo_->setToolTip("The carrier this site is supplied from - the Diff column compares its cargo with what is left");
  carrier_combo_->addItem("all our carriers", QVariant::fromValue(qulonglong{0}));
  if(auto carriers{db_.load_carriers()}; carriers)
    for(info::carrier_t const & c: *carriers)
      if(c.tracked or c.carrier_type == "SquadronCarrier")
        carrier_combo_->addItem(
          qformat("{} ({})", c.carrier_name, c.carrier_id), QVariant::fromValue(qulonglong{c.market_id})
        );
  supply->addWidget(carrier_combo_, 1);
  layout->addLayout(supply);
  header_ = new QLabel(central);
  header_->setWordWrap(true);
  layout->addWidget(header_);

  table_ = new QTableWidget(central);
  table_->setColumnCount(9);
  // short captions in a small font - the numbers beneath are narrow, and long captions widened every
  // column; the full meaning is in the tooltips
  table_->setHorizontalHeaderLabels(
    {"Commodity", "Left", "Diff", "Req.", "Given", "Hold", "Carriers", "Here", "Price"}
  );
  QStringList const tips{
    "Commodity",
    "Still to deliver",
    "The chosen carrier's cargo (or all our carriers') and the ship's hold, less what is still to deliver - below zero is what is "
    "still to be brought, above what is to spare",
    "Required in all",
    "Provided so far",
    "In the ship's hold",
    "On our carriers",
    "For sale at the port you stand at",
    "Price at the port you stand at"
  };
  for(int column{}; column != tips.size(); ++column)
    table_->horizontalHeaderItem(column)->setToolTip(tips[column]);
  QFont small{table_->horizontalHeader()->font()};
  small.setPointSizeF(small.pointSizeF() * 0.8);
  table_->horizontalHeader()->setFont(small);
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

  connect(
    site_combo_,
    &QComboBox::currentIndexChanged,
    this,
    [this](int)
    {
      restore_carrier();
      show_site();
    }
  );
  connect(
    carrier_combo_,
    &QComboBox::currentIndexChanged,
    this,
    [this](int)
    {
      if(uint64_t const market{selected_market()}; market != 0u)
        QSettings{"ebasoft", "EliteHelpTool"}.setValue(supplier_setting(market), carrier_combo_->currentData());
      show_site();
    }
  );
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

auto construction_window_t::restore_carrier() -> void
  {
  uint64_t const supplier{construction_supplier(selected_market())};
  int const index{carrier_combo_->findData(QVariant::fromValue(qulonglong{supplier}))};
  QSignalBlocker const block{carrier_combo_};
  carrier_combo_->setCurrentIndex(index < 0 ? 0 : index);
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
  auto const listed = [](std::vector<info::construction_site_t> const & sites, uint64_t market)
  { return std::ranges::contains(sites, market, [](info::construction_site_t const & s) { return s.depot.market_id; }); };
  // the chosen site gone from the list, and not for an abandoned mark: it was finished
  uint64_t finished{};
  if(keep != 0u and not listed(sites_, keep))
    {
    if(show_abandoned_->isChecked())
      finished = keep;
    else if(auto const all{db_.load_construction_sites(true)}; all and not listed(*all, keep))
      finished = keep;
    }
  if(keep == 0u)
    keep = state_.settlement_market_id_;
  QSignalBlocker const block{site_combo_};
  site_combo_->clear();
  for(info::construction_site_t const & site: sites_)
    site_combo_->addItem(
      qformat(
        "{}  -  {}  ({:.0f}%, seen {:%Y-%m-%d}){}",
        info::shown_name(site),
        site.system,
        site.depot.progress * 100.0,
        site.depot.updated,
        site.abandoned ? "  [abandoned]" : ""
      ),
      QVariant::fromValue(qulonglong{site.depot.market_id})
    );
  if(auto const index{site_combo_->findData(QVariant::fromValue(qulonglong{keep}))}; index >= 0)
    site_combo_->setCurrentIndex(index);
  restore_carrier();
  show_site();
  if(finished != 0u)
    {
    spdlog::info("construction window: site {} is under way no more", finished);
    emit site_finished(finished);
    }
  }

namespace
  {
///\brief the colour of an economy, as colonisation planners draw it
[[nodiscard]]
auto economy_colour(commodity_facts::economy_e e) -> QColor
  {
  using commodity_facts::economy_e;
  switch(e)
    {
    case economy_e::agriculture: return QColor{0x7a, 0xcc, 0x00};
    case economy_e::high_tech:   return QColor{0x00, 0xcc, 0xcc};
    case economy_e::industrial:  return QColor{0x99, 0x99, 0x00};
    case economy_e::military:    return QColor{0xbb, 0x00, 0xbb};
    case economy_e::refinery:    return QColor{0xcc, 0x66, 0x00};
    case economy_e::extraction:  return QColor{0xcc, 0x33, 0x33};
    }
  return Qt::gray;
  }

///\brief a square in the colours of the economies producing a commodity - one colour whole, several cut
/// along the diagonal; a dot in the corner for what only ports on the ground produce
[[nodiscard]]
auto economy_icon(commodity_facts::fact_t const & fact) -> QIcon
  {
  using commodity_facts::economy_e;
  std::vector<QColor> colours;
  for(economy_e const e:
      {economy_e::agriculture,
       economy_e::high_tech,
       economy_e::industrial,
       economy_e::military,
       economy_e::refinery,
       economy_e::extraction})
    if((uint8_t(fact.produced_by) & uint8_t(e)) != 0u)
      colours.push_back(economy_colour(e));
  constexpr int side{16};
  QPixmap pixmap{side, side};
  pixmap.fill(Qt::transparent);
  QPainter painter{&pixmap};
  painter.setPen(Qt::NoPen);
  if(colours.size() <= 1u)
    painter.fillRect(0, 0, side, side, colours.empty() ? QColor{Qt::gray} : colours.front());
  else
    {
    // bands across the diagonal, one per economy
    double const band{2.0 * side / double(colours.size())};
    for(size_t i{}; i != colours.size(); ++i)
      {
      double const from{band * double(i)};
      double const to{band * double(i + 1u)};
      QPolygonF band_shape;
      band_shape << QPointF{from, 0} << QPointF{to, 0} << QPointF{to - side, double(side)}
                 << QPointF{from - side, double(side)};
      painter.setBrush(colours[i]);
      painter.setClipRect(0, 0, side, side);
      painter.drawPolygon(band_shape);
      }
    }
  if(fact.surface)
    {
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPointF{side - 4.0, side - 4.0}, 3.0, 3.0);
    }
  return QIcon{pixmap};
  }

///\brief who produces a commodity, in words - for the tooltip of its square
[[nodiscard]]
auto producers_text(commodity_facts::fact_t const & fact) -> QString
  {
  using commodity_facts::economy_e;
  constexpr std::array<std::pair<economy_e, char const *>, 6> names{{
    {economy_e::agriculture, "Agriculture"},
    {economy_e::high_tech, "High Tech"},
    {economy_e::industrial, "Industrial"},
    {economy_e::military, "Military"},
    {economy_e::refinery, "Refinery"},
    {economy_e::extraction, "Extraction"},
  }};
  QStringList economies;
  for(auto const & [economy, name]: names)
    if((uint8_t(fact.produced_by) & uint8_t(economy)) != 0u)
      economies << QString::fromLatin1(name);
  QString text{QStringLiteral("Produced by: %1").arg(economies.join(QStringLiteral(", ")))};
  if(fact.surface)
    text += QStringLiteral("\nonly at ports on the ground");
  else if(fact.orbital)
    text += QStringLiteral("\nonly at ports in orbit");
  return text;
  }
  }  // namespace

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
        here[entry.key.empty() ? info::commodity_key(entry.name) : entry.key] = std::move(entry);

  // what our carriers hold, together - kept by hand and by every docking's balance
  std::map<std::string, int64_t> carriers;
  if(auto totals{db_.load_carrier_cargo_totals()}; totals)
    carriers = std::move(*totals);
  // and the one this site is supplied from, alone
  uint64_t const supplier{carrier_combo_->currentData().toULongLong()};
  // with none chosen, all our carriers together - as on the overlay
  std::map<std::string, int64_t> supplier_cargo{supplier == 0u ? carriers : std::map<std::string, int64_t>{}};
  if(supplier != 0u)
    if(auto cargo{db_.load_carrier_cargo(supplier)}; cargo)
      for(info::carrier_cargo_t const & item: *cargo)
        supplier_cargo[item.key] += item.count;
  // the hold counts as well - bought for the carrier or for the site, it is on its way. Docked at that
  // carrier, its cargo is booked only on leaving, so until then the hold as it was on docking stands in
  // for the hold now, or what was moved across would be counted twice
  std::map<std::string, int64_t> on_board;
  if(state_.carrier_visit_ and (supplier == 0u or state_.carrier_visit_->carrier_id == supplier))
    for(auto const & [key, item]: state_.carrier_visit_->hold)
      on_board[key] += item.second;
  else
    for(auto const & [key, count]: hold)
      on_board[key] += count;

  // by type, then by name - the way the game's own list reads; every type opens with a row of its own
  std::map<std::string, std::string> categories;
  if(auto known{db_.load_commodity_categories()}; known)
    categories = std::move(*known);
  auto const category_of = [&](info::construction_need_t const & need) -> std::string
  {
    if(auto const found{categories.find(need.key)}; found != categories.end())
      return found->second;
    if(auto const known{commodity_facts::category_of(need.key)}; known)
      return std::string{*known};
    return "Other";
  };
  std::vector<info::construction_need_t const *> wanted;
  uint64_t left_total{};
  uint64_t required_total{};
  int done{};
  for(info::construction_need_t const & need: it->needs)
    {
    required_total += need.required;
    if(need.required <= need.provided)
      {
      ++done;
      continue;
      }
    left_total += need.required - need.provided;
    wanted.push_back(&need);
    }
  std::ranges::sort(
    wanted,
    [&](info::construction_need_t const * a, info::construction_need_t const * b)
    { return info::needs_before(category_of(*a), a->commodity, category_of(*b), b->commodity); }
  );

  auto const number = [](uint64_t v) { return QString::number(qulonglong(v)); };
  QColor const lacking{0xcc, 0x44, 0x44};
  QColor const enough{0x44, 0xaa, 0x44};
  auto const diff_cell = [&](int64_t diff)
  {
    auto * cell = new QTableWidgetItem(diff > 0 ? QStringLiteral("+%1").arg(diff) : QString::number(diff));
    cell->setForeground(diff < 0 ? lacking : enough);
    return cell;
  };
  int64_t lacking_total{};
  QBrush const band{palette().color(QPalette::Highlight)};
  QBrush const band_text{palette().color(QPalette::HighlightedText)};
  std::string last_category;
  for(info::construction_need_t const * need: wanted)
    {
    if(std::string const category{category_of(*need)}; category != last_category)
      {
      last_category = category;
      int const row{table_->rowCount()};
      table_->insertRow(row);
      for(int column{}; column != table_->columnCount(); ++column)
        {
        auto * cell = new QTableWidgetItem(column == 0 ? QString::fromStdString(category) : QString{});
        cell->setBackground(band);
        cell->setForeground(band_text);
        table_->setItem(row, column, cell);
        }
      }
    int const row{table_->rowCount()};
    table_->insertRow(row);
    uint32_t const left{need->required - need->provided};
    auto * name = new QTableWidgetItem(QString::fromStdString(need->commodity));
    if(auto const fact{commodity_facts::find(need->key)}; fact)
      {
      name->setIcon(economy_icon(*fact));
      name->setToolTip(producers_text(*fact));
      }
    table_->setItem(row, 0, name);
    table_->setItem(row, 1, new QTableWidgetItem(number(left)));
      {
      auto const s{supplier_cargo.find(need->key)};
      auto const b{on_board.find(need->key)};
      int64_t const diff{
        (s == supplier_cargo.end() ? 0 : s->second) + (b == on_board.end() ? 0 : b->second) - int64_t(left)
      };
      lacking_total += std::min<int64_t>(diff, 0);
      table_->setItem(row, 2, diff_cell(diff));
      }
    table_->setItem(row, 3, new QTableWidgetItem(number(need->required)));
    table_->setItem(row, 4, new QTableWidgetItem(number(need->provided)));
    auto const h{hold.find(need->key)};
    table_->setItem(row, 5, new QTableWidgetItem(h == hold.end() ? QString{} : number(h->second)));
    auto const c{carriers.find(need->key)};
    table_->setItem(
      row, 6, new QTableWidgetItem(c == carriers.end() or c->second <= 0 ? QString{} : number(uint64_t(c->second)))
    );
    auto const m{here.find(need->key)};
    bool const sold{m != here.end() and m->second.stock > 0u and m->second.buy_price > 0u};
    table_->setItem(row, 7, new QTableWidgetItem(sold ? number(m->second.stock) : QString{}));
    table_->setItem(row, 8, new QTableWidgetItem(sold ? number(m->second.buy_price) : QString{}));
    }
  if(not wanted.empty())
    {
    int const row{table_->rowCount()};
    table_->insertRow(row);
    auto * total = new QTableWidgetItem("Sum total:");
    total->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table_->setItem(row, 0, total);
    table_->setItem(row, 1, new QTableWidgetItem(number(left_total)));
    // what the carrier still lacks in all - a surplus of one commodity makes up for nothing of another
      {
      auto * cell = diff_cell(lacking_total);
      cell->setToolTip("What is still to be brought, in all");
      table_->setItem(row, 2, cell);
      }
    }
  header_->setText(qformat(
    "{} in {}: {:.1f}% built, {} t of {} t left, {} commodities complete. Updated {:%Y-%m-%d %H:%M} UTC.",
    info::shown_name(*it),
    it->system,
    it->depot.progress * 100.0,
    left_total,
    required_total,
    done,
    it->depot.updated
  ));
  }
