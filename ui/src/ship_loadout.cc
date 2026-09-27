#include <eht_settings.h>
#include <ship_loadout.h>
#include <qscrollarea.h>

ship_loadout_window_t::ship_loadout_window_t(ship_loadout_t const & state, QWidget * parent) :
    QMdiSubWindow(parent),
    ship_loadout_{state}
  {
    setup_ui();
  }

auto ship_loadout_window_t::setup_ui() -> void
  {
  auto * container = new QWidget(this);
  auto * layout = new QVBoxLayout(container);

  // the ship's header
  ship_info_label = new QLabel("Waiting for Loadout Data...", container);
  ship_info_label->setStyleSheet(
    QString("font-size: 15px; font-weight: bold; color: %1;")
      .arg(QString::fromStdString(eht::settings()->gui.ship_header.write()))
  );
  layout->addWidget(ship_info_label);

  // the section of main parameters
  auto * grid = new QGridLayout();

  auto create_bar = [&](QString const & label, QString const & color)
  {
    auto * bar = new QProgressBar(container);
    bar->setFormat(label + ": %v / %m");
    bar->setStyleSheet(QString("QProgressBar::chunk { background-color: %1; }").arg(color));
    bar->setAlignment(Qt::AlignCenter);
    return bar;
  };

  auto const cfg{eht::settings()};
  auto const hex = [](eht::colour_t const & c) { return QString::fromStdString(c.write()); };
  hull_bar = create_bar("HULL", hex(cfg->gui.hull_bar));
  fuel_bar = create_bar("FUEL", hex(cfg->gui.fuel_bar));
  cargo_bar = create_bar("CARGO", hex(cfg->gui.cargo_bar));

  grid->addWidget(new QLabel("Hull Health:"), 0, 0);
  grid->addWidget(hull_bar, 0, 1);
  grid->addWidget(new QLabel("Fuel Level:"), 1, 0);
  grid->addWidget(fuel_bar, 1, 1);
  grid->addWidget(new QLabel("Cargo Bay:"), 2, 0);
  grid->addWidget(cargo_bar, 2, 1);
  layout->addLayout(grid);

  // the scrolling area for the modules
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

auto ship_loadout_window_t::refresh_ui(ship_loadout_t const & load) -> void
  {
  ship_loadout_ = load;
  auto const & loadout{ship_loadout_};

  ship_info_label->setText(QString::fromStdString(loadout.ShipName + " (" + loadout.Ship + ") - " + loadout.ShipIdent));

  hull_bar->setRange(0, 100);
  hull_bar->setValue(static_cast<int>(loadout.HullHealth * 100));
  hull_bar->setFormat(QString("Hull: %1%").arg(static_cast<int>(loadout.HullHealth * 100)));

  fuel_bar->setMaximum(static_cast<int>(loadout.FuelCapacity.Main));
  fuel_bar->setValue(static_cast<int>(loadout.FuelLevel));

  cargo_bar->setMaximum(loadout.CargoCapacity);
  cargo_bar->setValue(loadout.CargoUsed);

  // 2. synchronise the modules (rebuilt dynamically when the size changes)
  if(module_rows.size() != loadout.Modules.size())
    {
    // clear the layout
    QLayoutItem * child;
    while((child = modules_layout->takeAt(0)) != nullptr)
      {
      if(child->widget())
        delete child->widget();
      delete child;
      }
    module_rows.clear();

    // build it anew
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

  // 3. update the values of the rows (always)
  for(size_t i = 0; i < loadout.Modules.size(); ++i)
    {
    auto const & mod = loadout.Modules[i];
    auto & ui = module_rows[i];

    ui.health->setValue(static_cast<int>(mod.Health * 100));
    auto const cfg{eht::settings()};
    ui.health->setStyleSheet(
      QString("QProgressBar::chunk { background-color: %1; }")
        .arg(
          QString::fromStdString(
            mod.Health > cfg->gui.module_damaged_below ? cfg->gui.module_healthy.write() : cfg->gui.module_damaged.write()
          )
        )
    );

    ui.prio->setText(QString("P%1").arg(mod.Priority));
    ui.status->setText(mod.On ? "ONLINE" : "OFF");
    ui.status->setStyleSheet(
      QString("color: %1;").arg(QString::fromStdString(mod.On ? cfg->gui.module_on.write() : cfg->gui.module_off.write()))
    );
    }
  }
