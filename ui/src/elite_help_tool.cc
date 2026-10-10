#include <eht_settings.h>
#include <evidence_log.h>
#include <graphics_profile.h>
#include <netstate.h>
#include <event_guard.h>
#include <main_window.h>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

#include <qapplication.h>
#include <qguiapplication.h>
#include <qsplitter.h>
#include <qtimer.h>
#include <csignal>
#include <qpalette.h>
#include <qstylefactory.h>
#include <qmdisubwindow.h>
#include <qsettings.h>
#include <qtoolbar.h>
#include <qpushbutton.h>
#include <qboxlayout.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qdesktopservices.h>
#include <qurl.h>
#include <string_view>
#include <vector>
#include <qprogressbar.h>
#include <qscrollarea.h>
#include <qgroupbox.h>
#include <qicon.h>
#include <qmessagebox.h>

namespace
  {
///\brief the tool windows live for as long as the application runs
///\detail each exists in a single instance and has its own button on the toolbar; closing one would leave
/// the button without a window, so the close event is swallowed
class close_blocker_t final : public QObject
  {
public:
  using QObject::QObject;

protected:
  auto eventFilter(QObject * watched, QEvent * event) -> bool override
    {
    if(event->type() == QEvent::Close)
      {
      event->ignore();
      return true;
      }
    return QObject::eventFilter(watched, event);
    }
  };
  }  // namespace

Q_DECLARE_METATYPE(window_type_e)

main_window_t::main_window_t(std::string db_path, std::string journal_path, QWidget * parent) :
    QMainWindow(parent),
    db_path_{db_path},
    state_{this, db_path, journal_path}
  {
  setup_ui();
  load_settings();

  // the server comes up independently of the game - which may start before the tool, or not at all
  overlay_feed_ = std::make_unique<overlay_feed_t>(overlay::default_socket_path(), db_path);

  overlay_timer_ = new QTimer(this);
  overlay_timer_->setInterval(static_cast<int>(eht::settings()->overlay.refresh.publish_ms));
  connect(
    overlay_timer_,
    &QTimer::timeout,
    this,
    [this]
    {
      publish_guard_(
        "overlay publish",
        [this]
        {
          // the pace follows the settings file, which can change while the tool runs
          if(
            int const pace{static_cast<int>(std::max(100u, eht::settings()->overlay.refresh.publish_ms))};
            overlay_timer_->interval() != pace
          )
            overlay_timer_->setInterval(pace);
          publish_overlay();
        }
      );
    }
  );
  overlay_timer_->start();

  // a sample's picture is counted down in the game from the moment its request arrives, so the request
  // goes out as soon as the scan is seen rather than at the next tick
  auto * const picture_timer{new QTimer(this)};
  picture_timer->setInterval(50);
  connect(
    picture_timer,
    &QTimer::timeout,
    this,
    [this]
    {
      publish_guard_(
        "overlay publish",
        [this]
        {
          if(overlay_feed_ and overlay_feed_->picture_due(state_))
            publish_overlay();
        }
      );
    }
  );
  picture_timer->start();

  // the backup looks whether it is due every ten minutes, the first time a minute after the start - once the
  // journal has told whose it is
  auto * const backup_timer{new QTimer(this)};
  backup_timer->setInterval(std::chrono::minutes{10});
  connect(backup_timer, &QTimer::timeout, this, [this] { eht::event_guard("backup", [this] { follow_backup(); }); });
  backup_timer->start();
  QTimer::singleShot(std::chrono::minutes{1}, this, [this] { eht::event_guard("backup", [this] { follow_backup(); }); });
  }

auto main_window_t::follow_backup() -> void
  {
  if(backup_.valid())
    {
    if(backup_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
      return;
    backup::summary_t const summary{backup_.get()};
    for(std::string const & error: summary.errors)
      spdlog::error("backup: {}", error);
    if(summary.errors.empty())
      if(
        auto const written{backup::write_mark(
          backup_destination_,
          backup::mark_t{
            .at = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()), .pictures = backup_pictures_
          }
        )};
        not written
      )
        // without the mark the next look finds the backup due again, and the whole of it runs every ten minutes
        spdlog::error("backup: its mark could not be written in {}: {}", backup_destination_.string(), written.error().message());
    spdlog::info(
      "backup: {} month(s) of journals packed{}, {} file(s) of the codex copied{}, in {}",
      summary.months.size(),
      summary.months.empty() ? std::string{} : std::format(" ({})", summary.months.back()),
      summary.pictures_copied,
      summary.live_db_copied ? ", live.sqlite refreshed" : "",
      backup_destination_.string()
    );
    return;
    }

  auto const cfg{eht::settings()};
  if(not cfg->backup.enabled or state_.commander_name_.empty())
    return;
  // a directory for each commander - two accounts on one machine keep their journals apart
  std::filesystem::path const destination{
    backup::expand_home(cfg->backup.dir) / codex_files::file_safe(state_.commander_name_)
  };

  // the settings at every look - a few small files, and a verification of the game's files may take them any day
  std::filesystem::path const journal_dir{state_.journal_dir_path_};
  if(not cfg->backup.game_dir.empty())
    backup_game_dir_ = backup::expand_home(cfg->backup.game_dir);
  else
    {
    std::error_code ec;
    std::filesystem::path cwd;
    if(auto const pid{netstate::find_game(journal_dir)}; pid)
      cwd = std::filesystem::read_symlink(std::format("/proc/{}/cwd", *pid), ec);
    if(auto found{backup::game_dir_of(evidence::find_netlog_dir(journal_dir, cwd))};
       not found.empty() and found != backup_game_dir_)
      {
      spdlog::info("backup: the game's directory is {}", found.string());
      backup_game_dir_ = std::move(found);
      }
    }
  std::filesystem::path const options_dir{graphics_profile::graphics_dir_of(journal_dir).parent_path()};
  backup::settings_summary_t const settings{backup::copy_settings(
    destination,
    backup::settings_sources_t{
      .options_dir = options_dir, .game_dir = backup_game_dir_, .tool_settings = std::filesystem::path{eht::settings_file_name}
    }
  )};
  for(std::string const & error: settings.errors)
    spdlog::error("backup: {}", error);
  if(settings.copied != 0u)
    spdlog::info("backup: {} file(s) of the settings copied to {}", settings.copied, (destination / "settings").string());
  // the closed session logs of edworld - small, a few megabytes before packing, and gone with a verification
  backup::mod_logs_summary_t const mod_logs{backup::move_mod_logs(destination, backup_game_dir_, 3)};
  for(std::string const & error: mod_logs.errors)
    spdlog::error("backup: {}", error);
  if(mod_logs.moved != 0u)
    spdlog::info("backup: {} closed edworld log(s) packed into {}", mod_logs.moved, (destination / "mod-logs").string());
  auto const pictures{backup::count_pictures(codex_files::codex_dir())};
  if(not pictures)
    {
    // the count decides when the next backup is due, so a wrong one is not guessed at
    spdlog::error("backup: the codex pictures could not be counted: {}", pictures.error().message());
    return;
    }
  auto const mark{backup::read_mark(destination)};
  if(not mark)
    spdlog::error("backup: the mark of the last backup could not be read, a backup is due: {}", mark.error().message());
  auto const now{std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now())};
  if(not backup::due(mark.value_or(backup::mark_t{}), now, *pictures, cfg->backup.every_days, cfg->backup.every_pictures))
    return;

  backup_destination_ = destination;
  backup_pictures_ = *pictures;
  spdlog::info("backup: due, writing to {}", destination.string());
  backup_ = std::async(
    std::launch::async,
    [destination, journals = std::filesystem::path{state_.journal_dir_path_}, codex = codex_files::codex_dir(),
     live_db = std::filesystem::path{state_.db_.live_db_path_}, level = cfg->backup.level]
    { return backup::run(destination, journals, codex, live_db, level); }
  );
  }

auto main_window_t::publish_overlay() -> void
  {
  if(not overlay_feed_)
    return;

  // a route plotted outside the game is known only to the Route window - the overlay has nowhere to reach for it
  overlay_feed_t::plotted_route_t plotted{};
  if(route_view_)
    plotted = overlay_feed_t::plotted_route_t{
      .waypoints = route_view_->neutron_route_, .reached = route_view_->reached_, .name = route_view_->neutron_name_
    };

  // the site chosen in the Construction window goes to the overlay while that window is the current one -
  // not the active one, which there is none of while the game has the focus
  uint64_t construction_focus{};
  if(construction_view_)
    {
    construction_view_->refresh_ui();
    if(mdi_area_->currentSubWindow() == construction_view_ and not construction_view_->isMinimized())
      construction_focus = construction_view_->selected_market();
    }
  overlay_feed_->set_construction_focus(construction_focus);
  if(ships_view_)
    ships_view_->refresh_ui();
  if(community_goal_view_)
    community_goal_view_->refresh_ui();
  if(surface_view_)
    {
    surface_view_->refresh_ui();
    overlay_feed_->set_surface_target(surface_view_->target());
    }
  overlay_feed_->set_extension_hint(extension_ ? extension_->overlay_hint() : std::string{});
  overlay_feed_->publish(state_, plotted);
  // a backup running is looked at with every frame, so its end is written down when it comes
  if(backup_.valid())
    follow_backup();

  if(extension_)
    extension_->tick(eht::extension::route_view_t{.waypoints = plotted.waypoints, .reached = plotted.reached});
  }

auto main_window_t::choose_opening_window(bool inhabited) -> void
  {
  // once only: after this the windows are the user's business, and the ship moving from one system
  // to another is no reason to take the view away from whatever they were reading
  if(std::exchange(opening_settled_, true))
    return;

  // Where there are factions there is something to read about them; where there are none there is
  // nothing but what the scanner found, and the faction window would open on an empty table
  spdlog::debug("opening on {}", inhabited ? "system info" : "exploration");
  activate_window(inhabited ? window_type_e::faction_state : window_type_e::system);
  }

auto main_window_t::start_monitoring() -> void
  {
  monitoring_started_ = std::chrono::steady_clock::now();
  // the thread touches db_, so it starts only once that is open
  // EDDN hears every line the journal thread reads; the held scans wait in the tool's own directory
  eddn_sender_ = std::make_unique<eddn::sender_t>();
  eddn_publisher_ = std::make_unique<eddn::publisher_t>(
    std::filesystem::path{state_.journal_dir_path_},
    std::filesystem::path{"eddn_held.jsonl"},
    [this](eddn::message_t && message) { eddn_sender_->enqueue(std::move(message)); }
  );
  extension_ = eht::extension::make_extension(std::filesystem::path{state_.journal_dir_path_});
  if(extension_ and extension_->sets_destination() and route_view_)
    route_view_->set_destination_handler([this](std::string const & system) { extension_->set_destination(system); });
  state_.raw_line_listener_ = [this](std::string_view line, bool live)
  {
    eddn_publisher_->feed(line, live);
    if(extension_)
      extension_->journal_line(line, live);
  };

  worker_thread_ = std::jthread(
    [this](std::stop_token stoken)
    {
      // with this thread gone the journal is no longer followed; the window stays for what it already knows
      eht::event_guard("journal reader", [this, &stoken] { background_worker(stoken); });
    }
  );
  }

auto main_window_t::setup_ui() -> void
  {
  resize(1200, 800);
  setWindowTitle("Elite Dangerous Help Tool");

  // the central area for the subwindows
  mdi_area_ = new QMdiArea(this);
  mdi_area_->setViewMode(QMdiArea::SubWindowView);
  setCentralWidget(mdi_area_);

  close_blocker_ = new close_blocker_t(this);

  setup_toolbox();

  system_view_ = new system_window_t(state_);
  add_tool_window(system_view_, window_type_e::system);

  ship_view_ = new ship_loadout_window_t(state_.ship_loadout);
  add_tool_window(ship_view_, window_type_e::ship);

  jlw_ = new journal_log_window_t{};
  add_tool_window(jlw_, window_type_e::journal_log);

  mission_view_ = new mission_window_t{state_};
  add_tool_window(mission_view_, window_type_e::mission);

  route_view_ = new route_window_t{state_, db_path_};
  add_tool_window(route_view_, window_type_e::route);

  faction_view_ = new faction_window_t{state_};
  add_tool_window(faction_view_, window_type_e::faction);

  faction_state_view_ = new faction_state_window_t{state_, db_path_};
  add_tool_window(faction_state_view_, window_type_e::faction_state);

  micro_resource_view_ = new micro_resource_window_t{db_path_};
  add_tool_window(micro_resource_view_, window_type_e::micro_resource);

  bgs_view_ = new bgs_window_t{db_path_};
  add_tool_window(bgs_view_, window_type_e::bgs);

  construction_view_ = new construction_window_t{state_, db_path_};
  add_tool_window(construction_view_, window_type_e::construction);
  // a site finished while its window was on top: the window would move on to another site, while the
  // commander now stands at a new settlement or station - the system shows it
  connect(
    construction_view_,
    &construction_window_t::site_finished,
    this,
    [this](uint64_t)
    {
      if(mdi_area_->currentSubWindow() == construction_view_)
        activate_window(window_type_e::system);
    }
  );

  ships_view_ = new ships_window_t{state_, db_path_};
  add_tool_window(ships_view_, window_type_e::ships);

  surface_view_ = new surface_window_t{state_};
  add_tool_window(surface_view_, window_type_e::surface);

  network_incident_view_ = new network_incident_window_t{db_path_, state_.journal_dir_path_};
  add_tool_window(network_incident_view_, window_type_e::network_incident);

  credits_view_ = new credits_window_t{state_.journal_dir_path_};
  add_tool_window(credits_view_, window_type_e::credits);
  community_goal_view_ = new community_goal_window_t{state_};
  add_tool_window(community_goal_view_, window_type_e::community_goal);
  }

auto main_window_t::add_tool_window(QMdiSubWindow * sub, window_type_e type) -> void
  {
  mdi_area_->addSubWindow(sub);
  sub->setProperty("window_type", QVariant::fromValue(type));

  // no close button - there is one window of each and it is to live to the end of the session
  // WindowSystemMenuHint would add the window menu with a close entry, so it is not here
  sub->setWindowFlags(Qt::SubWindow | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint);
  sub->installEventFilter(close_blocker_);
  sub->show();
  }

auto main_window_t::subwindow_for(window_type_e type) const -> QMdiSubWindow *
  {
  switch(type)
    {
    case window_type_e::system:        return system_view_;
    case window_type_e::ship:          return ship_view_;
    case window_type_e::mission:       return mission_view_;
    case window_type_e::route:         return route_view_;
    case window_type_e::faction:       return faction_view_;
    case window_type_e::faction_state: return faction_state_view_;
    case window_type_e::micro_resource: return micro_resource_view_;
    case window_type_e::bgs: return bgs_view_;
    case window_type_e::construction: return construction_view_;
    case window_type_e::ships:        return ships_view_;
    case window_type_e::surface:      return surface_view_;
    case window_type_e::network_incident: return network_incident_view_;
    case window_type_e::credits:      return credits_view_;
    case window_type_e::community_goal: return community_goal_view_;
    case window_type_e::journal_log:   return jlw_;
    case window_type_e::none:          break;
    }
  return nullptr;
  }

auto main_window_t::activate_window(window_type_e type) -> void
  {
  auto * sub{subwindow_for(type)};
  if(not sub)
    return;

  if(sub->mdiArea() == nullptr)
    mdi_area_->addSubWindow(sub);

  // a collapsed window has to be restored first; raise() alone would not show it
  if(sub->isMinimized())
    sub->showNormal();
  else
    sub->show();

  sub->raise();
  mdi_area_->setActiveSubWindow(sub);

  // BGS work changes with every mission handed in and every recalculation, and the window reads the
  // database on a connection of its own - reaching for it from the bar is the natural moment to refresh
  if(type == window_type_e::bgs and bgs_view_)
    bgs_view_->refresh_ui();
  }

auto main_window_t::setup_toolbox() -> void
  {
  auto * toolbox_dock = new QToolBar("Toolbox", this);
  toolbox_dock->setMovable(false);
  addToolBar(Qt::LeftToolBarArea, toolbox_dock);

  // one button per window; clicking it brings the window to the front
  struct tool_button_t
    {
    window_type_e type;
    QString label;
    };

  std::vector<tool_button_t> const tools{
    {window_type_e::system, "Exploration"},
    {window_type_e::faction_state, "System info"},
    {window_type_e::faction, "Reputation"},
    {window_type_e::mission, "Missions"},
    {window_type_e::route, "Route"},
    {window_type_e::ship, "Ship"},
    {window_type_e::micro_resource, "Data"},
    {window_type_e::bgs, "BGS"},
    {window_type_e::construction, "Construction"},
    {window_type_e::ships, "Ships"},
    {window_type_e::surface, "Surface"},
    {window_type_e::network_incident, "Network"},
    {window_type_e::credits, "Credits"},
    {window_type_e::community_goal, "Community goals"},
    {window_type_e::journal_log, "Log"}
  };

  for(tool_button_t const & tool: tools)
    {
    auto * btn = new QPushButton(tool.label, this);
    toolbox_dock->addWidget(btn);

    connect(btn, &QPushButton::clicked, this, [this, type = tool.type]() { activate_window(type); });
    }

  // not a window of the tool but a page beside it - what was sampled, where, and what it looked like
  auto * codex = new QPushButton("Codex", this);
  toolbox_dock->addWidget(codex);
  connect(
    codex,
    &QPushButton::clicked,
    this,
    []() { QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(codex_t::page_path().string()))); }
  );

  // the stars and planets photographed as the ship looked at them
  auto * sky = new QPushButton("Sky", this);
  toolbox_dock->addWidget(sky);
  connect(
    sky,
    &QPushButton::clicked,
    this,
    []() { QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(sky_album_t::page_path().string()))); }
  );
  }

auto main_window_t::save_settings() -> void
  {
  QSettings settings("ebasoft", "EliteHelpTool");

  settings.beginGroup("main_window");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("state", saveState());
  settings.endGroup();

  settings.beginWriteArray("sub_windows");
  auto windows = mdi_area_->subWindowList();

  for(std::size_t i = 0; i < windows.size(); ++i)
    {
    settings.setArrayIndex(static_cast<int>(i));
    auto * sub = windows[i];
    settings.setValue("title", sub->windowTitle());
    settings.setValue("type", static_cast<int>(sub->property("window_type").value<window_type_e>()));
    }
  settings.endArray();

  // how the areas inside the windows are divided - stored by the splitter's name, so new windows get
  // this for free as long as a name is given
  settings.beginGroup("splitters");
  for(QSplitter const * splitter: findChildren<QSplitter *>())
    if(not splitter->objectName().isEmpty())
      settings.setValue(splitter->objectName(), splitter->saveState());
  settings.endGroup();
  }

auto main_window_t::load_settings() -> void
  {
  QSettings settings("ebasoft", "EliteHelpTool");

  settings.beginGroup("main_window");
  if(auto geo = settings.value("geometry"); geo.isValid())
    restoreGeometry(geo.toByteArray());
  if(auto state = settings.value("state"); state.isValid())
    restoreState(state.toByteArray());
  settings.endGroup();

  auto const count = settings.beginReadArray("sub_windows");
  for(int i = 0; i < count; ++i)
    {
    settings.setArrayIndex(i);
    auto type_int = settings.value("type").toInt();
    auto type = static_cast<window_type_e>(type_int);

    // settings from before a rebuild of the toolbar can hold stand-in windows that no longer exist
    QMdiSubWindow * sub{subwindow_for(type)};
    if(sub) [[likely]]
      {
      if(sub->mdiArea() == nullptr)
        mdi_area_->addSubWindow(sub);

      // the windows are switched with the bar on the left, so each is to take up the whole work area
      sub->showMaximized();
      }
    }
  settings.endArray();

  settings.beginGroup("splitters");
  for(QSplitter * splitter: findChildren<QSplitter *>())
    if(auto const state{settings.value(splitter->objectName())}; state.isValid())
      splitter->restoreState(state.toByteArray());
  settings.endGroup();
  }

auto main_window_t::background_worker(std::stop_token stoken) -> void
  {
  // How far this database was read last time, taken before a single line is replayed. Everything
  // stamped at or before it is already written down
  if(auto progress{state_.db_.load_journal_progress()}; progress and *progress)
    {
    state_.resume_from_ = **progress;
    spdlog::info("journal already read up to {:%Y-%m-%dT%H:%M:%SZ}", state_.resume_from_);
    }

  // the first filling of the faction list - db_ is touched from this thread alone
  state_.load_factions();
  state_.load_phenomena();
  QMetaObject::invokeMethod(
    this,
    [this]()
    {
      if(faction_view_)
        faction_view_->refresh_ui();
      if(faction_state_view_)
        faction_state_view_->refresh_ui();
    },
    Qt::QueuedConnection
  );

  // restarting the game creates a new journal; the following switches to it on its own
  tail_journal_dir(
    state_.journal_dir_path_,
    // one event that cannot be handled is skipped and said; it must not end the reading of the journal
    [this](std::string_view line)
    {
      if(not eht::event_guard("journal event", [this, line] { state_.discovery(line); }))
        spdlog::error("journal event skipped: {}", line.substr(0u, 200u));
    },
    stoken,
    [this](fs::path const & path)
    {
      spdlog::info("monitoring journal {}", path.string());
      // a new journal is a new session of the game: whatever was locked on is not locked on now
      state_.forget_live_combat();
      QMetaObject::invokeMethod(
        this, [this, path]() { file_to_monitor = path; }, Qt::QueuedConnection
      );
    },
    [this]()
    {
      // the replay has reached the present. Whatever it said about a target was true at the time and
      // is not true now, and only from here is the state a picture of where the ship actually is -
      // which is the first moment the choice of window can be made on anything but a guess
      spdlog::info(
        "caught up: {} events handled, {} walked past as already written",
        state_.events_handled_,
        state_.events_walked_past_
      );
      // from here the stream is live, and nothing in it is ever skipped again
      state_.catching_up_ = false;
      state_.forget_live_combat();
      state_.remember_progress();
      bool const inhabited{not state_.system_factions.empty()};
      QMetaObject::invokeMethod(
        this,
        [this, inhabited]()
        {
          choose_opening_window(inhabited);
          // the route's progress stood still through the replay; the system we are in now moves it on
          if(route_view_)
            route_view_->refresh_ui();
        },
        Qt::QueuedConnection
      );
    }
  );
  }

namespace
  {
///\brief a dark palette for when there is no desktop theme
[[nodiscard]]
auto dark_palette() -> QPalette
  {
  QColor const window{0x2b, 0x2b, 0x2b};
  QColor const text{0xdd, 0xdd, 0xdd};
  QColor const dimmed{0x77, 0x77, 0x77};

  QPalette palette;
  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, QColor{0x23, 0x23, 0x23});
  palette.setColor(QPalette::AlternateBase, QColor{0x31, 0x31, 0x31});
  palette.setColor(QPalette::ToolTipBase, window);
  palette.setColor(QPalette::ToolTipText, text);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::PlaceholderText, dimmed);
  palette.setColor(QPalette::Button, window);
  palette.setColor(QPalette::ButtonText, text);
  palette.setColor(QPalette::BrightText, Qt::red);
  palette.setColor(QPalette::Link, QColor{0x4a, 0x90, 0xd9});
  palette.setColor(QPalette::Highlight, QColor{0x2a, 0x63, 0x9c});
  palette.setColor(QPalette::HighlightedText, Qt::white);

  palette.setColor(QPalette::Disabled, QPalette::Text, dimmed);
  palette.setColor(QPalette::Disabled, QPalette::WindowText, dimmed);
  palette.setColor(QPalette::Disabled, QPalette::ButtonText, dimmed);
  return palette;
  }

///\brief forces a dark theme when the system does not impose one
///\detail started over ssh -X it lands on a machine with no desktop settings and qt comes up light.
/// styleHints()->setColorScheme does nothing there - without a desktop theme qt leaves the scheme Unknown
/// and the palette unchanged, so the palette goes in directly. Whether it is needed at all is decided by
/// the lightness of the background, because colorScheme is sometimes Unknown on a dark desktop too.
auto apply_dark_theme() -> void
  {
  if(QApplication::palette().color(QPalette::Window).lightness() < 128)
    return;

  QApplication::setStyle(QStyleFactory::create("Fusion"));
  QApplication::setPalette(dark_palette());
  }
  }  // namespace

namespace
  {
///\brief set from a signal handler, so this and nothing more - the rest happens in the event loop
volatile std::sig_atomic_t asked_to_stop{};
  }  // namespace

auto main(int argc, char * argv[]) -> int
  {
  QApplication app(argc, argv);
  QApplication::setWindowIcon(QIcon{":/icons/eht.png"});
  apply_dark_theme();

  // a look at the queries without a rebuild - SPDLOG_LEVEL=debug
  spdlog::cfg::load_env_levels();

  // the settings live beside the databases, in the directory the tool runs in; saved again, they apply
  // at once - the overlay's among them, since the layer takes its layout from us with every frame
  eht::load_settings(eht::settings_file_name);
  eht::settings_watcher_t const settings_watcher{std::filesystem::path{eht::settings_file_name}};

  main_window_t window{"ehtdb.sqlite", "journal-dir"};
  if(auto const res{window.state_.db_.open()}; not res)
    {
    // a database written by a newer EHT stops the older one here, rather than have it rewrite rows in
    // a shape it only half understands
    QString text{res.error() == std::errc::not_supported
                   ? QString{
                       "This database was written by a newer version of EHT than this build "
                       "understands. Continuing could lose data, so EHT is stopping here instead.\n\n"
                       "The log names the file. Best: run the newer EHT that wrote it.\n\n"
                       "ehtdb.sqlite and galaxy.sqlite can be backed up and rebuilt from your journals:\n\n"
                       "journal_tailer --dir \"<your journal folder>\" --commander <your FID>\n\n"
                       "(see the journal's own LoadGame event for the FID, and README.md for details). "
                       "live.sqlite - markets and carrier readings - cannot be rebuilt; only the newer EHT "
                       "opens it."
                     }
                   : QString::fromStdString(std::format("Could not open the database: {}", res.error().message()))};
    QMessageBox box{QMessageBox::Critical, "EHT: database", text, QMessageBox::Ok};
    box.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    box.exec();
    return EXIT_FAILURE;
    }

  // being killed from outside, or logging out, is to close the window properly; otherwise this session's
  // settings are lost - only closeEvent writes them
  std::signal(SIGTERM, [](int) { asked_to_stop = 1; });
  std::signal(SIGINT, [](int) { asked_to_stop = 1; });

  QTimer stop_watch;
  QObject::connect(
    &stop_watch,
    &QTimer::timeout,
    &app,
    [&window]()
    {
      if(asked_to_stop)
        window.close();
    }
  );
  stop_watch.start(200);

  window.start_monitoring();
  window.show();
  // the slots guard the tool's logic themselves; this is the last net, so that an exception leaving the
  // event loop is at least written down rather than ending the process silently
  try
    {
    return app.exec();
    }
  catch(std::exception const & error)
    {
    spdlog::critical("the event loop stopped by an exception: {}", error.what());
    }
  catch(...)
    {
    spdlog::critical("the event loop stopped by an unknown exception");
    }
  return EXIT_FAILURE;
  }

void main_window_t::closeEvent(QCloseEvent * event)
  {
  save_settings();
  QMainWindow::closeEvent(event);
  }

