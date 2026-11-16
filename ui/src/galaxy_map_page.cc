#include <galaxy_map_page.h>
#include <databse_storage.h>
#include <eht_settings.h>
#include <event_guard.h>
#include <galmap_read.h>

#include <format>
#include <ranges>

#include <qboxlayout.h>
#include <qclipboard.h>
#include <qguiapplication.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qtimer.h>
#include <qtreewidget.h>

#include <spdlog/spdlog.h>

namespace
  {
constexpr char const * explanation{
  "The systems the galaxy map shows, read off its labels by edworld_eht while the map is open, by mass code (the letter"
  " in Sector AB-C d12-34: how heavy its heaviest star may be): of that code's systems scanned in full, the share with"
  " a landable planet with an atmosphere and with a rare one, and their most common arrival stars. The never visited"
  " first. Double click a system, or choose it and Copy name, for its full name on the clipboard - the map's search"
  " takes it. The last list stays when the map is closed."
};

///\brief the full name a system's row copies
constexpr int full_name_role{Qt::UserRole + 1};

auto percent(uint32_t part, uint32_t whole) -> QString
  { return whole == 0u ? QString{} : QString::fromStdString(std::format("{:.1f}%", 100.0 * part / whole)); }
  }  // namespace

galaxy_map_page_t::galaxy_map_page_t(std::string db_path, QWidget * parent) :
    QWidget{parent},
    db_path_{std::move(db_path)}
  {
  auto * layout = new QVBoxLayout(this);
  auto * note{new QLabel(explanation, this)};
  note->setWordWrap(true);
  layout->addWidget(note);
  auto * row = new QHBoxLayout();
  status_ = new QLabel("the map not read yet (edworld_eht, the galaxy map open)", this);
  row->addWidget(status_, 1);
  copy_ = new QPushButton("Copy name", this);
  copy_->setEnabled(false);
  row->addWidget(copy_);
  layout->addLayout(row);

  tree_ = new QTreeWidget(this);
  tree_->setColumnCount(6);
  tree_->setHeaderLabels({"System", "Visited", "With atmosphere", "Rare", "Arrival stars", "In full"});
  tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  layout->addWidget(tree_, 1);

  connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) {
    eht::event_guard("galaxy map copy", [this] { copy_selected(); });
  });
  connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this] {
    auto const items{tree_->selectedItems()};
    copy_->setEnabled(not items.empty() and items.front()->data(0, full_name_role).isValid());
  });
  connect(copy_, &QPushButton::clicked, this, [this] { eht::event_guard("galaxy map copy", [this] { copy_selected(); }); });
  poll_ = new QTimer(this);
  poll_->setInterval(std::chrono::seconds{1});
  connect(poll_, &QTimer::timeout, this, [this] { eht::event_guard("galaxy map refresh", [this] { refresh(); }); });
  }

auto galaxy_map_page_t::showEvent(QShowEvent * event) -> void
  {
  QWidget::showEvent(event);
  eht::event_guard("galaxy map refresh", [this] { refresh(); });
  poll_->start();
  }

auto galaxy_map_page_t::read_odds() -> void
  {
  // a connection of its own for each read, as the album's; the state's belongs to the journal following thread
  database_storage_t db{db_path_};
  if(auto const opened{db.open()}; not opened)
    {
    spdlog::error("galaxy map: the database could not be opened: {}", opened.error().message());
    return;
    }
  auto census{db.load_atmosphere_census()};
  if(not census)
    {
    spdlog::error("galaxy map: the systems could not be read: {}", census.error().message());
    return;
    }
  auto const settings{eht::settings()};
  for(std::string const & galaxy: settings->exploration.shared_galaxies)
    if(auto shared{database_storage_t::load_atmosphere_census_from(galaxy)}; shared)
      *census = atmosphere_odds::merge(std::move(*census), *shared);
  odds_ = galaxy_map_view::mass_code_odds(*census, atmosphere_odds::prepare(*census), settings->exploration.rare_atmosphere_worlds);
  known_ = galaxy_map_view::known(*census);
  odds_read_ = std::chrono::steady_clock::now();
  }

auto galaxy_map_page_t::refresh() -> void
  {
  if(not isVisible())
    {
    poll_->stop();
    return;
    }
  auto const settings{eht::settings()};
  if(settings->edworld.dir.empty())
    return;
  bool const stale_odds{odds_.empty() or std::chrono::steady_clock::now() - odds_read_ > std::chrono::minutes{5}};
  if(stale_odds)
    read_odds();
  // a new list only when edworld wrote another one: the last stays when the map is closed
  auto read{read_galmap(std::filesystem::path{settings->edworld.dir} / "galmap")};
  bool const other{read and read->unix_ms != labels_ms_ and read->labels != labels_};
  if(read)
    labels_ms_ = read->unix_ms;
  if(other)
    labels_ = std::move(read->labels);
  if(other or stale_odds)
    show_view();
  }

auto galaxy_map_page_t::show_view() -> void
  {
  auto const view{galaxy_map_view::view(labels_, odds_, known_)};
  // the system chosen stays chosen in the new list, by its full name
  QString chosen;
  if(auto const items{tree_->selectedItems()}; not items.empty())
    chosen = items.front()->data(0, full_name_role).toString();

  auto const when{std::chrono::floor<std::chrono::seconds>(std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{labels_ms_}})};
  status_->setText(
    labels_ms_ == 0 ? QString{"the map not read yet (edworld_eht, the galaxy map open)"}
                    : QString::fromStdString(std::format(
                        "read {:%H:%M:%S} UTC: {} systems{}", when, view.names, view.sector.empty() ? std::string{} : ", " + view.sector
                      ))
  );

  tree_->clear();
  QBrush const grey{QColor::fromRgb(eht::settings()->exploration.below_worth.rgb)};
  QTreeWidgetItem * keep{};
  for(galaxy_map_view::group_t const & g: view.groups)
    {
    auto const & o{g.odds};
    std::string stars;
    for(auto const & [star, n]: o.stars | std::views::take(3))
      stars += (stars.empty() ? "" : "/") + star;
    auto * code{new QTreeWidgetItem(tree_)};
    code->setText(0, QString::fromStdString(std::format("{} ({} in view, {} new)", g.code, g.in_view, g.unvisited.size())));
    code->setText(2, percent(o.with_atmosphere, o.full));
    code->setText(3, percent(o.with_rare, o.full));
    code->setText(4, QString::fromStdString(stars));
    code->setText(5, QString::number(o.full));
    auto const add = [&](std::string const & label, bool visited)
    {
      std::string const full{galaxy_map_view::full_name(label, known_)};
      auto * item{new QTreeWidgetItem(code)};
      // shown without the sector, copied whole
      auto const parts{galaxy_map_view::parse(full)};
      std::string const shown{parts ? std::format("{} {}{}", parts->block, parts->mass_code, parts->number) : full};
      item->setText(0, QString::fromStdString(shown));
      item->setText(1, visited ? "yes" : "");
      item->setData(0, full_name_role, QString::fromStdString(full));
      item->setToolTip(0, QString::fromStdString(full));
      if(visited)
        for(int c{}; c != 2; ++c)
          item->setForeground(c, grey);
      if(item->data(0, full_name_role).toString() == chosen)
        keep = item;
    };
    for(std::string const & label: g.unvisited)
      add(label, false);
    for(std::string const & label: g.visited)
      add(label, true);
    code->setExpanded(true);
    }
  if(keep)
    keep->setSelected(true);
  }

auto galaxy_map_page_t::copy_selected() -> void
  {
  auto const items{tree_->selectedItems()};
  if(items.empty() or not items.front()->data(0, full_name_role).isValid())
    return;
  QString const full{items.front()->data(0, full_name_role).toString()};
  QGuiApplication::clipboard()->setText(full);
  status_->setText(status_->text().section(" | copied", 0, 0) + " | copied: " + full);
  }
