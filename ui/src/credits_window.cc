#include <credits_window.h>
#include <time_zone_warning.h>

#include <file_io.h>
#include <event_guard.h>
#include <format_credits.h>

#include <qboxlayout.h>
#include <qformat.h>
#include <qheaderview.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

namespace
  {
using namespace std::chrono_literals;

///\brief how often the scanner looks for files that grew - the balance is worth a glance after a sale, not
/// a reread of the journal every second
constexpr auto scan_interval{30s};

///\brief +1'234'567 Cr, -56 Cr
[[nodiscard]]
auto signed_credits(int64_t value) -> std::string
  {
  if(value < 0)
    return std::format("-{}", format_credits_value(uint64_t(-value)));
  return std::format("+{}", format_credits_value(uint64_t(value)));
  }

[[nodiscard]]
auto played_text(std::chrono::seconds played) -> std::string
  {
  auto const hours{std::chrono::duration_cast<std::chrono::hours>(played)};
  auto const minutes{std::chrono::duration_cast<std::chrono::minutes>(played - hours)};
  return std::format("{} h {:02} min", hours.count(), minutes.count());
  }

[[nodiscard]]
auto local_text(credits::time_point_t at) -> std::string
  {
  try
    {
    return std::format("{:%Y-%m-%d %H:%M}", std::chrono::current_zone()->to_local(at));
    }
  catch(...)
    {
    eht::warn_no_time_zone();
    return std::format("{:%Y-%m-%d %H:%M} UTC", at);
    }
  }

[[nodiscard]]
auto text_cell(QString const & text) -> QTableWidgetItem *
  {
  auto * const item{new QTableWidgetItem(text)};
  item->setFlags(item->flags() & ~Qt::ItemIsEditable);
  return item;
  }

///\brief a sum, right-aligned, green ahead and red behind - muted colours, readable on either theme
[[nodiscard]]
auto money_cell(int64_t value, bool coloured = true) -> QTableWidgetItem *
  {
  auto * const item{text_cell(value == 0 ? QString{} : QString::fromStdString(signed_credits(value)))};
  item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
  if(coloured and value > 0)
    item->setForeground(QColor{0x3c, 0xa0, 0x5a});
  else if(coloured and value < 0)
    item->setForeground(QColor{0xc8, 0x50, 0x46});
  return item;
  }

[[nodiscard]]
auto category_tooltip(credits::category_e category) -> QString
  {
  switch(category)
    {
    case credits::category_e::colonisation:
      return "Goods handed in at a construction site, at what they were last bought for - moved here from Trade";
    case credits::category_e::colonisation_payout:
      return "What construction sites paid for goods delivered - the journal names no amount, told from the "
             "balance readings";
    case credits::category_e::carrier_transfer: return "To and from a fleet carrier's bank - your own money either way";
    case credits::category_e::squadron_bank:
      return "Squadron bank deposits and withdrawals - no event is ever written for them, told from the balance "
             "readings (round amounts)";
    case credits::category_e::unexplained:
      return "The balance moved by this much and no event says why - mostly small, often a purchase in the same "
             "second as the reading";
    case credits::category_e::crew: return "NPC crew wages and hiring";
    case credits::category_e::upkeep:
      return "Fuel, repairs, ammunition, limpets, ship and module transfers, taxis and dropships";
    default: return {};
    }
  }

[[nodiscard]]
auto whole_file(std::filesystem::path const & path) -> std::string
  {
  std::ifstream file{path, std::ios::binary};
  if(not file)
    return {};
  return std::string{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
  }
  }  // namespace

credits_window_t::credits_window_t(std::string journal_dir, QWidget * parent) : QMdiSubWindow(parent)
  {
  setup_ui();
  scanner_ = std::jthread{[this, journal_dir](std::stop_token stoken)
                          { eht::event_guard("credits scanner", [&] { run_scanner(stoken, journal_dir); }); }};
  }

credits_window_t::~credits_window_t()
  {
  // the scanner writes into this object - it has to be gone before the members it touches
  scanner_.request_stop();
  if(scanner_.joinable())
    scanner_.join();
  }

auto credits_window_t::setup_ui() -> void
  {
  setWindowTitle("Credits");
  resize(1100, 700);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  auto * top{new QHBoxLayout};
  top->addWidget(new QLabel("Commander:", central_widget));
  commander_ = new QComboBox(central_widget);
  top->addWidget(commander_);
  balance_ = new QLabel("Reading the journal...", central_widget);
  QFont big{balance_->font()};
  big.setPointSizeF(big.pointSizeF() * 1.4);
  big.setBold(true);
  balance_->setFont(big);
  top->addWidget(balance_, 1);
  layout->addLayout(top);

  session_summary_ = new QLabel(central_widget);
  session_summary_->setWordWrap(true);
  layout->addWidget(session_summary_);

  session_view_ = new QTableWidget(central_widget);
  session_view_->setColumnCount(3);
  session_view_->setHorizontalHeaderLabels({"This session", "Credits", "Events"});
  session_view_->verticalHeader()->setVisible(false);
  session_view_->setSelectionMode(QAbstractItemView::NoSelection);
  session_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  layout->addWidget(session_view_, 1);

  auto * history_bar{new QHBoxLayout};
  history_bar->addWidget(new QLabel("History by", central_widget));
  period_ = new QComboBox(central_widget);
  period_->addItem("Day", QVariant::fromValue(int(credits::period_e::day)));
  period_->addItem("Week", QVariant::fromValue(int(credits::period_e::week)));
  period_->addItem("Month", QVariant::fromValue(int(credits::period_e::month)));
  period_->addItem("Quarter", QVariant::fromValue(int(credits::period_e::quarter)));
  period_->addItem("Year", QVariant::fromValue(int(credits::period_e::year)));
  period_->setCurrentIndex(1);
  history_bar->addWidget(period_);
  history_bar->addStretch(1);
  layout->addLayout(history_bar);

  history_view_ = new QTableWidget(central_widget);
  history_view_->verticalHeader()->setVisible(false);
  history_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  history_view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  layout->addWidget(history_view_, 3);

  auto * note{new QLabel(
    "Earned and spent leave aside money moved between your own pockets (carrier bank, squadron bank). "
    "Columns marked * are told from the balance the game itself reports - no journal event names their "
    "amounts. Played time runs from LoadGame to the last line of the session.",
    central_widget
  )};
  note->setWordWrap(true);
  layout->addWidget(note);

  connect(commander_, &QComboBox::currentIndexChanged, this, [this](int) { redraw(); });
  connect(period_, &QComboBox::currentIndexChanged, this, [this](int) { redraw(); });

  refresh_timer_ = new QTimer(this);
  connect(refresh_timer_, &QTimer::timeout, this, [this] { eht::event_guard("credits refresh", [this] { poll(); }); });
  refresh_timer_->start(2000);

  setWidget(central_widget);
  }

auto credits_window_t::poll() -> void
  {
  uint64_t const generation{generation_.load(std::memory_order_acquire)};
  if(generation == shown_generation_)
    return;
  {
  std::scoped_lock lock{ledgers_mtx_};
  shown_ = published_;
  }
  shown_generation_ = generation;
  if(not shown_)
    return;

  // the commander list follows what was found; the one picked stays picked
  QString const picked{commander_->currentData().toString()};
  QSignalBlocker const block{commander_};
  commander_->clear();
  for(credits::ledger_t const & ledger: *shown_)
    commander_->addItem(qformat("{} ({})", ledger.name, ledger.fid), QString::fromStdString(ledger.fid));
  if(int const ix{commander_->findData(picked)}; ix >= 0)
    commander_->setCurrentIndex(ix);
  else
    commander_->setCurrentIndex(0);
  redraw();
  }

auto credits_window_t::picked_ledger() const -> credits::ledger_t const *
  {
  if(not shown_)
    return nullptr;
  std::string const fid{commander_->currentData().toString().toStdString()};
  auto const it{std::ranges::find(*shown_, fid, &credits::ledger_t::fid)};
  return it == shown_->end() ? nullptr : &*it;
  }

auto credits_window_t::redraw() -> void
  {
  credits::ledger_t const * const ledger{picked_ledger()};
  if(ledger == nullptr)
    return;

  if(ledger->balance and ledger->balance_read)
    {
    int64_t const since{*ledger->balance - *ledger->balance_read};
    balance_->setText(qformat("{} Cr", format_credits_value(uint64_t(std::max<int64_t>(*ledger->balance, 0)))));
    balance_->setToolTip(qformat(
      "The game said {} Cr at {}; {} Cr since, from the journal",
      format_credits_value(uint64_t(std::max<int64_t>(*ledger->balance_read, 0))),
      local_text(ledger->balance_read_at),
      signed_credits(since)
    ));
    }
  else
    balance_->setText("No balance read yet");

  fill_session(*ledger);
  fill_history(*ledger);
  }

auto credits_window_t::fill_session(credits::ledger_t const & ledger) -> void
  {
  session_view_->setRowCount(0);
  if(ledger.sessions.empty())
    {
    session_summary_->setText("No session found");
    return;
    }
  credits::summary_t const session{credits::summarise_since(ledger, ledger.sessions.back().from)};
  std::string rate;
  if(auto const per_hour{session.per_hour()}; per_hour)
    rate = std::format(" - {} Cr per hour", signed_credits(*per_hour));
  std::string moved;
  if(session.transfers() != 0)
    moved = std::format("; moved between your own pockets {} Cr", signed_credits(session.transfers()));
  session_summary_->setText(qformat(
    "Session from {}, {} played: earned {} Cr, spent {} Cr, net {} Cr{}{}",
    local_text(ledger.sessions.back().from),
    played_text(session.played),
    signed_credits(session.income()),
    signed_credits(session.expenses()),
    signed_credits(session.net()),
    rate,
    moved
  ));

  int row{};
  for(size_t ix{}; ix != credits::category_count; ++ix)
    {
    if(session.count[ix] == 0)
      continue;
    auto const category{credits::category_e(ix)};
    session_view_->setRowCount(row + 1);
    auto * const name{text_cell(QString::fromUtf8(credits::category_label(category)))};
    name->setToolTip(category_tooltip(category));
    session_view_->setItem(row, 0, name);
    session_view_->setItem(row, 1, money_cell(session.by_category[ix], not credits::is_transfer(category)));
    auto * const count{text_cell(QString::number(session.count[ix]))};
    count->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    session_view_->setItem(row, 2, count);
    ++row;
    }
  }

auto credits_window_t::fill_history(credits::ledger_t const & ledger) -> void
  {
  auto const period{credits::period_e(period_->currentData().toInt())};
  std::chrono::time_zone const * zone{};
  try
    {
    zone = std::chrono::current_zone();
    }
  catch(...)
    {
    eht::warn_no_time_zone();
    zone = nullptr;
    }
  std::vector<credits::summary_t> const rows{credits::summarise(ledger, period, zone)};

  // only the categories that ever had anything in them get a column
  std::vector<credits::category_e> columns;
  for(size_t ix{}; ix != credits::category_count; ++ix)
    if(std::ranges::any_of(rows, [ix](credits::summary_t const & row) { return row.count[ix] != 0; }))
      columns.push_back(credits::category_e(ix));

  QStringList headers{"Period", "Played", "Earned", "Spent", "Net", "Net / hour", "Own pockets", "Balance at end"};
  constexpr int fixed_columns{8};
  for(credits::category_e const category: columns)
    headers.push_back(QString::fromUtf8(credits::category_label(category)));

  history_view_->clear();
  history_view_->setColumnCount(int(headers.size()));
  history_view_->setHorizontalHeaderLabels(headers);
  for(int ix{}; credits::category_e const category: columns)
    {
    if(auto * const header{history_view_->horizontalHeaderItem(fixed_columns + ix)}; header != nullptr)
      header->setToolTip(category_tooltip(category));
    ++ix;
    }
  history_view_->setRowCount(int(rows.size()));
  for(int row{}; credits::summary_t const & summary: rows)
    {
    history_view_->setItem(row, 0, text_cell(QString::fromStdString(summary.label)));
    auto * const played{text_cell(QString::fromStdString(played_text(summary.played)))};
    played->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    history_view_->setItem(row, 1, played);
    history_view_->setItem(row, 2, money_cell(summary.income()));
    history_view_->setItem(row, 3, money_cell(summary.expenses()));
    history_view_->setItem(row, 4, money_cell(summary.net()));
    history_view_->setItem(row, 5, summary.per_hour() ? money_cell(*summary.per_hour()) : text_cell({}));
    history_view_->setItem(row, 6, money_cell(summary.transfers(), false));
    auto * const closing{text_cell(
      summary.closing_balance ? QString::fromStdString(format_credits_value(uint64_t(std::max<int64_t>(*summary.closing_balance, 0))))
                              : QString{}
    )};
    closing->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    history_view_->setItem(row, 7, closing);
    for(int ix{}; credits::category_e const category: columns)
      {
      auto * const cell{money_cell(summary.by_category[size_t(category)], not credits::is_transfer(category))};
      if(uint32_t const count{summary.count[size_t(category)]}; count != 0)
        cell->setToolTip(qformat("{} events", count));
      history_view_->setItem(row, fixed_columns + ix, cell);
      ++ix;
      }
    ++row;
    }
  }

auto credits_window_t::run_scanner(std::stop_token stoken, std::string journal_dir) -> void
  {
  struct file_read_t
    {
    std::uintmax_t size{};
    std::vector<credits::commander_log_t> logs;
    };

  // file name -> what it said; the journal files sort by name the way they were written
  std::map<std::string, file_read_t> files;
  std::filesystem::path const journal_path{journal_dir};

  bool first{true};
  while(not stoken.stop_requested())
    {
    auto const started{std::chrono::steady_clock::now()};
    bool changed{};
    for(std::filesystem::path const & path: find_all_journals(journal_path))
      {
      if(stoken.stop_requested())
        return;
      std::error_code ec;
      std::uintmax_t const size{std::filesystem::file_size(path, ec)};
      if(ec)
        continue;
      file_read_t & read{files[path.filename().string()]};
      if(read.size == size)
        continue;
      read.size = size;
      read.logs = credits::scan_journal(whole_file(path));
      changed = true;
      }

    if(changed)
      {
      // every commander's logs in the files' order; the commander of the newest session first
      std::map<std::string, std::vector<credits::commander_log_t>> by_fid;
      std::string newest_fid;
      credits::time_point_t newest{};
      for(auto const & [name, read]: files)
        for(credits::commander_log_t const & log: read.logs)
          {
          by_fid[log.fid].push_back(log);
          if(not log.sessions.empty() and log.sessions.back().from >= newest)
            {
            newest = log.sessions.back().from;
            newest_fid = log.fid;
            }
          }

      auto ledgers{std::make_shared<ledgers_t>()};
      for(auto const & [fid, logs]: by_fid)
        if(not fid.empty())
          ledgers->push_back(credits::build_ledger(logs));
      std::ranges::stable_partition(*ledgers, [&newest_fid](credits::ledger_t const & l) { return l.fid == newest_fid; });

      {
      std::scoped_lock lock{ledgers_mtx_};
      published_ = std::move(ledgers);
      }
      generation_.fetch_add(1u, std::memory_order_release);
      if(first)
        spdlog::info(
          "credits: {} journal files read in {} ms, {} commanders",
          files.size(),
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(),
          published_->size()
        );
      first = false;
      }

    for(auto waited{0s}; waited < scan_interval and not stoken.stop_requested(); waited += 1s)
      std::this_thread::sleep_for(1s);
    }
  }
