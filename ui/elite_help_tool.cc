#include "logic.h"
#include <QApplication>
#include <QMainWindow>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QSettings>
#include "journal_log.h"
#include <QToolBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <string_view>
#include <vector>
#include <simple_enum/simple_enum.hpp>
#include <thread>
#include <stop_token>
#include <file_io.h>
#include "system_bodies_model.h"
#include <qpointer.h>

// Struktura reprezentująca definicję przycisku narzędziowego
struct tool_definition_t
  {
  QString label;
  QString window_title;
  };
enum struct window_type_e
  {
  none,
  system,
  journal_log,

  };

consteval auto adl_enum_bounds(window_type_e)
  {
  using enum window_type_e;
  return simple_enum::adl_info{none, journal_log};
  }
Q_DECLARE_METATYPE(window_type_e)

class main_window_t : public QMainWindow
  {
  Q_OBJECT

public:
  journal_log_window_t * jlw_{};
  journal_state_t state_;
  std::jthread worker_thread_;
  QPointer<system_window_t> system_view_;
  fs::path file_to_monitor{};
  
  [[nodiscard]]
  explicit main_window_t(QWidget * parent = nullptr);

  auto closeEvent(QCloseEvent * event) -> void override
    {
    save_settings();
    QMainWindow::closeEvent(event);
    }

  auto on_journal_event_received() -> void;

private:
  auto background_worker(std::stop_token stoken) -> void;

  auto setup_ui() -> void;

  auto setup_toolbox() -> void;

  auto create_journal_tool_window(QString const & title) -> QMdiSubWindow *;

  auto create_tool_window(QString const & title) -> QMdiSubWindow *;

  QMdiArea * mdi_area_{nullptr};

  auto save_settings() -> void;

  auto load_settings() -> void;
  };

void journal_state_t::handle(events::event_holder_t && payload)
  {
  if(nullptr != parent->jlw_)
    {
    bool update_system{};
    std::visit(
      [&](auto && event)
      {
        using T = std::decay_t<decltype(event)>;
        if constexpr(std::same_as<T, events::fsd_jump_t>)
          {
          jump_info = event;
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fsd_target_t>)
          {
          // planet_value_e const vl{exploration::system_approx_value(event.StarClass, event.Name)};
          // debug("next target {}[{}] {}\033[m", value_color(vl), event.StarClass, event.Name);
          next_target = event;
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::start_jump_t>)
          {
          // planet_value_e const vl{exploration::system_approx_value(event.StarClass, event.StarSystem)};
          // info("[{}] jump to {}[{}] {}\033[m\n", arg.timestamp, value_color(vl), arg.StarClass, arg.StarSystem);
          system = star_system_t{
            .system_address = event.SystemAddress,
            .name = event.StarSystem,
            .star_type = event.StarClass,
            .luminosity = {},
            .scan_bary_centre = {},
            .bodies = {},
            .stellar_mass = {},
            .sub_class = {}
          };
          fss_complete = false;
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fss_body_signals_t>)
          {
          // info(" {}", event.BodyName);
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.signals_ = std::move(event.Signals);
            }
          update_system = true;
          }
        else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>) 
        {
          fss_complete = true;
        }
        else if constexpr(std::same_as<T, events::scan_bary_centre_t>)
          {
          system.scan_bary_centre.emplace_back(std::move(event));
          }
        else if constexpr(std::same_as<T, events::scan_detailed_scan_t>)
          {
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end())
            *it = to_body(std::move(event));
          else
            system.bodies.emplace_back(to_body(std::move(event)));

          update_system = true;
          }
        else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
          {
          // info("saa scan complete for {}", event.BodyName);
          auto it{system.body_by_id(event.BodyID)};
          if(it != system.bodies.end())
            {
            planet_details_t & details{std::get<planet_details_t>(it->details)};
            details.mapped = true;
            }
          update_system = true;
          }
      },
      payload
    );
      {
      std::lock_guard lock(buffer_mtx_);
      event_buffer_.push_back(std::move(payload));
      }
    QMetaObject::invokeMethod(
      parent->jlw_,
      [this]()
      {
        std::vector<events::event_holder_t> batch;
          {
          std::lock_guard lock(buffer_mtx_);
          batch = std::move(event_buffer_);
          event_buffer_.clear();
          }

        if(not batch.empty() and parent->jlw_)
          parent->jlw_->add_logs_batch(std::move(batch));
      },
      Qt::QueuedConnection
    );

    if(update_system)
      QMetaObject::invokeMethod(
        parent, [target = parent]() mutable { target->on_journal_event_received(); }, Qt::QueuedConnection
      );
    }
  }

auto main_window_t::on_journal_event_received() -> void
  {
  if(system_view_) [[likely]]
    system_view_->refresh_ui();
  }

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

    {
    auto * btn = new QPushButton("journal", this);
    toolbox_dock->addWidget(btn);

    connect(btn, &QPushButton::clicked, this, [this, title = "Journal log"]() { create_journal_tool_window(title); });
    }
  }

auto main_window_t::create_journal_tool_window(QString const & title) -> QMdiSubWindow *
  {
  auto * widget = new QWidget();
  auto * layout = new QVBoxLayout(widget);
  jlw_ = new journal_log_window_t{};

  layout->addWidget(jlw_);

  auto * sub_window = mdi_area_->addSubWindow(widget);
  sub_window->setWindowTitle(title);
  sub_window->setAttribute(Qt::WA_DeleteOnClose);
  sub_window->show();
  sub_window->setProperty("window_type", QVariant::fromValue(window_type_e::journal_log));

  return sub_window;
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
    // settings.setValue("geometry", windows[i]->saveGeometry());
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
    // auto geometry = settings.value("geometry").toByteArray();

    // Tworzymy okno i przywracamy jego geometrię
    QMdiSubWindow * sub{};
    switch(type)
      {
      case window_type_e::none:        sub = create_tool_window(title); break;
      case window_type_e::system:      sub = system_view_; break;
      case window_type_e::journal_log: sub = create_journal_tool_window(title); break;
      }
    if(sub) [[likely]]
      {
      if(sub->mdiArea() == nullptr)
        mdi_area_->addSubWindow(sub);

      // LOGIKA NAPRAWCZA:
      // Jeśli brak zapisanych danych, ustawiamy sensowne minimum
      QPoint pos = pos_var.isValid() ? pos_var.toPoint() : QPoint(10 * i, 10 * i);
      QSize size = size_var.isValid() ? size_var.toSize() : QSize(400, 300);

      // Zabezpieczenie przed zerowym rozmiarem
      if(size.width() <= 0 || size.height() <= 0)
        size = QSize(400, 300);

      sub->move(pos);
      sub->resize(size);
      sub->show();
      }
    // if(sub)
    //   sub->restoreGeometry(geometry);
    }
  settings.endArray();
  }
auto main_window_t::background_worker(std::stop_token stoken) -> void
  {
    file_to_monitor = *find_latest_journal("journal-dir");
  tail_file(
     // "journal-dir/Journal.2025-12-25T122142.01.log"
    file_to_monitor
    , std::bind_front(&generic_state_t::discovery, &state_), stoken
  );
  }

auto main(int argc, char * argv[]) -> int
  {
  QApplication app(argc, argv);
  
  
  main_window_t window;
  window.show();
  return app.exec();
  }

#include "elite_help_tool.moc"
