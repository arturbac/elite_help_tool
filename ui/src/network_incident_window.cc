#include <network_incident_window.h>
#include <time_zone_warning.h>
#include <data/network.h>

#include <backup.h>
#include <event_guard.h>
#include <eht_settings.h>
#include <evidence_log.h>
#include <file_io.h>
#include <netstate.h>
#include <network_incident.h>

#include <qboxlayout.h>
#include <qformat.h>
#include <qheaderview.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ranges>

namespace
  {
///\brief net-monitor's log is read this far each side of a found moment - wide enough to reach its five
/// minute heartbeat on at least one side, so a quiet window still reads as known-clean rather than unknown
constexpr std::chrono::seconds correlation_span{360};
///\brief how long the scanner sleeps between ticks - a disconnect is worth noticing well before the next
/// bar visit, but there is no reason to reread the current netLog file every second either
constexpr std::chrono::seconds scan_interval{60};

[[nodiscard]]
auto verdict_text(network_incident::verdict_e verdict) -> QString
  {
  switch(verdict)
    {
    case network_incident::verdict_e::unknown:      return "no net-monitor log for this moment";
    case network_incident::verdict_e::clean:        return "network clean";
    case network_incident::verdict_e::unbound_only: return "Unbound only, path OK";
    case network_incident::verdict_e::network_down: return "local/upstream network down";
    }
  return {};
  }

[[nodiscard]]
auto text_cell(QString const & text) -> QTableWidgetItem * { return new QTableWidgetItem(text); }

[[nodiscard]]
auto whole_file(std::filesystem::path const & path) -> std::string
  {
  std::ifstream file{path, std::ios::binary};
  if(not file)
    return {};
  return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  }

///\brief the files of a directory whose name contains needle, oldest first - both netLog.* and Journal.*
/// are named with an ISO 8601 stamp, so a lexicographic sort is a chronological one
[[nodiscard]]
auto files_containing(std::filesystem::path const & dir, std::string_view needle) -> std::vector<std::filesystem::path>
  {
  std::vector<std::filesystem::path> found;
  std::error_code ec;
  if(dir.empty() or not std::filesystem::is_directory(dir, ec))
    return found;
  for(auto const & entry: std::filesystem::directory_iterator{dir, ec})
    if(entry.is_regular_file(ec) and entry.path().filename().string().contains(needle))
      found.push_back(entry.path());
  std::ranges::sort(found);
  return found;
  }

///\brief the machine's offset from UTC when the netLog file began - its name is in local time, and a file
/// from the other side of a daylight saving change has the other offset
[[nodiscard]]
auto local_utc_offset(std::string_view netlog_name) -> std::chrono::seconds
  {
  try
    {
    if(auto const local{evidence::netlog_local_start(netlog_name)}; local)
      return std::chrono::current_zone()->get_info(*local).first.offset;
    return std::chrono::current_zone()->get_info(std::chrono::system_clock::now()).offset;
    }
  catch(...)
    {
    eht::warn_no_time_zone();
    return std::chrono::seconds{0};
    }
  }

///\brief where the game's netLog files are - a settings override, else the running game's own directory
[[nodiscard]]
auto resolve_netlog_dir(std::filesystem::path const & journal_dir) -> std::filesystem::path
  {
  auto const cfg{eht::settings()};
  if(not cfg->evidence.netlog_dir.empty())
    return backup::expand_home(cfg->evidence.netlog_dir);

  std::filesystem::path cwd;
  if(auto const pid{netstate::find_game(journal_dir)}; pid)
    {
    std::error_code ec;
    cwd = std::filesystem::read_symlink(std::format("/proc/{}/cwd", *pid), ec);
    }
  return evidence::find_netlog_dir(journal_dir, cwd);
  }

///\brief correlates a find against net-monitor and stores it, quietly skipping one already known
///\returns false for one too fresh yet - the net-monitor window after it has not passed, and a verdict
/// taken now would be frozen half empty, since a stored incident is never looked at again
auto store_found(database_storage_t & db, network_incident::detected_incident_t const & found) -> bool
  {
  auto const occurred{std::chrono::floor<std::chrono::seconds>(found.occurred)};
  // the current netLog is read again every pass - journald is asked only about what is new; a row found
  // before the times after a disconnect were read gets them now
  if(auto known{db.network_incident_known(occurred, found.category)}; known and *known)
    {
    if(auto res{db.fill_network_incident_times(info::network_incident_t{
         .occurred = occurred,
         .category = found.category,
         .last_rx_s = found.last_rx_s,
         .reconnect_started_s = found.reconnect_started_s,
         .reconnected_s = found.reconnected_s
       })};
       not res)
      spdlog::error("network incident scan: failed to fill in the times of an incident");
    return true;
    }
  if(std::chrono::system_clock::now() < found.occurred + correlation_span)
    return false;
  std::string const log{network_incident::net_monitor_log(found.occurred, correlation_span, correlation_span)};
  info::network_incident_t incident{
    .occurred = occurred,
    .category = found.category,
    .detail = found.detail,
    .verdict = network_incident::classify(log),
    .net_monitor_log = log,
    .last_rx_s = found.last_rx_s,
    .reconnect_started_s = found.reconnect_started_s,
    .reconnected_s = found.reconnected_s
  };
  if(auto res{db.store(incident)}; not res)
    spdlog::error("network incident scan: failed to store an incident");
  return true;
  }
  }  // namespace

network_incident_window_t::network_incident_window_t(std::string db_path, std::string journal_dir, QWidget * parent) :
    QMdiSubWindow(parent),
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("network incident window: failed to open {}", db_path);

  setup_ui();
  reload();
  if(auto res{db_.load_latest_incident_oid()}; res)
    shown_through_ = *res;

  scanner_ = std::jthread{
    [db_path, journal_dir](std::stop_token stoken)
    {
      eht::event_guard(
        "network incidents scanner", [&] { network_incident_window_t::run_scanner(stoken, db_path, journal_dir); }
      );
    }
  };
  }

network_incident_window_t::~network_incident_window_t() = default;

auto network_incident_window_t::setup_ui() -> void
  {
  setWindowTitle("Network incidents");
  resize(820, 600);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  auto * note{new QLabel(
    "Disconnects and technical failures, found automatically from the game's own netLog history and from "
    "journal files that end without a Shutdown - nothing here is typed in by hand. The game's own colour+ship "
    "dialogs never reach any log, so they cannot be matched to a row; what is shown instead is the real "
    "technical detail behind each one, and whether net-monitor.service saw the local network in trouble at "
    "that moment. \"Ended without Shutdown\" is not necessarily a crash - closing the game any way other than "
    "its own exit menu looks the same on disk.",
    central_widget
  )};
  note->setWordWrap(true);
  layout->addWidget(note);

  summary_ = new QLabel(central_widget);
  summary_->setWordWrap(true);
  layout->addWidget(summary_);

  history_view_ = new QTableWidget(central_widget);
  history_view_->setColumnCount(4);
  history_view_->setHorizontalHeaderLabels({"When (UTC)", "Category", "Net-monitor", "Detail"});
  history_view_->verticalHeader()->setVisible(false);
  history_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  history_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  history_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  history_view_->horizontalHeader()->setStretchLastSection(true);
  layout->addWidget(history_view_, 2);

  layout->addWidget(new QLabel("Net-monitor log around the picked row:", central_widget));
  detail_view_ = new QPlainTextEdit(central_widget);
  detail_view_->setReadOnly(true);
  detail_view_->setPlaceholderText("Pick a row above to see net-monitor's log of the minutes around it");
  layout->addWidget(detail_view_, 1);

  legend_ = new QLabel(central_widget);
  legend_->setWordWrap(true);
  {
  QString text{"Community's reading of the game's own dialogs (not Frontier's own documentation, and no row "
               "above is matched to one of these - a legend to read by eye only): "};
  bool first{true};
  for(network_incident::known_code_t const & code: network_incident::known_codes())
    {
    if(not first)
      text += "; ";
    first = false;
    text += qformat("{} - {} ({})", code.code, code.meaning, code.side);
    }
  legend_->setText(text);
  }
  layout->addWidget(legend_);

  connect(
    history_view_,
    &QTableWidget::itemSelectionChanged,
    this,
    [this]
    {
      // the table sorts on a header click, so the view's row is not the index into rows_
      QTableWidgetItem const * const first{history_view_->item(history_view_->currentRow(), 0)};
      size_t const ix{first != nullptr ? first->data(Qt::UserRole).value<size_t>() : rows_.size()};
      detail_view_->setPlainText(ix < rows_.size() ? QString::fromStdString(rows_[ix].net_monitor_log) : QString{});
    }
  );

  refresh_timer_ = new QTimer(this);
  connect(refresh_timer_, &QTimer::timeout, this, [this] { eht::event_guard("network incidents refresh", [this] { poll(); }); });
  refresh_timer_->start(20000);

  setWidget(central_widget);
  }

auto network_incident_window_t::poll() -> void
  {
  // a single count(*) is cheap enough for a timer; redrawing the whole table is not, so it only happens
  // when the scanner has actually found something since the last time this window looked
  auto res{db_.load_latest_incident_oid()};
  if(not res or *res == shown_through_)
    return;
  shown_through_ = *res;
  reload();
  }

auto network_incident_window_t::reload() -> void
  {
  auto res{db_.load_network_incidents()};
  if(not res)
    {
    spdlog::error("network incident window: failed to load incidents");
    return;
    }

  rows_ = std::move(*res);
  summary_->setText(
    rows_.empty() ? QString{"Nothing found yet - the first scan of netLog's whole history can take a while"}
                  : qformat("{} found, newest first", rows_.size())
  );

  history_view_->setSortingEnabled(false);
  history_view_->setRowCount(int(rows_.size()));
  for(int ix{}; info::network_incident_t const & row: rows_)
    {
    QTableWidgetItem * const when{text_cell(qformat("{:%Y-%m-%d %H:%M:%S}", row.occurred))};
    when->setData(Qt::UserRole, QVariant::fromValue(size_t(ix)));
    history_view_->setItem(ix, 0, when);
    history_view_->setItem(ix, 1, text_cell(QString::fromStdString(row.category)));
    history_view_->setItem(ix, 2, text_cell(verdict_text(row.verdict)));
    std::string detail{row.detail};
    if(row.last_rx_s)
      detail = std::format("silent {:.0f} s before - {}", *row.last_rx_s, detail);
    if(row.reconnected_s)
      detail = std::format("connected again {:.0f} s after - {}", *row.reconnected_s, detail);
    history_view_->setItem(ix, 3, text_cell(QString::fromStdString(detail)));
    ++ix;
    }
  history_view_->setSortingEnabled(true);
  }

auto network_incident_window_t::run_scanner(std::stop_token stoken, std::string db_path, std::string journal_dir) -> void
  {
  database_storage_t db{db_path};
  if(auto res{db.open()}; not res)
    {
    spdlog::error("network incident scan: failed to open {}", db_path);
    return;
    }

  std::filesystem::path const journal_path{journal_dir};

  while(not stoken.stop_requested())
    {
    auto progress{db.load_incident_scan_progress()};
    if(not progress)
      spdlog::error("network incident scan: failed to read progress");
    else
      {
      std::string netlog_through{progress->netlog_through};
      std::string journal_through{progress->journal_through};

      // --- netLog: every completed file past the mark, and the current one on every tick ---
      auto const netlog_files{files_containing(resolve_netlog_dir(journal_path), "netLog")};
      for(size_t ix{}; ix != netlog_files.size(); ++ix)
        {
        bool const is_current{ix + 1 == netlog_files.size()};
        std::string const name{netlog_files[ix].filename().string()};
        if(not is_current and name <= progress->netlog_through)
          continue;
        std::string const text{whole_file(netlog_files[ix])};
        if(text.empty())
          continue;
        bool settled{true};
        for(network_incident::detected_incident_t const & found: network_incident::scan_netlog(name, local_utc_offset(name), text))
          settled = store_found(db, found) and settled;
        if(not is_current and settled)
          netlog_through = name;
        }

      // --- journal: completed files past the mark; the newest only once the game is confirmed gone ---
      bool const game_running{netstate::find_game(journal_path).has_value()};
      auto const journal_files{find_all_journals(journal_path)};
      for(size_t ix{}; ix != journal_files.size(); ++ix)
        {
        bool const is_newest{ix + 1 == journal_files.size()};
        if(is_newest and game_running)
          continue;
        std::string const name{journal_files[ix].filename().string()};
        if(name <= progress->journal_through)
          continue;
        if(auto const found{network_incident::scan_journal_for_crash(whole_file(journal_files[ix]))};
           found and not store_found(db, *found))
          break;
        journal_through = name;
        }

      if(netlog_through != progress->netlog_through or journal_through != progress->journal_through)
        if(auto res{db.store_incident_scan_progress(netlog_through, journal_through)}; not res)
          spdlog::error("network incident scan: failed to save progress");
      }

    for(int tick{}; tick != scan_interval.count() and not stoken.stop_requested(); ++tick)
      std::this_thread::sleep_for(std::chrono::seconds{1});
    }
  }
