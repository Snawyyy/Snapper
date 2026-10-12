#ifndef SNAPPER_UI_FADER_H_
#define SNAPPER_UI_FADER_H_

#include <QDoubleSpinBox>
#include <QSize>
#include <QWidget>

#include "ui/bound_drag.h"

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QWheelEvent;

namespace snapper {

class LiveEdit;

// A mixing-desk fader beside a number field: a groove that lights up in
// Teto red up to a raised cap. Drag the cap, or drag from anywhere on
// the groove, to set the number (a press alone changes nothing, so a
// double click never jumps it); Shift moves it ten times finer; the wheel
// steps it; double click puts it back to its neutral value; Escape
// during a drag cancels. A tick marks the neutral value. The field is
// the edit path (one undo step per drag), so the fader never touches
// the project itself.
class Fader final : public QWidget {
  Q_OBJECT

 public:
  // The groove spans the field's minimum to shown_high (past it the cap
  // rests at the end); neutral is where double click returns.
  Fader(QDoubleSpinBox* box, LiveEdit* live, double shown_high,
        double neutral, QWidget* parent);

  QSize sizeHint() const override { return QSize(120, 22); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  // The cap's centre x for value, and the value for a cap at x.
  double XOf(double value) const;
  double ValueAt(double x) const;
  void Drag(const QMouseEvent* event);

  QDoubleSpinBox* box_;
  BoundDrag drag_;
  double shown_high_;
  double neutral_;
  // Where the press landed, for the fine (relative) drag.
  double press_x_ = 0.0;
  bool is_pressed_ = false;
  bool is_hot_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FADER_H_
