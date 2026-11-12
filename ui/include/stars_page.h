#pragma once
#include <databse_storage.h>
#include <star_census.h>

#include <chrono>
#include <string>
#include <vector>

#include <qwidget.h>

class QComboBox;
class QLabel;
class QTableWidget;
class QTimer;
struct current_state_t;

///\brief the kinds of star of the systems visited, in the region the commander is in and over all - to spread the
/// visits evenly over the kinds and to set the galaxy map's star filter by what is still rare; shown in the
/// Exploration window
///\detail read again while shown, when the commander is in another system or the last reading is a minute old
class stars_page_t final : public QWidget
  {
  Q_OBJECT

public:
  stars_page_t(std::string db_path, current_state_t const & state, QWidget * parent = nullptr);

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  auto refresh(bool now) -> void;
  auto show_census() -> void;

  std::string db_path_;
  current_state_t const & state_;
  QComboBox * grouping_{};
  QLabel * region_label_{};
  QTableWidget * region_view_{};
  QLabel * all_label_{};
  QTableWidget * all_view_{};
  QTimer * poll_{};
  std::vector<star_census::system_t> systems_;
  uint64_t read_at_system_{};
  std::chrono::steady_clock::time_point read_{};
  };
