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
#include <string_view>
#include <vector>
#include <qprogressbar.h>
#include <qscrollarea.h>
#include <qgroupbox.h>

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
  overlay_timer_->setInterval(2000);
  connect(overlay_timer_, &QTimer::timeout, this, [this] { publish_overlay(); });
  overlay_timer_->start();
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

  overlay_feed_->publish(state_, plotted);
  }

auto main_window_t::start_monitoring() -> void
  {
  // the thread touches db_, so it starts only once that is open
  worker_thread_ = std::jthread([this](std::stop_token stoken) { background_worker(stoken); });
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
    {window_type_e::journal_log, "Log"}
  };

  for(tool_button_t const & tool: tools)
    {
    auto * btn = new QPushButton(tool.label, this);
    toolbox_dock->addWidget(btn);

    connect(btn, &QPushButton::clicked, this, [this, type = tool.type]() { activate_window(type); });
    }
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
  // the first filling of the faction list - db_ is touched from this thread alone
  state_.load_factions();
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
    std::bind_front(&generic_state_t::discovery, &state_),
    stoken,
    [this](fs::path const & path)
    {
      spdlog::info("monitoring journal {}", path.string());
      // a new journal is a new session of the game: whatever was locked on is not locked on now
      state_.forget_live_combat();
      QMetaObject::invokeMethod(
        this, [this, path]() { file_to_monitor = path; }, Qt::QueuedConnection
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
  apply_dark_theme();

  // a look at the queries without a rebuild - SPDLOG_LEVEL=debug
  spdlog::cfg::load_env_levels();

  main_window_t window{"ehtdb.sqlite", "journal-dir"};
  if(not window.state_.db_.open())
    return EXIT_FAILURE;

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
  return app.exec();
  }

void main_window_t::closeEvent(QCloseEvent * event)
  {
  save_settings();
  QMainWindow::closeEvent(event);
  }

