#include <ship_loadout.h>
#include <qscrollarea.h>

ship_loadout_window_t::ship_loadout_window_t(ship_loadout_t & state, QWidget * parent) :
    QMdiSubWindow(parent),
    ship_loadout_{&state}
  {
    setup_ui();
  }

auto ship_loadout_window_t::setup_ui() -> void
  {
  auto * container = new QWidget(this);
  auto * layout = new QVBoxLayout(container);

  // Nagłówek statku
  ship_info_label = new QLabel("Waiting for Loadout Data...", container);
  ship_info_label->setStyleSheet("font-size: 15px; font-weight: bold; color: #ffad33;");
  layout->addWidget(ship_info_label);

  // Sekcja parametrów głównych
  auto * grid = new QGridLayout();

  auto create_bar = [&](QString const & label, QString const & color)
  {
    auto * bar = new QProgressBar(container);
    bar->setFormat(label + ": %v / %m");
    bar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color));
    bar->setAlignment(Qt::AlignCenter);
    return bar;
  };

  hull_bar = create_bar("HULL", "#cc4444");
  fuel_bar = create_bar("FUEL", "#4444cc");
  cargo_bar = create_bar("CARGO", "#44cc44");

  grid->addWidget(new QLabel("Hull Health:"), 0, 0);
  grid->addWidget(hull_bar, 0, 1);
  grid->addWidget(new QLabel("Fuel Level:"), 1, 0);
  grid->addWidget(fuel_bar, 1, 1);
  grid->addWidget(new QLabel("Cargo Bay:"), 2, 0);
  grid->addWidget(cargo_bar, 2, 1);
  layout->addLayout(grid);

  // Obszar scrollowania dla modułów
  auto * scroll = new QScrollArea(container);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);

  auto * scroll_content = new QWidget();
  modules_layout = new QVBoxLayout(scroll_content);
  modules_layout->setAlignment(Qt::AlignTop);
  modules_layout->setContentsMargins(0, 5, 0, 0);

  scroll->setWidget(scroll_content);
  layout->addWidget(new QLabel("<b>System Modules:</b>"));
  layout->addWidget(scroll);

  setWidget(container);
  setWindowTitle("Ship Diagnostic Tool");
  resize(450, 550);
  }

auto ship_loadout_window_t::refresh_ui() -> void
  {
  auto const & loadout{*ship_loadout_};

  ship_info_label->setText(QString::fromStdString(loadout.ShipName + " (" + loadout.Ship + ") - " + loadout.ShipIdent));

  hull_bar->setRange(0, 100);
  hull_bar->setValue(static_cast<int>(loadout.HullHealth * 100));
  hull_bar->setFormat(QString("Hull: %1%").arg(static_cast<int>(loadout.HullHealth * 100)));

  fuel_bar->setMaximum(static_cast<int>(loadout.FuelCapacity.Main));
  fuel_bar->setValue(static_cast<int>(loadout.FuelLevel));

  cargo_bar->setMaximum(loadout.CargoCapacity);
  cargo_bar->setValue(loadout.CargoUsed);

  // 2. Synchronizacja modułów (Dynamic Rebuild if size changes)
  if(module_rows.size() != loadout.Modules.size())
    {
    // Czyścimy layout
    QLayoutItem * child;
    while((child = modules_layout->takeAt(0)) != nullptr)
      {
      if(child->widget())
        delete child->widget();
      delete child;
      }
    module_rows.clear();

    // Budujemy od nowa
    for(auto const & mod: loadout.Modules)
      {
      auto * row = new QWidget();
      auto * row_l = new QHBoxLayout(row);
      row_l->setContentsMargins(2, 2, 2, 2);

      auto * name = new QLabel(QString::fromStdString(mod.Slot));
      name->setFixedWidth(140);
      name->setToolTip(QString::fromStdString(mod.Item));

      auto * h_bar = new QProgressBar();
      h_bar->setFixedHeight(10);
      h_bar->setTextVisible(false);

      auto * p_lab = new QLabel();
      p_lab->setFixedWidth(25);

      auto * s_lab = new QLabel();
      s_lab->setFixedWidth(35);

      row_l->addWidget(name);
      row_l->addWidget(h_bar, 1);
      row_l->addWidget(p_lab);
      row_l->addWidget(s_lab);

      modules_layout->addWidget(row);
      module_rows.push_back({h_bar, p_lab, s_lab});
      }
    }

  // 3. Aktualizacja wartości wierszy (zawsze)
  for(size_t i = 0; i < loadout.Modules.size(); ++i)
    {
    auto const & mod = loadout.Modules[i];
    auto & ui = module_rows[i];

    ui.health->setValue(static_cast<int>(mod.Health * 100));
    ui.health->setStyleSheet(
      QString("QProgressBar::chunk { background-color: %1; }").arg(mod.Health > 0.4 ? "#2ecc71" : "#e74c3c")
    );

    ui.prio->setText(QString("P%1").arg(mod.Priority));
    ui.status->setText(mod.On ? "ONLINE" : "OFF");
    ui.status->setStyleSheet(mod.On ? "color: #00ff00;" : "color: #ff4444;");
    }
  }
