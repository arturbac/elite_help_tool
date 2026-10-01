#include <surface_window.h>
#include <biology.h>
#include <qformat.h>

#include <qboxlayout.h>
#include <qformlayout.h>
#include <qheaderview.h>
#include <qpushbutton.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <format>

namespace
  {
[[nodiscard]]
auto describe(nav::target_t const & target) -> std::string
  {
  return std::format(
    "{}{:.4f}, {:.4f}  on {}",
    target.label.empty() ? std::string{} : target.label + "  ",
    target.point.latitude,
    target.point.longitude,
    target.body
  );
  }
  }  // namespace

surface_window_t::surface_window_t(current_state_t const & state, QWidget * parent) :
    QMdiSubWindow(parent),
    state_{state}
  {
  setup_ui();
  }

auto surface_window_t::setup_ui() -> void
  {
  setWindowTitle("Surface");
  resize(720, 640);

  auto * central_widget = new QWidget(this);
  auto * layout = new QVBoxLayout(central_widget);

  here_ = new QLabel(central_widget);
  here_->setWordWrap(true);
  layout->addWidget(here_);

  auto * form = new QFormLayout;
  point_ = new QLineEdit(central_widget);
  point_->setPlaceholderText("latitude, longitude - e.g. -12.3456, 45.6789 or 12.3 S 45.6 E");
  form->addRow("Point", point_);
  body_ = new QLineEdit(central_widget);
  body_->setPlaceholderText("the body we are near");
  form->addRow("Body", body_);
  label_ = new QLineEdit(central_widget);
  label_->setPlaceholderText("optional, e.g. crashed ship");
  form->addRow("Name", label_);
  layout->addLayout(form);

  auto * buttons = new QHBoxLayout;
  auto * go = new QPushButton("Go there", central_widget);
  auto * clear = new QPushButton("Clear", central_widget);
  buttons->addWidget(go);
  buttons->addWidget(clear);
  buttons->addStretch();
  layout->addLayout(buttons);

  feedback_ = new QLabel(central_widget);
  feedback_->setWordWrap(true);
  layout->addWidget(feedback_);

  way_ = new QLabel(central_widget);
  way_->setWordWrap(true);
  layout->addWidget(way_);

  layout->addWidget(new QLabel("Recent (double click to go there again)", central_widget));
  recent_ = new QListWidget(central_widget);
  layout->addWidget(recent_, 1);

  codex_header_ = new QLabel(central_widget);
  codex_header_->setWordWrap(true);
  layout->addWidget(codex_header_);
  codex_ = new QTableWidget(central_widget);
  codex_->setColumnCount(4);
  codex_->setHorizontalHeaderLabels({"Codex entry", "Kind", "Distance", "Way"});
  codex_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  codex_->setSelectionBehavior(QAbstractItemView::SelectRows);
  codex_->verticalHeader()->setVisible(false);
  codex_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  layout->addWidget(codex_, 2);

  setWidget(central_widget);

  connect(go, &QPushButton::clicked, this, [this] { set_target(); });
  connect(point_, &QLineEdit::returnPressed, this, [this] { set_target(); });
  connect(label_, &QLineEdit::returnPressed, this, [this] { set_target(); });
  connect(
    clear,
    &QPushButton::clicked,
    this,
    [this]
    {
      target_.reset();
      feedback_->setText("No target.");
      refresh_ui();
    }
  );
  connect(
    recent_,
    &QListWidget::itemDoubleClicked,
    this,
    [this](QListWidgetItem * item)
    {
      size_t const ix{static_cast<size_t>(recent_->row(item))};
      if(ix < recent_targets_.size())
        choose(recent_targets_[ix]);
    }
  );

  connect(
    codex_,
    &QTableWidget::cellDoubleClicked,
    this,
    [this](int row, int)
    {
      if(row < 0 or static_cast<size_t>(row) >= codex_points_.size() or codex_body_.empty())
        return;
      nav::codex_point_t const & entry{codex_points_[static_cast<size_t>(row)]};
      choose(nav::target_t{.body = codex_body_, .point = entry.point, .label = entry.name});
    }
  );

  // what is typed is read at once, so a wrong spelling shows before it is sent anywhere
  connect(
    point_,
    &QLineEdit::textChanged,
    this,
    [this](QString const & text)
    {
      if(text.trimmed().isEmpty())
        feedback_->clear();
      else if(auto point{nav::parse_point(text.toStdString())}; point)
        feedback_->setText(qformat("Read as {:.4f}, {:.4f}", point->latitude, point->longitude));
      else
        feedback_->setText("Not a place yet - two numbers are needed, the latitude and the longitude.");
    }
  );

  refresh_ui();
  }

auto surface_window_t::set_target() -> void
  {
  auto point{nav::parse_point(point_->text().toStdString())};
  if(not point)
    {
    feedback_->setText("Not a place - two numbers are needed, the latitude and the longitude.");
    return;
    }
  std::string body{body_->text().trimmed().toStdString()};
  if(body.empty())
    body = near_body_;
  if(body.empty())
    {
    feedback_->setText("Which body? None is near - name it, or fly close to it first.");
    return;
    }
  choose(nav::target_t{.body = std::move(body), .point = *point, .label = label_->text().trimmed().toStdString()});
  }

auto surface_window_t::choose(nav::target_t target) -> void
  {
  std::erase_if(
    recent_targets_,
    [&](nav::target_t const & known)
    {
      return known.body == target.body and known.point.latitude == target.point.latitude
             and known.point.longitude == target.point.longitude;
    }
  );
  recent_targets_.insert(recent_targets_.begin(), target);
  constexpr size_t kept{20u};
  if(recent_targets_.size() > kept)
    recent_targets_.resize(kept);
  feedback_->setText(qformat("Going to {}", describe(target)));
  target_ = std::move(target);
  show_recent();
  refresh_ui();
  }

auto surface_window_t::show_recent() -> void
  {
  recent_->clear();
  for(nav::target_t const & target: recent_targets_)
    recent_->addItem(QString::fromStdString(describe(target)));
  }

auto surface_window_t::refresh_ui() -> void
  {
  auto const status{load_status(state_.journal_dir_path_)};
  bool const on_surface{
    status and not status->BodyName.empty() and status->Latitude and status->Longitude and status->PlanetRadius
    and *status->PlanetRadius > 0.0
  };

  if(status and not status->BodyName.empty())
    {
    // the body field follows the body we are near, until something else is typed into it
    QString const current{QString::fromStdString(status->BodyName)};
    if(body_->text().isEmpty() or body_->text() == QString::fromStdString(near_body_))
      body_->setText(current);
    near_body_ = status->BodyName;
    }

  follow_index();
  show_codex(
    on_surface ? std::optional{bio::surface_point_t{*status->Latitude, *status->Longitude}} : std::nullopt,
    on_surface ? *status->PlanetRadius : 0.0,
    on_surface ? status->Heading : std::nullopt
  );

  if(on_surface)
    here_->setText(qformat(
      "Near {} at {:.4f}, {:.4f}{}",
      status->BodyName,
      *status->Latitude,
      *status->Longitude,
      status->Heading ? std::format(", heading {:.0f}°", *status->Heading) : std::string{}
    ));
  else
    here_->setText("Not near a body's surface.");

  if(not target_)
    {
    way_->setText("No target - the overlay shows nothing.");
    return;
    }
  if(not on_surface or status->BodyName != target_->body)
    {
    way_->setText(qformat("Target on {} - the overlay points the way once you are near it.", target_->body));
    return;
    }
  nav::guidance_t const way{nav::guide(
    bio::surface_point_t{*status->Latitude, *status->Longitude}, status->Heading, target_->point, *status->PlanetRadius
  )};
  way_->setText(qformat(
    "{}, course {:.0f}°{}",
    nav::format_distance(way.distance_m),
    way.bearing_deg,
    way.turn_deg ? std::format(", {}", nav::format_turn(*way.turn_deg)) : std::string{}
  ));
  }

auto surface_window_t::follow_index() -> void
  {
  if(indexing_.valid())
    {
    if(indexing_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
      return;
    index_ = indexing_.get();
    index_ready_ = true;
    }
  // a new entry is written the moment it is scanned, and seeing it here a few seconds later is soon enough
  constexpr std::chrono::seconds every{5};
  auto const now{std::chrono::steady_clock::now()};
  if(not index_ or now - indexed_ < every)
    return;
  indexed_ = now;
  bool const first{not index_ready_};
  indexing_ = std::async(
    std::launch::async,
    [index = std::move(*index_), dir = std::filesystem::path{state_.journal_dir_path_}, first]() mutable
    {
      auto const started{std::chrono::steady_clock::now()};
      index.update(dir);
      if(first)
        spdlog::info(
          "surface: codex read from the journals in {} ms",
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count()
        );
      return std::move(index);
    }
  );
  index_.reset();
  }

auto surface_window_t::show_codex(std::optional<bio::surface_point_t> here, double radius_m, std::optional<double> heading)
  -> void
  {
  // made again for another body, or when an entry was logged here since - not at every read of the journals,
  // which would take away the row being pointed at
  std::vector<nav::codex_point_t> fresh;
  if(index_ and not near_body_.empty())
    fresh = index_->on_body(near_body_);
  if(index_ and (near_body_ != codex_body_ or fresh.size() != codex_points_.size()))
    {
    codex_body_ = near_body_;
    codex_points_ = std::move(fresh);
    // the nearest first, as it stands when the table is made - the order does not follow every step
    if(here and radius_m > 0.0)
      std::ranges::sort(
        codex_points_,
        {},
        [&](nav::codex_point_t const & entry) { return bio::surface_distance_m(*here, entry.point, radius_m); }
      );
    codex_->setRowCount(static_cast<int>(codex_points_.size()));
    for(size_t row{}; row != codex_points_.size(); ++row)
      {
      nav::codex_point_t const & entry{codex_points_[row]};
      auto * name{new QTableWidgetItem(QString::fromStdString(entry.first ? entry.name + "  (first)" : entry.name))};
      name->setToolTip(qformat("{}  {:.4f}, {:.4f}", entry.seen, entry.point.latitude, entry.point.longitude));
      codex_->setItem(static_cast<int>(row), 0, name);
      codex_->setItem(static_cast<int>(row), 1, new QTableWidgetItem(QString::fromStdString(entry.category)));
      codex_->setItem(static_cast<int>(row), 2, new QTableWidgetItem);
      codex_->setItem(static_cast<int>(row), 3, new QTableWidgetItem);
      }
    codex_->resizeColumnToContents(1);
    }

  if(not index_ready_ and codex_points_.empty())
    codex_header_->setText("Codex: reading the journals...");
  else if(codex_body_.empty())
    codex_header_->setText(index_ready_ ? "Codex: not near a body." : "Codex: reading the journals...");
  else
    codex_header_->setText(qformat(
      "Codex on {}: {} (double click to go there)",
      codex_body_,
      codex_points_.empty() ? std::string{"nothing logged here"} : std::format("{} places", codex_points_.size())
    ));

  for(size_t row{}; row != codex_points_.size(); ++row)
    {
    if(not here or radius_m <= 0.0)
      {
      codex_->item(static_cast<int>(row), 2)->setText({});
      codex_->item(static_cast<int>(row), 3)->setText({});
      continue;
      }
    nav::guidance_t const way{nav::guide(*here, heading, codex_points_[row].point, radius_m)};
    codex_->item(static_cast<int>(row), 2)->setText(QString::fromStdString(nav::format_distance(way.distance_m)));
    codex_->item(static_cast<int>(row), 3)->setText(qformat(
      "{:.0f}°{}", way.bearing_deg, way.turn_deg ? std::format("  {}", nav::format_turn(*way.turn_deg)) : std::string{}
    ));
    }
  }
