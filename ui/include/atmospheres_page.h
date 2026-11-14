#pragma once
#include <atmosphere_odds.h>

#include <chrono>
#include <string>

#include <qwidget.h>

class QComboBox;
class QLabel;
class QTableWidget;
class QTimer;

///\brief the chance of a jump to find a landable planet of an atmosphere, by the class of the arrival star - the
/// galaxy map's star filter to set for the rare atmospheres; with the parent star and the parent body of such planets.
/// Over this account's galaxy and the shared ones; shown in the Exploration window
///\detail while shown, read again when this galaxy knows more bodies, or the shared ones are five minutes old
class atmospheres_page_t final : public QWidget
  {
  Q_OBJECT

public:
  explicit atmospheres_page_t(std::string db_path, QWidget * parent = nullptr);

protected:
  auto showEvent(QShowEvent * event) -> void override;

private:
  auto refresh(bool now) -> void;
  auto show_atmospheres() -> void;
  auto show_odds() -> void;

  std::string db_path_;
  QLabel * summary_{};
  QComboBox * atmosphere_{};
  QTableWidget * arrival_view_{};
  QTableWidget * parent_star_view_{};
  QTableWidget * parent_body_view_{};
  QTimer * poll_{};
  atmosphere_odds::prepared_t prepared_;
  int64_t bodies_read_{-1};
  std::chrono::steady_clock::time_point read_{};
  };
