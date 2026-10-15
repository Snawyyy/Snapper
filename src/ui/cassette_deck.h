#ifndef SNAPPER_UI_CASSETTE_DECK_H_
#define SNAPPER_UI_CASSETTE_DECK_H_

#include <QWidget>

#include "ui/managers.h"

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QRectF;

namespace snapper {

// An old tape player under the Cast tabs whose cassette follows the
// playhead: the reels turn with every frame it moves, forward or back,
// and the tape winds from the left reel onto the right one from the
// first frame to the last. Dragging across it scrubs the playhead;
// Escape puts it back where the drag began.
class CassetteDeck final : public QWidget {
  Q_OBJECT

 public:
  explicit CassetteDeck(const Managers& managers);

  bool hasHeightForWidth() const override { return true; }
  int heightForWidth(int width) const override;
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void PaintBody(QPainter* painter, const QRectF& body) const;
  void PaintWindow(QPainter* painter, const QRectF& window) const;
  // One reel at middle: its wound tape pack and the turning hub.
  void PaintReel(QPainter* painter, const QPointF& middle, double hub,
                 double pack, double turn) const;
  // How far the tape has wound, 0 at the first frame to 1 at the last.
  double Wound() const;

  Managers managers_;
  bool is_dragging_ = false;
  double drag_x_ = 0.0;
  int drag_frame_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_UI_CASSETTE_DECK_H_
