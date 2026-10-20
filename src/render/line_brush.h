#ifndef SNAPPER_RENDER_LINE_BRUSH_H_
#define SNAPPER_RENDER_LINE_BRUSH_H_

#include <QPainter>
#include <QPointF>

#include <span>

#include "model/vector.h"

namespace snapper {

// Draws shape's line with its brush (anything but LineBrush::kSolid),
// dab by dab along each of its curves, the width following the points'
// widths. The dabs are placed the same way every time, so a still line
// never shimmers between frames.
void PaintBrushedStroke(const VectorDrawing& drawing, const VectorShape& shape,
                        std::span<const QPointF> places, QPainter* painter);

}  // namespace snapper

#endif  // SNAPPER_RENDER_LINE_BRUSH_H_
