#pragma once
#include <galaxy_map_view.h>

#include <chrono>
#include <string>
#include <vector>

#include <qwidget.h>

class QLabel;
class QPushButton;
class QTimer;
class QTreeWidget;

///\brief the systems the galaxy map shows, as edworld_eht read them off its labels (`galmap`), by mass code with the
/// codes' odds - the same as the overlay's block on the map - in a tree, a system's full name copied to the clipboard
/// for the map's search. The last list stays when the map is closed, to be copied from; shown in the Exploration window
///\detail while shown: the file looked at every second, the odds read again every five minutes
class galaxy_map_page_t final : public QWidget
  {
  Q_OBJECT

public:
  explicit galaxy_map_page_t(std::string db_path, QWidget * parent = nullptr);

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  auto refresh() -> void;
  auto read_odds() -> void;
  auto show_view() -> void;
  auto copy_selected() -> void;

  std::string db_path_;
  QLabel * status_{};
  QTreeWidget * tree_{};
  QPushButton * copy_{};
  QTimer * poll_{};
  galaxy_map_view::odds_t odds_;
  galaxy_map_view::known_t known_;
  std::chrono::steady_clock::time_point odds_read_{};
  std::vector<std::string> labels_;
  int64_t labels_ms_{};
  };
