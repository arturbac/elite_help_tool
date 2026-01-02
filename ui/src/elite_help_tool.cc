#include <main_window.h>


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

// Struktura reprezentująca definicję przycisku narzędziowego
struct tool_definition_t
  {
  QString label;
  QString window_title;
  };

Q_DECLARE_METATYPE(window_type_e)

main_window_t::main_window_t(QWidget * parent) : QMainWindow(parent), state_{this}
  {
  setup_ui();
  load_settings();

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

  setup_toolbox();

  system_view_ = new system_window_t(state_);
  mdi_area_->addSubWindow(system_view_);
  system_view_->setProperty("window_type", QVariant::fromValue(window_type_e::system));
  system_view_->show();

  ship_view_ = new ship_loadout_window_t(state_.ship_loadout);
  mdi_area_->addSubWindow(ship_view_);
  ship_view_->setProperty("window_type", QVariant::fromValue(window_type_e::ship));
  ship_view_->show();
  
  jlw_ = new journal_log_window_t{};
  mdi_area_->addSubWindow(jlw_);
  jlw_->setProperty("window_type", QVariant::fromValue(window_type_e::journal_log));
  jlw_->show();
  }

auto main_window_t::setup_toolbox() -> void
  {
  auto * toolbox_dock = new QToolBar("Toolbox", this);
  toolbox_dock->setMovable(false);
  addToolBar(Qt::LeftToolBarArea, toolbox_dock);

  std::vector<tool_definition_t> const tools = {
    {"Exploration", "Exploration View"},
    {"Log", "System Logs"},
    {"Systems", "Systems Overview"},
    {"PVE PM", "PVE Performance Monitor"}
  };

  // Tworzenie przycisków na toolbarze
  for(auto const & tool: tools)
    {
    auto * btn = new QPushButton(tool.label, this);
    toolbox_dock->addWidget(btn);

    connect(btn, &QPushButton::clicked, this, [this, title = tool.window_title]() { create_tool_window(title); });
    }

    // {
    // auto * btn = new QPushButton("journal", this);
    // toolbox_dock->addWidget(btn);
    // 
    // // connect(btn, &QPushButton::clicked, this, [this, title = "Journal log"]() { create_journal_tool_window(title); });
    // }
  }

auto main_window_t::create_tool_window(QString const & title) -> QMdiSubWindow *
  {
  auto * widget = new QWidget();
  auto * layout = new QVBoxLayout(widget);
  layout->addWidget(new QLabel("Content of " + title, widget));

  auto * sub_window = mdi_area_->addSubWindow(widget);
  sub_window->setWindowTitle(title);
  sub_window->setAttribute(Qt::WA_DeleteOnClose);
  sub_window->show();
  sub_window->setProperty("window_type", QVariant::fromValue(window_type_e::none));
  return sub_window;
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
    auto title = settings.value("title").toString();
    auto const pos_var = settings.value("pos");
    auto const size_var = settings.value("size");

    QMdiSubWindow * sub{};
    switch(type)
      {
      case window_type_e::none:        sub = create_tool_window(title); break;
      case window_type_e::system:      sub = system_view_; break;
      case window_type_e::ship:        sub = ship_view_; break;
      case window_type_e::journal_log: sub = jlw_; break;
      }
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
  file_to_monitor = *find_latest_journal("journal-dir");
  tail_file(
    // "journal-dir/Journal.2025-12-25T122142.01.log"
    // "journal-dir/debug_highmetal.json"
    file_to_monitor,
    std::bind_front(&generic_state_t::discovery, &state_),
    stoken
  );
  }

auto main(int argc, char * argv[]) -> int
  {
  QApplication app(argc, argv);

  main_window_t window;
  window.show();
  return app.exec();
  }

