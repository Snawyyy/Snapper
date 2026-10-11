// RigCanvas's pivot snap: a joint catching on the middle and edges of
// its drawing.

#include <cassert>
#include <cmath>

#include "anim/guides.h"
#include "edit/history_manager.h"
#include "ui/rig_canvas.h"

namespace snapper {

QPointF RigCanvas::SnappedPivot(const Doll& doll, const QString& piece,
                                QPointF wanted, bool* is_snapped) const {
  assert(is_snapped != nullptr);
  assert(std::isfinite(wanted.x()) && std::isfinite(wanted.y()));
  const QSize size = ArtSizeOf(doll, piece);
  const bool has_art = size.isValid();
  if (!has_art) {
    return wanted;
  }
  // A few screen pixels, in the drawing's own.
  const QTransform world = World();
  const double scale = std::hypot(world.m11(), world.m12());
  const double reach = scale > 1e-9 ? kGuideReach / scale : 0.0;
  const GuideSnap snap = SnapToGuides(
      QRectF(wanted, QSizeF(0.0, 0.0)), {QRectF(QPointF(), QSizeF(size))},
      reach, {});
  *is_snapped = !snap.lines.empty();
  return wanted + snap.shift;
}

}  // namespace snapper
