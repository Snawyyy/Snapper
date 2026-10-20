#ifndef SNAPPER_EDIT_VECTOR_RESHAPE_H_
#define SNAPPER_EDIT_VECTOR_RESHAPE_H_

#include <QPointF>

#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "base/error.h"
#include "model/vector.h"

namespace snapper {

// The shaping edits of a vector drawing: cutting a line and pulling
// points with the magnet.

// One segment of a curve: from its point segment to the next.
struct VectorEdge final {
  int curve = -1;
  int segment = -1;

  bool operator==(const VectorEdge&) const = default;
};

// The segment within reach of at, front-most curve first, with places
// the drawing's points as shown; none when no line is that near.
std::optional<VectorEdge> EdgeAt(const VectorDrawing& drawing,
                                 std::span<const QPointF> places, QPointF at,
                                 double reach);

// Cuts edge out of its curve: an open curve splits in two there, a
// closed one opens at it. Shapes that used the curve go (their outline
// is broken), as do points left on no line.
Result<void> DeleteEdge(VectorDrawing* drawing, VectorEdge edge);

// How much the magnet moves each point: 1 at center, easing to 0 at
// radius (points as placed in places, in the drawing's point order).
// Points past radius are left out.
std::vector<std::pair<PointId, double>> MagnetWeights(
    const VectorDrawing& drawing, std::span<const QPointF> places,
    QPointF center, double radius);

// Moves each point by its own step.
Result<void> ShiftPoints(VectorDrawing* drawing,
                         std::span<const std::pair<PointId, QPointF>> moves);

}  // namespace snapper

#endif  // SNAPPER_EDIT_VECTOR_RESHAPE_H_
