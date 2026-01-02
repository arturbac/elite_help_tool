#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QString>
#include <QListWidget>
#include <QListWidgetItem>
#include <memory>
#include <elite_events.h>


// Bazowa klasa dla spójności (opcjonalnie)
struct base_log_widget_t : public QWidget
  {
  using QWidget::QWidget;
  bool ignored{};
  };

template<typename T>
class elite_event_widget_t : public base_log_widget_t
  {
public:
  explicit elite_event_widget_t(T const & obj, QWidget * parent = nullptr);
  };

using log_payload_t = events::event_holder_t;

class journal_log_window_t : public QWidget
  {
  Q_OBJECT
public:
  [[nodiscard]]
  explicit journal_log_window_t(QWidget * parent = nullptr);

  // Metoda dodająca log wykorzystująca C++23 std::variant jako payload
  auto add_log(log_payload_t const & payload) -> void;
  auto add_logs_batch(std::vector<events::event_holder_t>&& batch) -> void;
  
private:
  QListWidget * m_list_widget;
  };
