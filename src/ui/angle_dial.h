#ifndef SNAPPER_UI_ANGLE_DIAL_H_
#define SNAPPER_UI_ANGLE_DIAL_H_

#include <QDoubleSpinBox>
#include <QSize>
#include <QWidget>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

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
  AngleDial(QDoubleSpinBox* box, LiveEdit* live, QWidget* parent);

  QSize sizeHint() const override { return QSize(24, 24); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void Turn(const QMouseEvent* event);

  QDoubleSpinBox* box_;
  LiveEdit* live_;
  double start_ = 0.0;
  bool is_turning_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_ANGLE_DIAL_H_
