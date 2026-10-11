#ifndef SNAPPER_UI_MOTION_ART_H_
#define SNAPPER_UI_MOTION_ART_H_

#include <QColor>
#include <QRectF>

#include "model/doll.h"

class QPainter;

namespace snapper {

// What the tile of a dot motion is called.
QString MotionName(WarpMotionKind kind);

// Draws a tiny looping cartoon of a dot moving by kind inside area at
// phase 0 to 1 of its loop: None a dot sitting, Wave a dot riding a
// wave, Pulse and Breathe a dot swelling (quick and slow), Sway a dot
// swinging, Shiver a dot trembling.
void PaintMotion(QPainter* painter, const QRectF& area, WarpMotionKind kind,
                 double phase, const QColor& ink);

}  // namespace snapper

#endif  // SNAPPER_UI_MOTION_ART_H_
