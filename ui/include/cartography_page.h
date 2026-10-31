#pragma once
#include <biology.h>
#include <databse_storage.h>

#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <vector>

#include <qwidget.h>

class QLabel;
class QTableWidget;
class QTimer;

///\brief the sales of cartographic data, what each paid against EHT's estimate - a reward for exploring,
/// shown in the Exploration window
///\detail the sales are dug out of every journal of the account, which takes from a second to a minute; it is
/// done in a thread of its own, so the tool goes on answering, and done again at most once a minute while shown
class cartography_page_t final : public QWidget
  {
  Q_OBJECT

public:
  explicit cartography_page_t(std::string db_path, QWidget * parent = nullptr);

  ///\brief reads the journals again when the page is on top and the last reading is a minute old, or at once
  auto refresh(bool now) -> void;

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  auto show_sales(std::vector<bio::cartography_sale_t> sales) -> void;

  /// a connection of its own; the state's db_ belongs to the journal following thread
  database_storage_t db_;
  QLabel * note_{};
  QTableWidget * view_{};
  ///\brief looks every so often whether the reading in its thread is done
  QTimer * poll_{};
  std::optional<std::future<std::vector<bio::cartography_sale_t>>> reading_;
  std::chrono::steady_clock::time_point read_{};
  };
