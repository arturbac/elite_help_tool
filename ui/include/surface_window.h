#pragma once
#include "logic.h"
#include <surface_nav.h>

#include <qlabel.h>
#include <qlineedit.h>
#include <qlistwidget.h>
#include <qmdisubwindow.h>
#include <qtablewidget.h>

#include <chrono>
#include <future>
#include <optional>
#include <vector>

///\brief a point on a body's surface to go to, typed in by hand
///
/// A tip-off or a guide gives a place as two numbers, and the game cannot be told them. Here they are
/// typed or pasted in any of the usual spellings; the overlay then points the way and says how far. The
/// body is the one we are near unless another is named, so a target can be set before arriving
class surface_window_t final : public QMdiSubWindow
  {
  Q_OBJECT
public:
  explicit surface_window_t(current_state_t const & state, QWidget * parent = nullptr);

  ///\brief the target the overlay leads to, if one is set
  [[nodiscard]]
  auto target() const -> std::optional<nav::target_t> const & { return target_; }

  ///\brief reads where we stand again and says the way from there
  auto refresh_ui() -> void;

private:
  current_state_t const & state_;

  QLabel * here_{};
  QLineEdit * point_{};
  QLineEdit * body_{};
  QLineEdit * label_{};
  QLabel * feedback_{};
  QLabel * way_{};
  QListWidget * recent_{};
  QLabel * codex_header_{};
  QTableWidget * codex_{};

  std::optional<nav::target_t> target_;
  ///\brief the targets set this session, the newest first - going back and forth between two is common
  std::vector<nav::target_t> recent_targets_;
  ///\brief the body we were last near, to fill in the body field while it was not typed by hand
  std::string near_body_;

  ///\brief the codex read out of the journals - idle here between two updates, away in the task during one
  std::optional<nav::codex_index_t> index_{std::in_place};
  std::future<nav::codex_index_t> indexing_;
  std::chrono::steady_clock::time_point indexed_{};
  ///\brief the whole archive has been read once - until then an empty table says nothing
  bool index_ready_{};
  ///\brief the entries of the body the table shows, in the table's order
  std::vector<nav::codex_point_t> codex_points_;
  std::string codex_body_;

  auto setup_ui() -> void;
  auto set_target() -> void;
  auto choose(nav::target_t target) -> void;
  auto show_recent() -> void;
  ///\brief starts reading what the journals added, or takes the result when it is ready
  auto follow_index() -> void;
  ///\brief the table for the body we are near - rebuilt when the body or its entries change, the distances every time
  auto show_codex(std::optional<bio::surface_point_t> here, double radius_m, std::optional<double> heading) -> void;
  };
