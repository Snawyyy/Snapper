#ifndef SNAPPER_UI_ANGLE_DIAL_H_
#define SNAPPER_UI_ANGLE_DIAL_H_

#include <QDoubleSpinBox>
#include <QPointF>
#include <QSize>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace snapper {

class LiveEdit;

// A small dial beside an angle field: a needle that shows the field's
// angle (0 up, clockwise) and turns it when dragged. Shift snaps to 15
// degrees. The field takes the turn, so it is one undo step through the
// field's live edit (live, null for a field that edits no project);
// Escape during a drag puts the angle back and drops that step.
class AngleDial final : public QWidget {
  Q_OBJECT

 public:
  // diameter is in pixels; from 36 up the dial also shows a tick every
  // 15 degrees, so the snap steps can be seen.
  AngleDial(QDoubleSpinBox* box, LiveEdit* live, QWidget* parent,
            int diameter = 24);

  QSize sizeHint() const override { return QSize(diameter_, diameter_); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void Turn(const QMouseEvent* event);
  // Sets the field to angle as one undo step.
  void SetAngle(double angle);

  QDoubleSpinBox* box_;
  LiveEdit* live_;
  int diameter_;
  double start_ = 0.0;
  // Pressed, and where; turning starts once it moves a few pixels.
  bool is_pressed_ = false;
  QPointF press_at_;
  bool is_turning_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_ANGLE_DIAL_H_
