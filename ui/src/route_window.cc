#include <route_window.h>
#include <qbrush.h>
#include <qboxlayout.h>
#include <ranges>
#include <algorithm>
#include <qheaderview.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qguiapplication.h>
#include <qclipboard.h>
#include <json_glaze.h>
#include <spdlog/spdlog.h>
route_model_t::route_model_t(std::vector<info::route_item_t> const & route, QObject * parent) :
    QAbstractTableModel(parent),
    route_(route)
  {
  }

[[nodiscard]]
auto route_model_t::rowCount(QModelIndex const &) const -> int
  {
  return static_cast<int>(route_.size());
  }

[[nodiscard]]
auto route_model_t::columnCount(QModelIndex const &) const -> int
  {
  return int(column_e::column_max);  // System, Class, Status
  }

[[nodiscard]]
auto route_model_t::data(QModelIndex const & index, int role) const -> QVariant
  {
  if(not index.isValid() or index.row() >= static_cast<int>(route_.size()))
    return {};

  auto const & item = route_[static_cast<std::size_t>(index.row())];

  if(role == Qt::DisplayRole)
    {
    switch(column_e(index.column()))
      {
      case column_e::system: return QString::fromStdString(item.system);
      case column_e::star_type:
          {
          static constexpr std::string_view kgbfoam = "KGBFOAM";
          bool is_scoopable = item.star_class.length() == 1 && kgbfoam.contains(item.star_class[0]);
          // Unicode fuel pump: \u26FD
          return is_scoopable ? QString::fromUtf8("\u26FD ") + QString::fromStdString(item.star_class)
                              : QString::fromStdString(item.star_class);
          }
      case column_e::visited: return item.visited ? "Visited" : "Pending";
      case column_e::distance: return item.distance;
      default: break;
      }
    }

  if(role == Qt::ForegroundRole and item.visited)
    return QBrush(Qt::gray);

  return {};
  }

auto route_model_t::update_data(std::vector<info::route_item_t> && new_route) -> void
  {
  beginResetModel();
  route_ = std::move(new_route);
  endResetModel();
  }

// the rest of the methods (index, parent, hasChildren) - the standard implementation of a flat model
[[nodiscard]]
auto route_model_t::index(int r, int c, QModelIndex const & p) const -> QModelIndex
  {
  return hasIndex(r, c, p) ? createIndex(r, c) : QModelIndex{};
  }

[[nodiscard]]
auto route_model_t::parent(QModelIndex const &) const -> QModelIndex
  {
  return {};
  }

[[nodiscard]]
auto route_model_t::hasChildren(QModelIndex const & p) const -> bool
  {
  return !p.isValid();
  }

[[nodiscard]]
auto route_model_t::flags(QModelIndex const & i) const -> Qt::ItemFlags
  {
  return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
  }

[[nodiscard]]
auto route_model_t::headerData(int s, Qt::Orientation o, int r) const -> QVariant
  {
  if(r != Qt::DisplayRole || o != Qt::Horizontal)
    return {};
  switch(column_e(s))
    {
    case column_e::system:  return "System";
    case column_e::star_type:  return "Class";
    case column_e::visited:  return "Status";
    case column_e::distance:  return "Distance";
    default: return {};
    }
  }

namespace spansh
  {
///\brief the shape of the neutron plotter's file - we read from it only what is actually flown
///\detail the structs need external linkage, because glaze reflection does not reach into an anonymous
/// namespace; unknown keys are skipped, so the rest of the file is no trouble
struct jump_t
  {
  std::string system;
  uint64_t id64;
  bool neutron_star;
  double x;
  double y;
  double z;
  };

struct result_t
  {
  std::vector<jump_t> system_jumps;
  };

struct route_t
  {
  result_t result;
  };
  }  // namespace spansh

route_window_t::route_window_t(current_state_t const & state, std::string db_path, QWidget * parent) :
    QMdiSubWindow(parent),
    state_(state),
    db_{db_path}
  {
  if(auto res{db_.open()}; not res)
    spdlog::error("route window: failed to open {}", db_path);

  setup_ui();

  // the remembered route comes back on its own - that is what it was remembered for
  if(auto saved{db_.load_neutron_route()}; saved and not saved->empty())
    {
    neutron_name_ = saved->front().route_name;
    neutron_route_ = std::move(*saved);
    remembered_ = true;
    }

  show_route();
  }

auto route_window_t::load_from_file() -> void
  {
  auto const chosen{QFileDialog::getOpenFileName(this, "Neutron route (spansh)", {}, "JSON (*.json)")};
  if(chosen.isEmpty())
    return;

  spansh::route_t parsed{};
  std::string buffer;
  if(auto res{glz::read_file_json<glz::opts{.error_on_unknown_keys = false}>(parsed, chosen.toStdString(), buffer)};
     res)
    {
    info_label_->setText(QString{"could not read the route: %1"}.arg(chosen));
    spdlog::error("route window: failed to parse {}", chosen.toStdString());
    return;
    }

  if(parsed.result.system_jumps.empty())
    {
    info_label_->setText("the file contains no waypoints");
    return;
    }

  neutron_name_ = QFileInfo{chosen}.completeBaseName().toStdString();
  remembered_ = false;

  std::vector<info::neutron_waypoint_t> loaded;
  loaded.reserve(parsed.result.system_jumps.size());
  for(spansh::jump_t const & jump: parsed.result.system_jumps)
    loaded.push_back(info::neutron_waypoint_t{
      .route_name = neutron_name_,
      .position = {},
      .system = jump.system,
      .system_address = jump.id64,
      .loc_x = jump.x,
      .loc_y = jump.y,
      .loc_z = jump.z,
      .neutron = jump.neutron_star,
      .distance = {}
    });

  reached_ = 0u;
  apply_direction(std::move(loaded));
  }

auto route_window_t::apply_direction(std::vector<info::neutron_waypoint_t> route) -> void
  {
  if(reversed_box_->isChecked())
    std::ranges::reverse(route);

  // The distances in the file describe the way in the file's own direction, so after a reversal they no
  // longer fit. The coordinates always fit, so we count them anew between neighbours
  for(size_t ix{}; ix < route.size(); ++ix)
    {
    route[ix].position = uint32_t(ix);
    route[ix].distance
      = ix == 0u ? 0.0
                 : info::distance(
                     info::space_location_t{route[ix].loc_x, route[ix].loc_y, route[ix].loc_z},
                     info::space_location_t{route[ix - 1u].loc_x, route[ix - 1u].loc_y, route[ix - 1u].loc_z}
                   );
    }

  neutron_route_ = std::move(route);
  clipboard_target_.clear();
  reached_ = 0u;
  progress_system_ = 0u;
  show_route();
  }

auto route_window_t::setup_ui() -> void
  {
  setWindowTitle("Route");
  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  auto * controls = new QHBoxLayout();
  load_button_ = new QPushButton("Load route...", central_widget);
  reversed_box_ = new QCheckBox("Reversed", central_widget);
  reversed_box_->setToolTip("Fly the route from its end - set before loading, or change it on a loaded route");
  remember_button_ = new QPushButton("Remember", central_widget);
  forget_button_ = new QPushButton("Forget", central_widget);
  clear_button_ = new QPushButton("Clear", central_widget);
  clear_button_->setToolTip("Drop the route loaded for one trip - the remembered route, if there is one, comes back");
  destination_button_ = new QPushButton("Copy next destination", central_widget);
  destination_button_->setToolTip("Put the next waypoint on the clipboard, for the galaxy map's search");
  controls->addWidget(load_button_);
  controls->addWidget(reversed_box_);
  controls->addWidget(remember_button_);
  controls->addWidget(forget_button_);
  controls->addWidget(clear_button_);
  controls->addWidget(destination_button_);
  controls->addStretch(1);

  info_label_ = new QLabel(central_widget);
  table_view_ = new QTableView(central_widget);

  model_ = new route_model_t({}, this);
  table_view_->setModel(model_);
    {
    auto * header{table_view_->horizontalHeader()};
    header->setStretchLastSection(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    for(int i{0}; i < model_->columnCount() - 1; ++i)
      header->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(model_->columnCount() - 1, QHeaderView::Stretch);
    }
  layout->addLayout(controls);
  layout->addWidget(info_label_);
  layout->addWidget(table_view_);

  setWidget(central_widget);

  connect(load_button_, &QPushButton::clicked, this, [this] { load_from_file(); });

  // joining the route halfway, or stepping back after a mistake - clicking a row sets the progress
  connect(table_view_, &QTableView::doubleClicked, this, [this](QModelIndex const & index) {
    jump_to_waypoint(index.row());
  });
  connect(reversed_box_, &QCheckBox::toggled, this, [this](bool) {
    // the direction changes in place, without reaching for the file again
    if(not neutron_route_.empty())
      apply_direction(std::move(neutron_route_));
  });
  connect(destination_button_, &QPushButton::clicked, this, [this] {
    if(reached_ >= neutron_route_.size())
      return;
    std::string const & next{neutron_route_[reached_].system};
    if(set_destination_)
      set_destination_(next);
    else
      QGuiApplication::clipboard()->setText(QString::fromStdString(next));
  });
  connect(remember_button_, &QPushButton::clicked, this, [this] {
    if(auto res{db_.store_neutron_route(neutron_route_)}; not res)
      spdlog::error("route window: failed to remember route");
    else
      remembered_ = true;
    show_route();
  });
  connect(forget_button_, &QPushButton::clicked, this, [this] {
    if(auto res{db_.store_neutron_route({})}; not res)
      spdlog::error("route window: failed to forget route");
    drop_route();
    show_route();
  });
  connect(clear_button_, &QPushButton::clicked, this, [this] {
    drop_route();
    // a one-off route only covered the remembered one, which stays stored - it is shown again, as at start
    if(auto saved{db_.load_neutron_route()}; saved and not saved->empty())
      {
      neutron_name_ = saved->front().route_name;
      neutron_route_ = std::move(*saved);
      remembered_ = true;
      }
    show_route();
  });

  refresh_ui();
  }

auto route_window_t::refresh_ui() -> void { show_route(); }

auto route_window_t::drop_route() -> void
  {
  neutron_route_.clear();
  neutron_name_.clear();
  clipboard_target_.clear();
  reached_ = 0u;
  progress_system_ = 0u;
  remembered_ = false;
  }

auto route_window_t::set_destination_handler(std::function<void(std::string const &)> handler) -> void
  {
  set_destination_ = std::move(handler);
  destination_button_->setText(set_destination_ ? "Set next destination" : "Copy next destination");
  destination_button_->setToolTip(
    set_destination_ ? "Make the next waypoint the destination" : "Put the next waypoint on the clipboard, for the galaxy map's search"
  );
  }

auto route_window_t::show_route() -> void
  {
  bool const loaded{not neutron_route_.empty()};
  remember_button_->setEnabled(loaded and not remembered_);
  forget_button_->setEnabled(loaded and remembered_);
  clear_button_->setEnabled(loaded and not remembered_);
  destination_button_->setEnabled(loaded and reached_ < neutron_route_.size());

  if(not loaded)
    {
    // with no route from a file the window shows what the game plotted
    auto current_route{state_.route_};
    auto remaining{current_route | std::views::filter([](auto const & item) { return not item.visited; })};
    auto const remaining_count{std::ranges::distance(remaining)};

    QString next_system{"Destination Reached"};
    if(remaining_count > 0)
      next_system = QString::fromStdString(remaining.front().system);

    info_label_->setText(QString{"Jumps remaining: %1 | Next: %2"}.arg(remaining_count).arg(next_system));
    model_->update_data(std::move(current_route));
    return;
    }

  // Where we are on the route. We look by the system address rather than by name - names are sometimes
  // identical for different places, addresses are not. Only at an arrival, and only from the next waypoint
  // on: a route there and back passes the same system again, and its earlier place is behind us
  if(state_.current_system_address_ != progress_system_)
    {
    progress_system_ = state_.current_system_address_;
    auto const ahead{neutron_route_ | std::views::drop(reached_)};
    auto const here{std::ranges::find(
      ahead,
      state_.current_system_address_,
      [](info::neutron_waypoint_t const & waypoint) -> uint64_t { return waypoint.system_address; }
    )};

    // Progress only moves forward: a system outside the list means "somewhere along the way", not "back to
    // the start". The game plots its own course to the next waypoint and sometimes leads through systems in
    // between
    if(here != ahead.end())
      reached_ += size_t(std::ranges::distance(ahead.begin(), here)) + 1u;
    }

  size_t const reached{reached_};

  std::vector<info::route_item_t> shown;
  shown.reserve(neutron_route_.size());
  for(size_t ix{}; ix < neutron_route_.size(); ++ix)
    {
    info::neutron_waypoint_t const & waypoint{neutron_route_[ix]};
    shown.push_back(info::route_item_t{
      .system = waypoint.system,
      .system_address = waypoint.system_address,
      .star_location = info::space_location_t{waypoint.loc_x, waypoint.loc_y, waypoint.loc_z},
      .star_class = waypoint.neutron ? std::string{"N"} : std::string{},
      .distance = waypoint.distance,
      .visited = ix < reached
    });
    }

  std::string const next{reached < neutron_route_.size() ? neutron_route_[reached].system : std::string{}};

  // The target goes to the clipboard only when it has changed. Putting it there at every refresh would
  // tread on whatever the user has just copied themselves
  bool copied{};
  if(not next.empty() and next != clipboard_target_)
    {
    clipboard_target_ = next;
    QGuiApplication::clipboard()->setText(QString::fromStdString(next));
    copied = true;
    }

  QString const name{
    QString::fromStdString(neutron_name_) + (remembered_ ? " [remembered]" : " [one-off]")
  };

  // The target first - in a narrow window the label is cut off on the right, and the target is exactly
  // what has to be read after every jump; the route's name is the least urgent thing here
  info_label_->setText(
    next.empty() ? QString{"Arrived | %1"}.arg(name)
                 // numerator and denominator in the same units - stops, not jumps
                 : QString{"Next: %1%2 | %3 of %4 stops left | %5"}
                     .arg(QString::fromStdString(next))
                     .arg(copied ? "  (copied)" : "")
                     .arg(neutron_route_.size() - reached)
                     .arg(neutron_route_.size())
                     .arg(name)
  );

  model_->update_data(std::move(shown));
  }

auto route_window_t::jump_to_waypoint(int row) -> void
  {
  if(neutron_route_.empty() or row < 0 or size_t(row) >= neutron_route_.size())
    return;

  // the waypoint clicked becomes the one being flown to - so every one before it is behind us, and the
  // system we are in does not move it on until we leave
  reached_ = size_t(row);
  progress_system_ = state_.current_system_address_;
  clipboard_target_.clear();
  show_route();
  }
