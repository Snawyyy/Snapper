#ifndef SNAPPER_ANIM_VECTOR_PATH_H_
#define SNAPPER_ANIM_VECTOR_PATH_H_

#include <QPainterPath>
#include <QPointF>

#include <span>
#include <vector>

#include "model/vector.h"

namespace snapper {

// The two handles of a curve's point i, where places holds the curve's
// points (as posed) in order and curvature their roundness: the line
// leaves along the direction from the point before to the point after,
// each handle as long as curvature of its own side. An open curve's end
// has its handles on the point. A corner (0) has both on the point.
struct CurveHandles final {
  QPointF in{};
  QPointF out{};
};
CurveHandles HandlesAt(std::span<const QPointF> places,
                       std::span<const double> curvature, int i,
                       bool is_closed);

// The path of curve, with places the drawing's points as posed (in the
// drawing's point order).
QPainterPath CurvePath(const VectorDrawing& drawing, const VectorCurve& curve,
                       std::span<const QPointF> places);

// The path of one segment of curve: from its point segment to the next
// (the last segment of a closed curve goes back to the first point).
QPainterPath SegmentPath(const VectorDrawing& drawing, const VectorCurve& curve,
                         int segment, std::span<const QPointF> places);

// The path of shape: its closed curves for the fill (even-odd), or every
// curve for a stroke.
QPainterPath ShapeFillPath(const VectorDrawing& drawing,
                           const VectorShape& shape,
                           std::span<const QPointF> places);
QPainterPath ShapeStrokePath(const VectorDrawing& drawing,
                             const VectorShape& shape,
                             std::span<const QPointF> places);

// The drawing's points at rest, in its point order.
std::vector<QPointF> RestPlaces(const VectorDrawing& drawing);

}  // namespace snapper

#endif  // SNAPPER_ANIM_VECTOR_PATH_H_
