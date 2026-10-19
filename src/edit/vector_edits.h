#ifndef SNAPPER_EDIT_VECTOR_EDITS_H_
#define SNAPPER_EDIT_VECTOR_EDITS_H_

#include <QPointF>
#include <QTransform>

#include <span>
#include <vector>

#include "base/error.h"
#include "model/vector.h"

namespace snapper {

// The edits of one vector drawing, as the vector tools make them. New
// points take ids from next_id, which is the character's counter.

// A new curve through places. An end within weld of an existing point
// is welded to it instead of making a new point, so a stroke drawn from
// a line's end carries the line on. Answers the curve's point ids in
// order.
Result<std::vector<PointId>> AddCurve(VectorDrawing* drawing, int* next_id,
                                      std::span<const QPointF> places,
                                      bool is_closed, double curvature,
                                      double weld);

// Adds a point at at after from: at the end of the open curve that ends
// at from, or as a new two-point curve from it. Answers the new point.
Result<PointId> ExtendFrom(VectorDrawing* drawing, int* next_id,
                           PointId from, QPointF at, double curvature);

// Moves each of ids by transform (around nothing: transform holds it).
Result<void> TransformPoints(VectorDrawing* drawing,
                             std::span<const PointId> ids,
                             const QTransform& transform);

Result<void> SetCurvature(VectorDrawing* drawing, std::span<const PointId> ids,
                          double curvature);
Result<void> SetWidth(VectorDrawing* drawing, std::span<const PointId> ids,
                      double width);

// Removes ids: curves lose them (a curve left with fewer than two points
// goes, a closed one with fewer than three opens), shapes lose curves
// that went, and a shape with no curve left goes.
Result<void> DeletePoints(VectorDrawing* drawing, std::span<const PointId> ids);

// Welds drop onto keep: every curve through drop goes through keep, and
// drop goes. A curve that would pass the same point twice in a row loses
// the repeat; an open curve whose two ends meet closes.
Result<void> Weld(VectorDrawing* drawing, PointId keep, PointId drop);

// The point within reach of at, nearest first, or none.
PointId PointNear(const VectorDrawing& drawing, QPointF at, double reach,
                  PointId except = PointId());

// A shape over every curve whose points are all in ids (closed curves
// filled), with style's colors and width. Answers its index.
Result<int> MakeShape(VectorDrawing* drawing, std::span<const PointId> ids,
                      const VectorShape& style);

// Sets shape's look (fill, stroke, width), keeping its curves.
Result<void> SetShapeStyle(VectorDrawing* drawing, int shape,
                           const VectorShape& style);
Result<void> RemoveShape(VectorDrawing* drawing, int shape);
// Moves shape by places in the draw order (positive is to the front).
Result<void> RaiseShape(VectorDrawing* drawing, int shape, int by);

// Index of the front-most shape whose fill holds at, or a stroke within
// reach of it, with places the drawing's points as shown; -1 for none.
int ShapeAt(const VectorDrawing& drawing, std::span<const QPointF> places,
            QPointF at, double reach);

// The curves that pass through point.
std::vector<int> CurvesThrough(const VectorDrawing& drawing, PointId point);

}  // namespace snapper

#endif  // SNAPPER_EDIT_VECTOR_EDITS_H_
