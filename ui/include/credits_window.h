#pragma once
#include <credits.h>

#include <qcombobox.h>
#include <qlabel.h>
#include <qmdisubwindow.h>
#include <qtablewidget.h>
#include <qtimer.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>

///\brief the player's money: the balance now, what this session brought in and took away, and the history
/// by day, week, month, quarter or year - every column a kind of income or expense
///
/// A background thread of its own reads the journal's whole history once (a couple of seconds for a year and
/// a half of a busy commander), later ticks only the files that grew. Nothing is stored: the journal files
/// are the record. Every commander found in the files gets a ledger; the one of the newest session is shown
/// first. What credits.h says of the balance readings applies here - the columns marked with a star are
/// told from the balance, not from an event
class credits_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  QComboBox * commander_{};
  QLabel * balance_{};
  QLabel * session_summary_{};
  QTableWidget * session_view_{};
  QComboBox * period_{};
  QTableWidget * history_view_{};

  explicit credits_window_t(std::string journal_dir, QWidget * parent = nullptr);
  credits_window_t(credits_window_t const &) = delete;
  auto operator=(credits_window_t const &) -> credits_window_t & = delete;
  ~credits_window_t() override;

  auto setup_ui() -> void;

private:
  using ledgers_t = std::vector<credits::ledger_t>;

  ///\brief what the scanner last put together, handed over whole
  std::mutex ledgers_mtx_;
  std::shared_ptr<ledgers_t const> published_;
  std::atomic<uint64_t> generation_{};

  ///\brief what is on the screen
  std::shared_ptr<ledgers_t const> shown_;
  uint64_t shown_generation_{};

  std::jthread scanner_;
  QTimer * refresh_timer_{};

  ///\brief the cheap poll a timer can afford: one atomic read, a redraw only when the scanner has news
  auto poll() -> void;
  auto redraw() -> void;
  [[nodiscard]]
  auto picked_ledger() const -> credits::ledger_t const *;
  auto fill_session(credits::ledger_t const & ledger) -> void;
  auto fill_history(credits::ledger_t const & ledger) -> void;

  auto run_scanner(std::stop_token stoken, std::string journal_dir) -> void;
  };
