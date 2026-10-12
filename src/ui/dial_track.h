#ifndef SNAPPER_UI_DIAL_TRACK_H_
#define SNAPPER_UI_DIAL_TRACK_H_

#include <QPointF>
#include <QSize>
#include <QWidget>

#include "model/smart_dial.h"

class QContextMenuEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace snapper {

// A smart dial as a sunken groove with a tick for each stop (filled for
// the rest stop, named under it) and a yellow knob at the value. Drag
// or turn the wheel to turn it; it snaps to stops unless Ctrl is held,
// and Shift turns finely. Double click goes back to rest; Escape during
// a drag puts it back. The widget only reports; the box turns the dial.
class DialTrack final : public QWidget {
  Q_OBJECT

 public:
  explicit DialTrack(QWidget* parent);

  // What it shows; value is 0 to 1.
  void Show(const SmartDial& dial, double value);
  double value() const { return value_; }
  // The stop whose tick is at x (widget pixels), or -1.
  int StopAt(double x) const;

  QSize sizeHint() const override { return QSize(180, 40); }
  QSize minimumSizeHint() const override { return QSize(90, 40); }

 signals:
  // A drag or a wheel burst starts, moves to value, and lands.
  void Started();
  void Turned(double value);
  void Landed();
  // Escape during a drag.
  void Cancelled();
  // Double click: back to the rest stop.
  void Rested();
  // A right click on the tick of stop, or on the groove (-1).
  void MenuAsked(int stop, QPoint global);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  // The groove's ends, in widget pixels.
  double Left() const;
  double Right() const;
  double XOf(double value) const;
  // The value under x, snapped unless is_free.
  double ValueAt(double x, bool is_free) const;

  SmartDial dial_;
  double value_ = 0.5;
  bool is_dragging_ = false;
  // Where a drag started, for fine steps with Shift.
  double press_x_ = 0.0;
  double press_value_ = 0.0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_DIAL_TRACK_H_
