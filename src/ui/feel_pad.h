#ifndef SNAPPER_UI_FEEL_PAD_H_
#define SNAPPER_UI_FEEL_PAD_H_

#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QWidget>

#include <vector>

#include "anim/node_feel.h"

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;

namespace snapper {

// The feel at a point of the pad: soft (left) to firm (right), light
// (bottom) to heavy (top). Points outside the pad are held to its edge.
Feel FeelAt(const QPointF& point, const QRectF& area);
// Where a feel sits on the pad: the reverse of FeelAt.
QPointF PointOf(const Feel& feel, const QRectF& area);

// A square to set how the picked dots feel with one drag. A puck shows
// the feel; when the picked dots differ, a hollow puck shows each. A
// press jumps the puck to the pointer and moving drags it; Escape
// during the drag puts everything back. It only reports: the panel
// decides what the feel does.
class FeelPad final : public QWidget {
  Q_OBJECT

 public:
  explicit FeelPad(QWidget* parent);

  // The pucks to show; ignored while a drag is going, so the puck
  // follows the pointer, not the project.
  void SetFeels(const std::vector<Feel>& feels);
  bool is_dragging() const { return is_dragging_; }
  // How many pucks show: one for one feel, a hollow one for each if
  // the picked dots differ.
  size_t puck_count() const { return feels_.size(); }
  // The square the puck moves in, inside the widget.
  QRectF Area() const;
  QSize sizeHint() const override;

 signals:
  // A press: a burst of Moved is coming, to be one undo step.
  void Grabbed();
  void Moved(const Feel& feel);
  void Released();
  // Escape: undo the burst.
  void Cancelled();

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  void Drag(const QPointF& point);
  void PaintPucks(QPainter* painter) const;

  std::vector<Feel> feels_;
  Feel dragged_;
  bool is_dragging_ = false;
};

}  // namespace snapper

#endif  // SNAPPER_UI_FEEL_PAD_H_
