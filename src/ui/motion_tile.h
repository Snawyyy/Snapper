#ifndef SNAPPER_UI_MOTION_TILE_H_
#define SNAPPER_UI_MOTION_TILE_H_

#include "model/doll.h"
#include "ui/loop_tile.h"

namespace snapper {

// One tile of how a dot moves by itself (None, Wave, Pulse, Breathe,
// Sway, Shiver): a tiny cartoon of the motion over its name.
class MotionTile final : public LoopTile {
  Q_OBJECT

 public:
  MotionTile(WarpMotionKind kind, MotionManager* motion, QWidget* parent);

  WarpMotionKind kind() const { return kind_; }

 protected:
  void PaintArt(QPainter* painter, const QRectF& art, double phase,
                const QColor& ink) const override;

 private:
  WarpMotionKind kind_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_MOTION_TILE_H_
