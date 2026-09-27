#include <ships_window.h>
#include <overlay_exploration.h>
#include <qformat.h>
#include <qboxlayout.h>
#include <qheaderview.h>
#include <spdlog/spdlog.h>

namespace
  {
enum struct column_e : int
  {
  ship,
  type,
  ident,
  system,
  station,
  distance,
  value,
  state,
  column_max
  };

///\brief a cell that sorts by a number rather than by its text
class number_item_t final : public QTableWidgetItem
  {
public:
  number_item_t(QString const & text, double key) : QTableWidgetItem{text} { setData(Qt::UserRole, key); }

  auto operator<(QTableWidgetItem const & other) const -> bool override
    { return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble(); }
  };

///\brief what the ship is doing, in a word or two
[[nodiscard]]
auto describe_state(fleet::placed_ship_t const & placed) -> std::string
  {
  std::string text;
  if(placed.ship.current)
    text = "flying";
  else if(placed.travelling)
    text = placed.ship.arrives != std::chrono::sys_seconds{}
             ? std::format("in transit, arrives {:%d.%m %H:%M} UTC", placed.ship.arrives)
             : std::string{"in transit"};
  else if(placed.on_carrier)
    text = "on carrier";
  if(placed.ship.hot)
    text += text.empty() ? "hot" : ", hot";
  return text;
  }
  }  // namespace

ships_window_t::ships_window_t(current_state_t const & state, std::string db_path, QWidget * parent) :
    QMdiSubWindow(parent),
    state_{state},
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("ships window: failed to open {}", db_path);

  setup_ui();
  }

auto ships_window_t::setup_ui() -> void
  {
  setWindowTitle("Ships");
  resize(1000, 520);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  header_ = new QLabel(central_widget);
  header_->setWordWrap(true);
  layout->addWidget(header_);

  table_ = new QTableWidget(central_widget);
  table_->setColumnCount(int(column_e::column_max));
  table_->setHorizontalHeaderLabels({"Ship", "Type", "ID", "System", "Station", "Distance", "Value", "State"});
  table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  table_->setSelectionBehavior(QAbstractItemView::SelectRows);
  table_->verticalHeader()->setVisible(false);
  table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  table_->horizontalHeader()->setSectionResizeMode(int(column_e::system), QHeaderView::Stretch);
  table_->setSortingEnabled(true);
  layout->addWidget(table_, 1);

  setWidget(central_widget);
  refresh_ui(true);
  }

auto ships_window_t::refresh_ui(bool force) -> void
  {
  auto const now{std::chrono::steady_clock::now()};
  if(
    not force and changes_seen_ == state_.fleet_changes_ and system_seen_ == state_.system.system_address
    and now - read_ < std::chrono::minutes{1}
  )
    return;
  read_ = now;
  changes_seen_ = state_.fleet_changes_;
  system_seen_ = state_.system.system_address;

  auto const game_now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  auto placed{fleet::locate(db_, state_.system.name, state_.system.system_location, game_now)};
  if(not placed)
    {
    spdlog::error("ships window: failed to load the fleet");
    return;
    }

  uint64_t total{};
  for(fleet::placed_ship_t const & ship: *placed)
    total += ship.ship.value;
  header_->setText(
    placed->empty() ? QString{"No ships known yet. The whole fleet is read the next time a shipyard is opened."}
                    : qformat(
                        "{} ships worth {} | distances from {} | the full list comes on opening a shipyard",
                        placed->size(),
                        overlay_exploration::short_credits(total),
                        state_.system.name.empty() ? std::string{"?"} : state_.system.name
                      )
  );

  // sorting while rows are being filled would move them under the loop
  table_->setSortingEnabled(false);
  table_->setRowCount(int(placed->size()));
  for(auto const & [row, ship]: *placed | std::views::enumerate)
    {
    auto const set = [&](column_e column, QTableWidgetItem * item) { table_->setItem(int(row), int(column), item); };
    set(column_e::ship, new QTableWidgetItem{QString::fromStdString(fleet::shown_name(ship.ship))});
    set(column_e::type, new QTableWidgetItem{QString::fromStdString(ship.ship.type_name)});
    set(column_e::ident, new QTableWidgetItem{QString::fromStdString(ship.ship.ident)});
    set(column_e::system, new QTableWidgetItem{QString::fromStdString(ship.system.empty() ? "?" : ship.system)});
    set(column_e::station, new QTableWidgetItem{QString::fromStdString(ship.station)});
    // unknown distances sort after every known one
    set(
      column_e::distance,
      new number_item_t{
        ship.distance_ly ? qformat("{:.1f} ly", *ship.distance_ly) : QString{"?"},
        ship.distance_ly.value_or(1e9) + (ship.ship.current ? -1.0 : 0.0)
      }
    );
    set(
      column_e::value,
      new number_item_t{
        QString::fromStdString(overlay_exploration::short_credits(ship.ship.value)), double(ship.ship.value)
      }
    );
    set(column_e::state, new QTableWidgetItem{QString::fromStdString(describe_state(ship))});
    }
  table_->setSortingEnabled(true);
  }
