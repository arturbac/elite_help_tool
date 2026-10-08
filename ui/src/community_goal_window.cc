#include <community_goal_window.h>
#include <community_goal.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qheaderview.h>

#include <ranges>

namespace
  {
enum struct column_e : int
  {
  goal,
  place,
  contributed,
  band,
  bracket,
  contributors,
  tier,
  time_left,
  column_max
  };

[[nodiscard]]
auto bracket_text(community_goal::bracket_e bracket) -> QString
  {
  switch(bracket)
    {
    case community_goal::bracket_e::top_50:  return "top 50%";
    case community_goal::bracket_e::top_75:  return "top 75% only";
    case community_goal::bracket_e::outside: return "below 75%";
    }
  return {};
  }

///\brief green for the top half, amber for the top three quarters, red below - text colour only
[[nodiscard]]
auto bracket_colour(community_goal::bracket_e bracket) -> QColor
  {
  switch(bracket)
    {
    case community_goal::bracket_e::top_50:  return QColor{0x7b, 0xd8, 0x8f};
    case community_goal::bracket_e::top_75:  return QColor{0xff, 0xcc, 0x66};
    case community_goal::bracket_e::outside: return QColor{0xff, 0x7b, 0x72};
    }
  return {};
  }
  }  // namespace

community_goal_window_t::community_goal_window_t(current_state_t const & state, QWidget * parent) :
    QMdiSubWindow(parent),
    state_{state}
  {
  setup_ui();
  }

auto community_goal_window_t::setup_ui() -> void
  {
  setWindowTitle("Community goals");
  resize(1000, 260);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  header_ = new QLabel(central_widget);
  header_->setWordWrap(true);
  layout->addWidget(header_);

  table_ = new QTableWidget(central_widget);
  table_->setColumnCount(int(column_e::column_max));
  table_->setHorizontalHeaderLabels(
    {"Goal", "System / market", "Contributed", "Band", "Bracket", "Contributors", "Tier", "Time left"}
  );
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(int(column_e::goal), QHeaderView::Stretch);
  layout->addWidget(table_, 1);

  setWidget(central_widget);
  refresh_ui(true);
  }

auto community_goal_window_t::refresh_ui(bool force) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(not force and changes_seen_ == state_.community_goal_changes_ and now - drawn_ < std::chrono::minutes{1})
    return;
  drawn_ = now;
  changes_seen_ = state_.community_goal_changes_;

  auto const & goals{state_.community_goals_};
  header_->setText(
    goals.empty()
      ? QString{"No community goal known yet. The game lists them at login and on opening a goal's panel."}
      : QString{"As the game last told it - open the goal's panel after a delivery to read the new band."}
  );

  auto const game_now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  table_->setRowCount(int(goals.size()));
  for(auto const & [row, goal]: goals | std::views::enumerate)
    {
    auto const set = [&](column_e column, QString const & text)
    {
      auto * item{new QTableWidgetItem{text}};
      table_->setItem(int(row), int(column), item);
      return item;
    };
    auto const bracket{community_goal::bracket_of(goal.PlayerPercentileBand)};
    set(column_e::goal, QString::fromStdString(goal.Title));
    set(column_e::place, qformat("{} / {}", goal.SystemName, goal.MarketName));
    set(column_e::contributed, qformat("{}", goal.PlayerContribution));
    set(column_e::band, goal.PlayerPercentileBand == 0u ? QString{"-"} : qformat("{}%", goal.PlayerPercentileBand));
    set(column_e::bracket, bracket_text(bracket))->setForeground(bracket_colour(bracket));
    set(column_e::contributors, qformat("{}", goal.NumContributors));
    set(column_e::tier, QString::fromStdString(goal.TierReached));
    set(
      column_e::time_left,
      goal.IsComplete ? QString{"complete"}
                      : QString::fromStdString(community_goal::time_left(goal.Expiry, game_now))
    );
    }
  }
