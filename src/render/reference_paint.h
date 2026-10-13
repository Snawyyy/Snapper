#ifndef SNAPPER_RENDER_REFERENCE_PAINT_H_
#define SNAPPER_RENDER_REFERENCE_PAINT_H_

#include <QImage>
#include <QPainter>
#include <QSize>

#include "model/reference.h"

namespace snapper {

// A reference's picture of the moment, drawn on the Pose tab stage only:
// placed, flipped and tinted as it says, at its opacity (half that once
// its video has ended). painter draws in output pixels with the canvas
// corner at 0,0; scale is output pixels per canvas pixel. Nothing that
// makes the shot's own pictures calls this.
void PaintReference(const Reference& reference, const QImage& picture,
                    QSize canvas, double scale, bool is_ended,
                    QPainter* painter);

// picture in grey, its dark parts in the tint's color, alpha kept.
QImage TintedPicture(const QImage& picture, ReferenceTint tint);

}  // namespace snapper

#endif  // SNAPPER_RENDER_REFERENCE_PAINT_H_
