#include <atmospheres_page.h>
#include <databse_storage.h>
#include <eht_settings.h>
#include <event_guard.h>
#include <star_census.h>

#include <array>
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
  "Where to jump for a landable planet of a rare atmosphere. The chance is over the systems scanned in full - as many"
  " stars and planets known as the discovery scan counted - by the class of the arrival star, the one the galaxy map"
  " shows and its star filter switches; set the filter to the classes with the best chance. A class with fewer than"
  " 20 such systems is grey: its chance is weak. The parent star and the parent body tell where such planets sit, over"
  " every system; they cannot be seen before the jump. This account's galaxy and the shared ones."
};

auto make_table(QWidget * parent, QStringList const & labels) -> QTableWidget *
  {
  auto * view{new QTableWidget(parent)};
  view->setColumnCount(int(labels.size()));
  view->setHorizontalHeaderLabels(labels);
  view->verticalHeader()->setVisible(false);
  view->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  view->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  return view;
  }

auto cell(std::string const & text, bool number) -> QTableWidgetItem *
  {
  auto * item{new QTableWidgetItem(QString::fromStdString(text))};
  if(number)
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  return item;
  }

auto percent(double share) -> std::string { return std::format("{:.1f}%", 100.0 * share); }

///\brief with_what: a column telling what the star is, after its class
auto fill_shares(QTableWidget * view, std::vector<atmosphere_odds::share_row_t> const & rows, bool with_what) -> void
  {
  view->setRowCount(int(rows.size()));
  int const at{with_what ? 1 : 0};
  for(auto const & [index, row]: rows | std::views::enumerate)
    {
    int const r{int(index)};
    view->setItem(r, 0, cell(row.key, false));
    if(with_what)
      view->setItem(r, 1, cell(std::string{star_census::description(row.key)}, false));
    view->setItem(r, at + 1, cell(std::format("{}", row.worlds), true));
    view->setItem(r, at + 2, cell(percent(row.share), true));
    // the share among every landable planet with an atmosphere: a row well over it is where this one gathers
    view->setItem(r, at + 3, cell(percent(row.background), true));
    }
  }
  }  // namespace

atmospheres_page_t::atmospheres_page_t(std::string db_path, QWidget * parent) :
    QWidget{parent},
    db_path_{std::move(db_path)}
  {
  auto * layout = new QVBoxLayout(this);
  auto * note{new QLabel(explanation, this)};
  note->setWordWrap(true);
  layout->addWidget(note);
  summary_ = new QLabel(this);
  layout->addWidget(summary_);

  auto * row = new QHBoxLayout();
  row->addWidget(new QLabel("Atmosphere", this));
  atmosphere_ = new QComboBox(this);
  row->addWidget(atmosphere_, 1);
  layout->addLayout(row);

  layout->addWidget(new QLabel("By the arrival star - the galaxy map's star filter", this));
  arrival_view_ = make_table(this, {"Star", "What it is", "Systems in full", "With it", "Chance", "x average"});
  layout->addWidget(arrival_view_, 2);
  layout->addWidget(new QLabel("By the parent star", this));
  parent_star_view_ = make_table(this, {"Star", "What it is", "Planets", "Share", "Of all landable"});
  layout->addWidget(parent_star_view_, 1);
  layout->addWidget(new QLabel("By the parent body", this));
  parent_body_view_ = make_table(this, {"Orbits", "Planets", "Share", "Of all landable"});
  parent_body_view_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  parent_body_view_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
  layout->addWidget(parent_body_view_, 1);

  connect(
    atmosphere_,
    &QComboBox::currentIndexChanged,
    this,
    [this](int) { eht::event_guard("atmospheres choice", [this] { show_odds(); }); }
  );
  poll_ = new QTimer(this);
  poll_->setInterval(std::chrono::seconds{5});
  connect(
    poll_, &QTimer::timeout, this, [this] { eht::event_guard("atmospheres refresh", [this] { refresh(false); }); }
  );
  }

auto atmospheres_page_t::showEvent(QShowEvent * event) -> void
  {
  QWidget::showEvent(event);
  eht::event_guard("atmospheres refresh", [this] { refresh(true); });
  poll_->start();
  }

auto atmospheres_page_t::refresh(bool now) -> void
  {
  if(not isVisible())
    {
    poll_->stop();
    return;
    }
  // a connection of its own for each look, as the album's; the state's belongs to the journal following thread
  database_storage_t db{db_path_};
  if(auto const opened{db.open()}; not opened)
    {
    spdlog::error("atmospheres: the database could not be opened: {}", opened.error().message());
    return;
    }
  // the cheap look first: a body more in this galaxy, or the shared ones five minutes old, else nothing to read
  auto const clock{std::chrono::steady_clock::now()};
  auto const total{db.load_body_total()};
  if(not total)
    spdlog::error("atmospheres: the bodies could not be counted: {}", total.error().message());
  if(not now and total and *total == bodies_read_ and clock - read_ < std::chrono::minutes{5})
    return;
  bodies_read_ = total ? *total : -1;
  read_ = clock;

  auto census{db.load_atmosphere_census()};
  if(not census)
    {
    spdlog::error("atmospheres: the systems could not be read: {}", census.error().message());
    return;
    }
  for(std::string const & galaxy: eht::settings()->exploration.shared_galaxies)
    if(auto shared{database_storage_t::load_atmosphere_census_from(galaxy)}; shared)
      *census = atmosphere_odds::merge(std::move(*census), *shared);
    else
      spdlog::warn("atmospheres: the shared galaxy {} could not be read: {}", galaxy, shared.error().message());
  prepared_ = atmosphere_odds::prepare(*census);
  show_atmospheres();
  }

auto atmospheres_page_t::show_atmospheres() -> void
  {
  auto const rows{atmosphere_odds::atmospheres(prepared_)};
  summary_->setText(
    QString::fromStdString(
      std::format(
        "{} systems known, {} of them scanned in full; {} landable planets with an atmosphere",
        prepared_.systems_known,
        prepared_.full.size(),
        prepared_.worlds.size()
      )
    )
  );
  QString const chosen{atmosphere_->currentData().toString()};
    {
    QSignalBlocker const quiet{atmosphere_};
    atmosphere_->clear();
    for(atmosphere_odds::atmosphere_row_t const & row: rows)
      atmosphere_->addItem(
        QString::fromStdString(
          std::format("{} - {} found, in {} of the systems in full", row.atmosphere, row.worlds, percent(row.chance))
        ),
        QString::fromStdString(row.atmosphere)
      );
    // the one chosen stays; at first the rarest, which the list starts with
    if(int const at{atmosphere_->findData(chosen)}; at >= 0)
      atmosphere_->setCurrentIndex(at);
    }
  show_odds();
  }

auto atmospheres_page_t::show_odds() -> void
  {
  std::string const atmosphere{atmosphere_->currentData().toString().toStdString()};
  auto const rows{atmosphere_odds::by_arrival_star(prepared_, atmosphere)};
  QBrush const weak{palette().color(QPalette::PlaceholderText)};
  arrival_view_->setRowCount(int(rows.size()));
  for(auto const & [index, row]: rows | std::views::enumerate)
    {
    int const r{int(index)};
    std::array cells{
      cell(row.key, false),
      cell(std::string{star_census::description(row.key)}, false),
      cell(std::format("{}", row.systems), true),
      cell(row.systems_with != 0u ? std::format("{}", row.systems_with) : std::string{}, true),
      cell(row.systems_with != 0u ? percent(row.chance) : std::string{}, true),
      cell(row.systems_with != 0u ? std::format("{:.1f}", row.lift) : std::string{}, true)
    };
    for(auto const & [column, item]: cells | std::views::enumerate)
      {
      if(row.weak)
        item->setForeground(weak);
      arrival_view_->setItem(r, int(column), item);
      }
    }
  fill_shares(parent_star_view_, atmosphere_odds::by_parent_star(prepared_, atmosphere), true);
  fill_shares(parent_body_view_, atmosphere_odds::by_parent_body(prepared_, atmosphere), false);
  }
