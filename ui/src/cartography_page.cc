#include <cartography_page.h>
#include <data/progress.h>
#include <event_guard.h>
#include <format_credits.h>

#include <algorithm>
#include <format>
#include <ranges>

#include <qboxlayout.h>
#include <qbrush.h>
#include <qheaderview.h>
#include <qlabel.h>
#include <qtablewidget.h>
#include <qtimer.h>

#include <spdlog/spdlog.h>

namespace
  {
constexpr char const * explanation{
  "Each sale of cartographic data from the journals, against what EHT reckoned the bodies scanned there were worth"
  " at the moment of the sale - the same reckoning as the cartography a death would cost. The game writes one sum"
  " for all the systems of a sale and nothing for a body, so only a sale of one system gives an exact price:"
  " sell system by system to learn more. The base is what the bodies are worth and is set against the estimate;"
  " the bonus is paid for a system scanned or mapped in full, and a fleet carrier keeps a quarter of the base."
};
  }  // namespace

cartography_page_t::cartography_page_t(std::string db_path, QWidget * parent) : QWidget{parent}, db_{std::move(db_path)}
  {
  auto * layout = new QVBoxLayout(this);
  note_ = new QLabel(this);
  note_->setWordWrap(true);
  layout->addWidget(note_);
  view_ = new QTableWidget(this);
  view_->setColumnCount(9);
  view_->setHorizontalHeaderLabels(
    {"Sold (UTC)", "Systems", "Bodies sold", "Bodies priced", "Estimate", "Base", "Bonus", "Paid", "Base / estimate"}
  );
  view_->verticalHeader()->setVisible(false);
  view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  view_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
  view_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  layout->addWidget(view_, 1);

  poll_ = new QTimer(this);
  poll_->setInterval(std::chrono::milliseconds{200});
  connect(
    poll_,
    &QTimer::timeout,
    this,
    [this]
    {
      eht::event_guard(
        "cartography reading",
        [this]
        {
          if(not reading_ or reading_->wait_for(std::chrono::seconds{0}) != std::future_status::ready)
            return;
          poll_->stop();
          auto future{std::move(*reading_)};
          reading_.reset();
          show_sales(future.get());
        }
      );
    }
  );
  }

auto cartography_page_t::showEvent(QShowEvent * event) -> void
  {
  QWidget::showEvent(event);
  eht::event_guard("cartography refresh", [this] { refresh(false); });
  }

auto cartography_page_t::refresh(bool now) -> void
  {
  // read only while in view, and then not at every change of the game's state
  if(not isVisible() and not now)
    return;
  if(reading_)
    return;
  auto const clock{std::chrono::steady_clock::now()};
  if(not now and read_ != std::chrono::steady_clock::time_point{} and clock - read_ < std::chrono::minutes{1})
    return;
  read_ = clock;

  std::string fid;
  if(auto owner{db_.load_owner()}; owner and *owner)
    fid = (*owner)->fid;
  if(view_->rowCount() == 0)
    note_->setText("Reading the journals - with a long history this takes up to a minute...");
  reading_ = std::async(
    std::launch::async,
    [fid = std::move(fid)]() -> std::vector<bio::cartography_sale_t>
    {
      // a failure here is no reason to lose the tool: the page stays empty and says so in the log
      try
        {
        return bio::cartography_sales("journal-dir", fid);
        }
      catch(std::exception const & e)
        {
        spdlog::error("cartography: the journals could not be read: {}", e.what());
        }
      catch(...)
        {
        spdlog::error("cartography: the journals could not be read");
        }
      return {};
    }
  );
  poll_->start();
  }

auto cartography_page_t::show_sales(std::vector<bio::cartography_sale_t> sales) -> void
  {
  std::ranges::reverse(sales);

  std::string note{explanation};
  if(auto const accuracy{bio::estimate_accuracy(sales)}; accuracy)
    note += std::format(
      "\n{} sales of one system: a base of {:.2f} times the estimate (median), from {:.2f} to {:.2f}.",
      accuracy->sales,
      accuracy->median,
      accuracy->lowest,
      accuracy->highest
    );
  note_->setText(QString::fromStdString(note));

  auto const cell = [](std::string const & text, bool number) -> QTableWidgetItem *
  {
    auto * item{new QTableWidgetItem(QString::fromStdString(text))};
    if(number)
      item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
  };
  view_->setRowCount(int(sales.size()));
  for(auto const & [row, sale]: sales | std::views::enumerate)
    {
    int const r{int(row)};
    bool const single{sale.systems.size() == 1u};
    std::string systems{single ? sale.systems.front() : std::format("{} systems", sale.systems.size())};
    view_->setItem(r, 0, cell(std::format("{:%d.%m.%Y %H:%M}", sale.when), false));
    auto * names{cell(systems, false)};
    if(not single)
      {
      std::string all;
      for(std::string const & name: sale.systems)
        all += (all.empty() ? "" : "\n") + name;
      names->setToolTip(QString::fromStdString(all));
      }
    view_->setItem(r, 1, names);
    view_->setItem(r, 2, cell(sale.bodies != 0u ? std::format("{}", sale.bodies) : std::string{}, true));
    view_->setItem(r, 3, cell(std::format("{}", sale.priced), true));
    view_->setItem(r, 4, cell(format_credits_value(sale.estimate), true));
    view_->setItem(r, 5, cell(format_credits_value(sale.base_value), true));
    view_->setItem(r, 6, cell(format_credits_value(sale.bonus), true));
    view_->setItem(r, 7, cell(format_credits_value(sale.total), true));
    // a sale of several systems is a sum over all of them, so its ratio is shown but says less
    auto * ratio{cell(
      sale.estimate != 0u ? std::format("{:.2f}", double(sale.base_value) / double(sale.estimate)) : std::string{"-"},
      true
    )};
    if(not single)
      ratio->setForeground(QBrush{QColor{0x8a, 0x8a, 0x8a}});
    view_->setItem(r, 8, ratio);
    }
  }
