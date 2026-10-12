#ifndef SNAPPER_UI_CHECKERBOARD_H_
#define SNAPPER_UI_CHECKERBOARD_H_

#include <QPainter>
#include <QRectF>

namespace snapper {

// The see-through pattern behind a transparent shot: two muted grays,
// taken from the theme so they never fight the dolls' colors. cell is
// one square's side in pixels.
void PaintCheckerboard(QPainter* painter, const QRectF& area, double cell);

// A small checker badge in the corner of a shot picture, so a
// see-through shot is spotted at a glance.
void PaintClearBadge(QPainter* painter, const QRectF& picture);

}  // namespace snapper

#endif  // SNAPPER_UI_CHECKERBOARD_H_
