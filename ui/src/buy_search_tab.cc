#include <buy_search_tab.h>

#include <databse_storage.h>
#include <eht_settings.h>
#include <format_credits.h>
#include <logic.h>
#include <qformat.h>

#include <qboxlayout.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qcompleter.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <qtablewidget.h>
#include <qtreewidget.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <mutex>

namespace
  {
constexpr std::size_t most_wanted{9u};
constexpr std::size_t most_pairs{3u};
///\brief the value a column sorts by - the text alone sorts 10 before 9
constexpr int sort_role{Qt::UserRole + 1};
constexpr int market_role{Qt::UserRole + 2};
constexpr int system_role{Qt::UserRole + 3};

enum struct column_e : int
  {
  hits,
  place,
  type,
  pad,
  ly,
  jumps,
  ls,
  cost,
  balance,
  age,
  source,
  column_max
  };

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
    // an unknown distance or cost goes after every known one in the ascending order
    return a.isValid();
    }
  };

auto set(QTreeWidgetItem * item, column_e column, QString const & text, QVariant const & sort) -> void
  {
  item->setText(int(column), text);
  item->setData(int(column), sort_role, sort);
  }

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

[[nodiscard]]
auto signed_credits(int64_t value) -> QString
  {
  return qformat("{}{} Cr", value < 0 ? "-" : "", format_credits_value(uint64_t(value < 0 ? -value : value)));
  }
  }  // namespace

buy_search_tab_t::buy_search_tab_t(current_state_t const & state, database_storage_t & db, QWidget * parent) :
    QWidget{parent},
    state_{state},
    db_{db}
  {
  auto * layout{new QVBoxLayout(this)};

  // what is wanted: a name picked from the commodities ever seen at a market, and how much of it
  auto * pick_row{new QHBoxLayout()};
  commodity_edit_ = new QLineEdit();
  commodity_edit_->setPlaceholderText("Commodity - type and pick");
  QStringList names;
  if(auto known{db_.load_commodity_names()}; known)
    for(auto const & [key, name]: *known)
      names.push_back(QString::fromStdString(name));
  else
    spdlog::error("buy search: failed to load the commodity names");
  auto * completer{new QCompleter(names, commodity_edit_)};
  completer->setCaseSensitivity(Qt::CaseInsensitive);
  completer->setFilterMode(Qt::MatchContains);
  commodity_edit_->setCompleter(completer);
  pick_row->addWidget(commodity_edit_, 1);
  auto * from_missions{new QPushButton("From my missions")};
  from_missions->setToolTip("What the open missions still need, in their amounts");
  pick_row->addWidget(from_missions);
  auto * clear{new QPushButton("Clear")};
  pick_row->addWidget(clear);
  layout->addLayout(pick_row);

  wanted_view_ = new QTableWidget(0, 3);
  wanted_view_->setHorizontalHeaderLabels({"Commodity", "Tonnes", ""});
  wanted_view_->verticalHeader()->hide();
  wanted_view_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  wanted_view_->setMaximumHeight(170);
  wanted_view_->setToolTip("Tonnes 0: any stock at all");
  layout->addWidget(wanted_view_);

  // where to look
  auto * filter_row{new QHBoxLayout()};
  auto const check = [&](char const * text, bool on)
  {
    auto * box{new QCheckBox(text)};
    box->setChecked(on);
    filter_row->addWidget(box);
    connect(box, &QCheckBox::toggled, this, [this](bool) { search(); });
    return box;
  };
  space_stations_ = check("Space stations", true);
  outposts_ = check("Outposts", true);
  surface_ports_ = check("Surface ports", true);
  settlements_ = check("Settlements", true);
  // a carrier moves away, and its market with it
  carriers_ = check("Carriers", false);
  filter_row->addSpacing(12);
  filter_row->addWidget(new QLabel("Pad at least"));
  min_pad_ = new QComboBox();
  min_pad_->addItem("Any", int(buy_search::pad_e::none));
  min_pad_->addItem("S", int(buy_search::pad_e::small));
  min_pad_->addItem("M", int(buy_search::pad_e::medium));
  min_pad_->addItem("L", int(buy_search::pad_e::large));
  connect(min_pad_, &QComboBox::currentIndexChanged, this, [this](int) { search(); });
  filter_row->addWidget(min_pad_);
  unknown_pads_ = check("Unknown pads", true);
  unknown_pads_->setToolTip("Places whose pads were never read - docking there once tells them");
  eddn_prices_ = check("EDDN prices", true);
  eddn_prices_->setToolTip("Markets other commanders' tools sent to EDDN, heard around our systems");
  filter_row->addStretch(1);
  layout->addLayout(filter_row);

  auto * second_row{new QHBoxLayout()};
  selling_ = new QCheckBox("Stations buying");
  selling_->setToolTip(
    "Where to sell them: the stations buying them, as much as they want. A station takes back what it sells too, "
    "at a loss - that is no market for it and is left out"
  );
  second_row->addWidget(selling_);
  second_row->addSpacing(12);
  second_row->addWidget(new QLabel("Prices at most"));
  max_age_ = new QSpinBox();
  max_age_->setRange(1, 3650);
  max_age_->setSuffix(" days old");
  max_age_->setValue(int(eht::settings()->windows.buy_max_age_d));
  connect(max_age_, &QSpinBox::valueChanged, this, [this](int) { search(); });
  second_row->addWidget(max_age_);
  second_row->addSpacing(12);
  second_row->addWidget(new QLabel("At most"));
  max_ly_ = new QSpinBox();
  max_ly_->setRange(0, 100'000);
  max_ly_->setSuffix(" ly");
  max_ly_->setSpecialValueText("any ly");
  max_ly_->setValue(int(eht::settings()->windows.buy_max_ly));
  max_ly_->setToolTip("From the system you are in; a place of unknown distance is left out too. 0: no limit");
  connect(max_ly_, &QSpinBox::valueChanged, this, [this](int) { search(); });
  second_row->addWidget(max_ly_);
  second_row->addSpacing(12);
  second_row->addWidget(new QLabel("Mission reward"));
  reward_ = new QSpinBox();
  reward_->setRange(0, std::numeric_limits<int>::max());
  reward_->setSingleStep(10'000);
  reward_->setSuffix(" Cr");
  reward_->setGroupSeparatorShown(true);
  reward_->setToolTip("The reward offered on the mission board - the balance column is the reward less the cost");
  connect(reward_, &QSpinBox::valueChanged, this, [this](int) { search(); });
  second_row->addWidget(reward_);
  second_row->addStretch(1);
  layout->addLayout(second_row);

  note_ = new QLabel();
  note_->setWordWrap(true);
  layout->addWidget(note_);

  QStringList const headers{
    "Hits", "System | Station", "Type", "Pad", "ly", "Jumps", "Ls", "Cost", "Balance", "Age", "Source"
  };

  pairs_caption_ = new QLabel("No single place has everything - pairs that together do, the shortest way first:");
  layout->addWidget(pairs_caption_);
  pairs_view_ = new QTreeWidget();
  pairs_view_->setColumnCount(int(column_e::column_max));
  pairs_view_->setHeaderLabels(headers);
  pairs_view_->setMaximumHeight(200);
  layout->addWidget(pairs_view_);

  places_view_ = new QTreeWidget();
  places_view_->setColumnCount(int(column_e::column_max));
  places_view_->setHeaderLabels(headers);
  places_view_->setSortingEnabled(true);
  places_view_->header()->setSectionResizeMode(QHeaderView::Interactive);
  layout->addWidget(places_view_, 1);

  auto const clicked = [this](QTreeWidgetItem * item, int)
  {
    // a commodity's line belongs to the place above it
    while(item != nullptr and not item->data(0, market_role).isValid())
      item = item->parent();
    if(item != nullptr)
      emit market_clicked(item->data(0, market_role).toULongLong(), item->data(0, system_role).toString());
  };
  connect(places_view_, &QTreeWidget::itemDoubleClicked, this, clicked);
  connect(pairs_view_, &QTreeWidget::itemDoubleClicked, this, clicked);

  connect(
    commodity_edit_,
    &QLineEdit::returnPressed,
    this,
    [this]()
    {
      add_wanted(commodity_edit_->text().trimmed(), 0u);
      commodity_edit_->clear();
    }
  );
  connect(
    completer,
    qOverload<QString const &>(&QCompleter::activated),
    this,
    [this](QString const & name)
    {
      add_wanted(name, 0u);
      // the line edit takes the completion after this, so it is cleared once the event is done
      QMetaObject::invokeMethod(commodity_edit_, &QLineEdit::clear, Qt::QueuedConnection);
    }
  );
  connect(from_missions, &QPushButton::clicked, this, [this]() { add_from_missions(); });
  connect(
    selling_,
    &QCheckBox::toggled,
    this,
    [this](bool on)
    {
      // a reward pays for what is bought for a mission; selling earns by itself
      reward_->setEnabled(not on);
      for(QTreeWidget * view: {places_view_, pairs_view_})
        view->headerItem()->setText(int(column_e::cost), on ? "Earns" : "Cost");
      search();
    }
  );
  connect(
    clear,
    &QPushButton::clicked,
    this,
    [this]()
    {
      wanted_view_->setRowCount(0);
      search();
    }
  );
  search();
  }

auto buy_search_tab_t::showEvent(QShowEvent * event) -> void
  {
  QWidget::showEvent(event);
  search();
  }

auto buy_search_tab_t::add_wanted(QString const & commodity, uint32_t tonnes) -> void
  {
  if(commodity.isEmpty())
    return;
  for(int row{}; row != wanted_view_->rowCount(); ++row)
    if(wanted_view_->item(row, 0)->text().compare(commodity, Qt::CaseInsensitive) == 0)
      {
      if(tonnes != 0u)
        qobject_cast<QSpinBox *>(wanted_view_->cellWidget(row, 1))->setValue(int(tonnes));
      return;
      }
  if(std::size_t(wanted_view_->rowCount()) >= most_wanted)
    {
    note_->setText(qformat("At most {} commodities at once.", most_wanted));
    return;
    }

  int const row{wanted_view_->rowCount()};
  wanted_view_->insertRow(row);
  auto * name{new QTableWidgetItem(commodity)};
  name->setFlags(name->flags() & ~Qt::ItemIsEditable);
  wanted_view_->setItem(row, 0, name);
  auto * amount{new QSpinBox()};
  amount->setRange(0, 100'000);
  amount->setSuffix(" t");
  amount->setValue(int(tonnes));
  connect(amount, &QSpinBox::valueChanged, this, [this](int) { search(); });
  wanted_view_->setCellWidget(row, 1, amount);
  auto * remove{new QPushButton("Remove")};
  connect(
    remove,
    &QPushButton::clicked,
    this,
    [this, name]()
    {
      wanted_view_->removeRow(wanted_view_->row(name));
      search();
    }
  );
  wanted_view_->setCellWidget(row, 2, remove);
  search();
  }

auto buy_search_tab_t::add_from_missions() -> void
  {
  auto needs{db_.load_cargo_needs()};
  if(not needs)
    {
    spdlog::error("buy search: failed to load what the missions need");
    return;
    }
  if(needs->empty())
    note_->setText("No open mission needs a commodity.");
  for(info::cargo_need_t const & need: *needs)
    add_wanted(QString::fromStdString(need.commodity), need.count);
  }

auto buy_search_tab_t::wanted() const -> std::vector<buy_search::wanted_t>
  {
  std::vector<buy_search::wanted_t> list;
  for(int row{}; row != wanted_view_->rowCount(); ++row)
    list.push_back(
      buy_search::wanted_t{
        .commodity = wanted_view_->item(row, 0)->text().toStdString(),
        .tonnes = uint32_t(qobject_cast<QSpinBox *>(wanted_view_->cellWidget(row, 1))->value())
      }
    );
  return list;
  }

auto buy_search_tab_t::filter() const -> buy_search::filter_t
  {
  using enum buy_search::place_e;
  buy_search::filter_t result{
    .places = {},
    .min_pad = buy_search::pad_e(min_pad_->currentData().toInt()),
    .unknown_pads = unknown_pads_->isChecked(),
    .max_age = std::chrono::days{max_age_->value()},
    .eddn = eddn_prices_->isChecked(),
    .max_ly = max_ly_->value() > 0 ? std::optional<double>{max_ly_->value()} : std::nullopt
  };
  for(auto const & [box, place]: {std::pair{space_stations_, space_station},
                                  std::pair{outposts_, outpost},
                                  std::pair{surface_ports_, surface_port},
                                  std::pair{settlements_, settlement},
                                  std::pair{carriers_, carrier}})
    if(box->isChecked())
      result.places.insert(place);
  return result;
  }

auto buy_search_tab_t::search() -> void
  {
  places_view_->clear();
  pairs_view_->clear();
  pairs_view_->hide();
  pairs_caption_->hide();

  bool const selling{selling_->isChecked()};
  std::vector<buy_search::wanted_t> const wanted_now{wanted()};
  if(wanted_now.empty())
    {
    note_->setText(qformat(
      "Pick the commodities to {} - the places with the most of them come first.", selling ? "sell" : "buy"
    ));
    return;
    }

  std::vector<std::string> names;
  for(buy_search::wanted_t const & w: wanted_now)
    names.push_back(w.commodity);
  auto rows{selling ? db_.load_sell_offers(names) : db_.load_buy_offers(names)};
  if(not rows)
    {
    spdlog::error("buy search: failed to load the markets");
    note_->setText("The markets could not be read - see the log.");
    return;
    }

  std::optional<std::array<double, 3>> here;
  {
  std::scoped_lock const lock{state_.mutex_};
  if(state_.system.system_address != 0u)
    here = state_.system.system_location;
  jump_range_ = state_.ship_loadout.MaxJumpRange;
  }

  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  std::vector<buy_search::place_t> const ranked{
    buy_search::rank(buy_search::places_of(*rows, here), wanted_now, filter(), now)
  };
  note_->setText(
    ranked.empty() ? qformat("No market read {} any of them - the filters may leave the rest out.", selling ? "buys" : "has")
                   : qformat("{} places, from the markets opened with the tool running and heard on EDDN.", ranked.size())
  );
  show_places(ranked, wanted_now);
  show_pairs(ranked, wanted_now);
  }

namespace
  {
[[nodiscard]]
auto place_item(
  buy_search::place_t const & place,
  std::vector<buy_search::wanted_t> const & wanted,
  float jump_range,
  int64_t reward,
  bool selling,
  std::chrono::sys_seconds now
) -> QTreeWidgetItem *
  {
  auto * item{new sorted_item_t()};
  unsigned const hits{buy_search::hits(place, wanted)};
  set(item, column_e::hits, qformat("{}/{}", hits, wanted.size()), hits);
  set(
    item,
    column_e::place,
    qformat("{} | {}", place.system, place.station),
    QString::fromStdString(std::format("{} | {}", place.system, place.station))
  );
  set(item, column_e::type, QString::fromStdString(place.station_type), QString::fromStdString(place.station_type));
  set(item, column_e::pad, pad_name(place.pad), place.pad ? QVariant{int(*place.pad)} : QVariant{});
  if(place.ly)
    {
    set(item, column_e::ly, qformat("{:.1f}", *place.ly), *place.ly);
    if(auto const jumps{buy_search::jumps(*place.ly, jump_range)}; jumps)
      set(item, column_e::jumps, qformat("{}", *jumps), *jumps);
    }
  set(item, column_e::ls, qformat("{:.0f}", place.dist_ls), place.dist_ls);
  if(auto const cost{buy_search::cost(place, wanted)}; cost)
    {
    set(item, column_e::cost, signed_credits(*cost), qlonglong(*cost));
    if(reward > 0)
      set(item, column_e::balance, signed_credits(reward - *cost), qlonglong(reward - *cost));
    }
  auto const age{std::chrono::duration_cast<std::chrono::hours>(now - place.updated)};
  set(
    item,
    column_e::age,
    age < std::chrono::hours{48} ? qformat("{} h", age.count()) : qformat("{} d", age.count() / 24),
    qlonglong(age.count())
  );
  QString const source{place.source == "eddn" ? "EDDN" : "own"};
  set(item, column_e::source, source, source);
  item->setData(0, market_role, qulonglong(place.market_id));
  item->setData(0, system_role, QString::fromStdString(place.system));

  // one line under the place for each commodity wanted: what it costs there, or why it does not count
  for(buy_search::wanted_t const & w: wanted)
    {
    auto * line{new QTreeWidgetItem(item)};
    auto const offer{std::ranges::find_if(
      place.offers,
      [&](buy_search::offer_t const & o)
      {
        return QString::fromStdString(o.commodity).compare(QString::fromStdString(w.commodity), Qt::CaseInsensitive)
               == 0;
      }
    )};
    if(offer == place.offers.end())
      line->setText(0, qformat("{} - not {} here", w.commodity, selling ? "bought" : "sold"));
    else
      {
      double const deviation{
        offer->mean_price > 0 ? 100.0 * double(offer->buy_price - offer->mean_price) / double(offer->mean_price) : 0.0
      };
      line->setText(
        0,
        qformat(
          "{} - {} Cr, {} t {}, {:+.0f}% against the average{}",
          offer->commodity,
          format_credits_value(uint64_t(offer->buy_price)),
          offer->stock,
          selling ? "wanted" : "in stock",
          deviation,
          buy_search::covers(*offer, w)
            ? ""
            : selling ? std::format(" - wants fewer than the {} t to sell", w.tonnes)
                      : std::format(" - fewer than the {} t wanted", w.tonnes)
        )
      );
      }
    }
  return item;
  }
  }  // namespace

auto buy_search_tab_t::show_places(
  std::vector<buy_search::place_t> const & ranked, std::vector<buy_search::wanted_t> const & wanted
) -> void
  {
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  // the ranking is the order until a header is clicked - sorting off while filling keeps it
  int const column{places_view_->sortColumn()};
  Qt::SortOrder const order{places_view_->header()->sortIndicatorOrder()};
  places_view_->setSortingEnabled(false);
  for(buy_search::place_t const & place: ranked)
    {
    bool const selling{selling_->isChecked()};
    places_view_->addTopLevelItem(
      place_item(place, wanted, jump_range_, selling ? 0 : reward_->value(), selling, now)
    );
    for(int i{}; i != places_view_->topLevelItem(places_view_->topLevelItemCount() - 1)->childCount(); ++i)
      places_view_->topLevelItem(places_view_->topLevelItemCount() - 1)->child(i)->setFirstColumnSpanned(true);
    }
  // the line per commodity is what tells the places apart - open at once
  places_view_->expandAll();
  places_view_->setSortingEnabled(true);
  if(column > 0)
    places_view_->sortItems(column, order);
  else
    places_view_->header()->setSortIndicator(-1, Qt::AscendingOrder);
  for(int c{}; c != int(column_e::column_max); ++c)
    places_view_->resizeColumnToContents(c);
  }

auto buy_search_tab_t::show_pairs(
  std::vector<buy_search::place_t> const & ranked, std::vector<buy_search::wanted_t> const & wanted
) -> void
  {
  auto const pairs{buy_search::covering_pairs(ranked, wanted, most_pairs)};
  if(pairs.empty())
    return;
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  for(buy_search::pair_t const & pair: pairs)
    {
    auto * item{new QTreeWidgetItem()};
    buy_search::place_t const & a{ranked[pair.first]};
    buy_search::place_t const & b{ranked[pair.second]};
    item->setText(int(column_e::hits), qformat("{0}/{0}", wanted.size()));
    item->setText(int(column_e::place), qformat("{} + {}", a.station, b.station));
    item->setText(int(column_e::ly), qformat("{:.1f}", pair.ly));
    if(auto const jumps{buy_search::jumps(pair.ly, jump_range_)}; jumps)
      item->setText(int(column_e::jumps), qformat("{}", *jumps));
    pairs_view_->addTopLevelItem(item);
    item->addChild(place_item(a, wanted, jump_range_, 0, selling_->isChecked(), now));
    item->addChild(place_item(b, wanted, jump_range_, 0, selling_->isChecked(), now));
    for(int i{}; i != item->childCount(); ++i)
      for(int j{}; j != item->child(i)->childCount(); ++j)
        item->child(i)->child(j)->setFirstColumnSpanned(true);
    item->setExpanded(true);
    }
  pairs_view_->expandAll();
  for(int c{}; c != int(column_e::column_max); ++c)
    pairs_view_->resizeColumnToContents(c);
  pairs_caption_->show();
  pairs_view_->show();
  }
