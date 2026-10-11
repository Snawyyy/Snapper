#include "ui/motion_tile.h"

#include <cassert>

#include "ui/motion_art.h"

namespace snapper {

MotionTile::MotionTile(WarpMotionKind kind, MotionManager* motion,
                       QWidget* parent)
    : LoopTile(MotionName(kind), motion, parent), kind_(kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kWarpMotionKindCount);
}

void MotionTile::PaintArt(QPainter* painter, const QRectF& art, double phase,
                          const QColor& ink) const {
  assert(painter != nullptr);
  assert(phase >= 0.0 && phase <= 1.0);
  PaintMotion(painter, art, kind_, phase, ink);
}

}  // namespace snapper
