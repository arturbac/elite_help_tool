#include "journal_log.h"
#include <simple_enum/std_format.hpp>
#include "qformat.h"

// ⁕🌐🌕🌍🔵🔴🏷🏱📡📢
static constexpr auto value_color(planet_value_e value)
  {
  std::string_view color{""};
  if(value == planet_value_e::high)
    color = "color: blue; font-weight: bold;";
  else if(value == planet_value_e::medium)
    color = "color: yellow; font-weight: normal;";
  return color;
  }

template<typename T>
elite_event_widget_t<T>::elite_event_widget_t(T const & event, QWidget * parent) : base_log_widget_t(parent)
  {
  auto * layout = new QHBoxLayout(this);
  if constexpr(std::same_as<T, events::fsd_target_t>)
    {
    planet_value_e const vl{exploration::system_approx_value(event.StarClass, event.Name)};

    layout->addWidget(new QLabel("⇨🌞"));
    auto * val = new QLabel(qformat("next target [{}] {}", event.StarClass, event.Name));
    if(planet_value_e::low < vl)
      val->setStyleSheet(value_color(vl).data());
    layout->addWidget(val);
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::fsd_jump_t>)
    {
    layout->addWidget(new QLabel("🌞←"));
    layout->addWidget(new QLabel(qformat("{}", event.StarSystem)));
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::start_jump_t>)
    {
    planet_value_e const vl{exploration::system_approx_value(event.StarClass, event.StarSystem)};

    layout->addWidget(new QLabel("→🌞"));
    auto * val = new QLabel(qformat("[{}] {}", event.StarClass, event.StarSystem));
    if(planet_value_e::low < vl)
      val->setStyleSheet(value_color(vl).data());
    layout->addWidget(val);
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::fss_discovery_scan_t>)
    {
    layout->addWidget(new QLabel("🔍"));
    auto * val = new QLabel(
      qformat("system {} body:{} nonbody:{}", event.SystemName, event.BodyCount, event.NonBodyCount)

    );
    layout->addWidget(val);
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::fss_body_signals_t>)
    {
    layout->addWidget(new QLabel("📢"));
    std::string signfmt;
    for(events::signal_t const & signal: event.Signals)
      layout->addWidget(new QLabel(qformat("signals: {}:{}", signal.Type_Localised, signal.Count)));
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::fss_all_bodies_found_t>)
    {
    layout->addWidget(new QLabel("🔍"));
    layout->addWidget(new QLabel("all bodies found"));
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::scan_bary_centre_t>)
    {
    layout->addWidget(new QLabel("🌐"));
    layout->addWidget(new QLabel("scan bary centre"));
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::scan_detailed_scan_t>)
    {
    layout->addWidget(new QLabel(qformat("🌐 detailed scan", event.StarSystem)));
    layout->addWidget(
      new QLabel(QString::fromStdString(std::string{body_short_name(event.StarSystem, event.BodyName)}))
    );
    layout->addStretch();
    }
  else if constexpr(std::same_as<T, events::saa_scan_complete_t>)
    {
    layout->addWidget(new QLabel(qformat("🌕 scan complete", event.BodyName)));

    layout->addStretch();
    }
  // else
  //   {
  //   // ignored = true;
  //   }
  }

journal_log_window_t::journal_log_window_t(QWidget * parent) : QWidget(parent)
  {
  auto * layout = new QVBoxLayout(this);
  m_list_widget = new QListWidget(this);

  // Optymalizacja renderowania
  m_list_widget->setUniformItemSizes(false);
  m_list_widget->setSelectionMode(QAbstractItemView::NoSelection);

  layout->addWidget(m_list_widget);
  }

// Metoda dodająca log wykorzystująca C++23 std::variant jako payload
auto journal_log_window_t::add_log(log_payload_t const & payload) -> void
  {
  auto * item = new QListWidgetItem(m_list_widget);
  base_log_widget_t * display_widget = nullptr;

  // Wizytator tworzący odpowiedni widget na podstawie typu danych
  std::visit(
    [&](auto && arg)
    {
      using T = std::decay_t<decltype(arg)>;
      display_widget = new elite_event_widget_t<T>(arg);
    },
    payload
  );

  if(display_widget and not display_widget->ignored)
    {
    m_list_widget->addItem(item);
    display_widget->adjustSize();
    item->setSizeHint(display_widget->sizeHint());

    m_list_widget->setItemWidget(item, display_widget);
    }
  }

auto journal_log_window_t::add_logs_batch(std::vector<events::event_holder_t> && batch) -> void
  {
  setUpdatesEnabled(false);

  for(auto const & ev: batch)
    add_log(ev);

  setUpdatesEnabled(true);
  m_list_widget->scrollToBottom();
  }

// #include "journal_log.moc"
