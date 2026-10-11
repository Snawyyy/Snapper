#ifndef SNAPPER_UI_TRACK_PAD_H_
#define SNAPPER_UI_TRACK_PAD_H_

#include <QDoubleSpinBox>
#include <QPointF>
#include <QSize>
#include <QWidget>

#include "ui/bound_drag.h"

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace snapper {

class LiveEdit;

// A pad, as wide as it is given and 96 px tall, that moves two number
// fields (x and y) like sliding a puck over graph paper: drag and the
// grid slides under the yellow puck, so a move reads as a push. One unit
// of the field per pixel; Shift moves ten times finer; double click
// sends both back to zero; Escape during a drag cancels. The fields are
// the edit path (one undo step per drag).
class TrackPad final : public QWidget {
  Q_OBJECT

 public:
  TrackPad(QDoubleSpinBox* x, QDoubleSpinBox* y, LiveEdit* live,
           QWidget* parent);

  QSize sizeHint() const override { return QSize(96, 96); }

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void Drag(const QMouseEvent* event);

  QDoubleSpinBox* x_;
  QDoubleSpinBox* y_;
  BoundDrag drag_;
  QPointF press_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_TRACK_PAD_H_
