#include <main_window.h>
#include <spdlog/spdlog.h>

#include <qapplication.h>
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
///\brief okna narzedziowe zyja przez caly czas dzialania aplikacji
///\detail kazde istnieje w jednej instancji i ma swoj przycisk na toolbarze, zamkniecie zostawiloby
/// przycisk bez okna, wiec zdarzenie zamkniecia jest polykane
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
  }

auto main_window_t::start_monitoring() -> void
  {
  // watek dotyka db_, wiec startuje dopiero po jej otwarciu
  worker_thread_ = std::jthread([this](std::stop_token stoken) { background_worker(stoken); });
  }

auto main_window_t::setup_ui() -> void
  {
  resize(1200, 800);
  setWindowTitle("Elite Dangerous Help Tool");

  // Centralny obszar dla okien podrzędnych
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

  route_view_ = new route_window_t{state_};
  add_tool_window(route_view_, window_type_e::route);

  faction_view_ = new faction_window_t{state_};
  add_tool_window(faction_view_, window_type_e::faction);

  faction_state_view_ = new faction_state_window_t{state_, db_path_};
  add_tool_window(faction_state_view_, window_type_e::faction_state);
  }

auto main_window_t::add_tool_window(QMdiSubWindow * sub, window_type_e type) -> void
  {
  mdi_area_->addSubWindow(sub);
  sub->setProperty("window_type", QVariant::fromValue(type));

  // bez przycisku zamykania - okno jest jedno i ma zyc do konca sesji
  // WindowSystemMenuHint dokladalby menu okna z pozycja zamknij, wiec go tu nie ma
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

  // zwiniete okno trzeba wpierw rozwinac, samo raise() by go nie pokazalo
  if(sub->isMinimized())
    sub->showNormal();
  else
    sub->show();

  sub->raise();
  mdi_area_->setActiveSubWindow(sub);
  }

auto main_window_t::setup_toolbox() -> void
  {
  auto * toolbox_dock = new QToolBar("Toolbox", this);
  toolbox_dock->setMovable(false);
  addToolBar(Qt::LeftToolBarArea, toolbox_dock);

  // po jednym przycisku na okno, klikniecie wyciaga je na wierzch
  struct tool_button_t
    {
    window_type_e type;
    QString label;
    };

  std::vector<tool_button_t> const tools{
    {window_type_e::system, "System"},
    {window_type_e::faction_state, "Factions"},
    {window_type_e::faction, "Reputation"},
    {window_type_e::mission, "Missions"},
    {window_type_e::route, "Route"},
    {window_type_e::ship, "Ship"},
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
    settings.setValue("pos", sub->pos());
    settings.setValue("size", sub->size());
    }
  settings.endArray();
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
    auto const pos_var = settings.value("pos");
    auto const size_var = settings.value("size");

    // zapisy sprzed przebudowy toolbaru moga zawierac okna zastepcze, ktorych juz nie ma
    QMdiSubWindow * sub{subwindow_for(type)};
    if(sub) [[likely]]
      {
      if(sub->mdiArea() == nullptr)
        mdi_area_->addSubWindow(sub);

      QPoint pos = pos_var.isValid() ? pos_var.toPoint() : QPoint(10 * i, 10 * i);
      QSize size = size_var.isValid() ? size_var.toSize() : QSize(400, 300);

      if(size.width() <= 0 || size.height() <= 0)
        size = QSize(400, 300);

      sub->move(pos);
      sub->resize(size);
      sub->show();
      }
    }
  settings.endArray();
  }

auto main_window_t::background_worker(std::stop_token stoken) -> void
  {
  // pierwsze wypełnienie listy frakcji - db_ dotykane wyłącznie z tego wątku
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

  // restart gry tworzy nowy journal, sledzenie przelacza sie na niego samo
  tail_journal_dir(
    state_.journal_dir_path_,
    std::bind_front(&generic_state_t::discovery, &state_),
    stoken,
    [this](fs::path const & path)
    {
      spdlog::info("monitoring journal {}", path.string());
      QMetaObject::invokeMethod(
        this, [this, path]() { file_to_monitor = path; }, Qt::QueuedConnection
      );
    }
  );
  }

auto main(int argc, char * argv[]) -> int
  {
  QApplication app(argc, argv);

  main_window_t window{"ehtdb.sqlite", "journal-dir"};
  if(not window.state_.db_.open())
    return EXIT_FAILURE;

  window.start_monitoring();
  window.show();
  return app.exec();
  }

void main_window_t::closeEvent(QCloseEvent * event)
  {
  save_settings();
  QMainWindow::closeEvent(event);
  }

