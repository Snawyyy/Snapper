#ifndef SNAPPER_UI_LOOP_TILE_H_
#define SNAPPER_UI_LOOP_TILE_H_

#include <QAbstractButton>
#include <QColor>
#include <QRectF>
#include <QSize>
#include <QTimer>

#include "ui/dot_kinds.h"
#include "ui/hover_fade.h"

class QColor;
class QEnterEvent;
class QEvent;
class QPaintEvent;
class QPainter;

namespace snapper {

class MotionManager;

// A tile: a tiny cartoon over a name. The cartoon sits still, and loops
// only while the pointer is over the tile (never in Off). A press
// squashes the tile and it springs back. A tile in use has a Teto red
// outline, thick when every picked thing is of it, thin when only some
// are. What the cartoon is, is the subclass's (PaintArt).
class LoopTile : public QAbstractButton {
  Q_OBJECT

 public:
  LoopTile(const QString& label, MotionManager* motion, QWidget* parent);

  void SetShare(Share share);
  Share share() const { return share_; }
  // Whether the cartoon is looping right now.
  bool is_animating() const { return timer_.isActive(); }
  // Where the loop is, 0 to 1.
  double phase() const { return phase_; }
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  // Draws the cartoon inside art at phase 0 to 1 of its loop.
  virtual void PaintArt(QPainter* painter, const QRectF& art, double phase,
                        const QColor& ink) const = 0;

 private:
  void Step();

  MotionManager* motion_;
  HoverFade* fade_;
  QTimer timer_;
  double phase_;
  Share share_ = Share::kNone;
};

}  // namespace snapper

#endif  // SNAPPER_UI_LOOP_TILE_H_
