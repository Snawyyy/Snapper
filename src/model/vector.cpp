#include "model/vector.h"

#include <algorithm>
#include <cassert>

namespace snapper {

int PointIndex(const VectorDrawing& drawing, PointId id) {
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(id.value() >= 0);
  const auto found = std::ranges::find_if(
      drawing.points, [id](const VectorPoint& point) { return point.id == id; });
  return found == drawing.points.end()
             ? -1
             : static_cast<int>(found - drawing.points.begin());
}

const VectorPoint* FindPoint(const VectorDrawing& drawing, PointId id) {
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(id.value() >= 0);
  const int index = PointIndex(drawing, id);
  return index < 0 ? nullptr : &drawing.points[static_cast<size_t>(index)];
}

VectorPoint* FindPoint(VectorDrawing* drawing, PointId id) {
  assert(drawing != nullptr);
  assert(id.value() >= 0);
  const VectorPoint* point =
      FindPoint(static_cast<const VectorDrawing&>(*drawing), id);
  return const_cast<VectorPoint*>(point);
}

namespace {

bool IsSoundCurve(const VectorDrawing& drawing, const VectorCurve& curve) {
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(curve.points.size() < 1000000);
  if (curve.points.size() > static_cast<size_t>(kMaxCurvePoints)) {
    return false;
  }
  return std::ranges::all_of(curve.points, [&drawing](PointId id) {
    return FindPoint(drawing, id) != nullptr;
  });
}

}  // namespace

bool IsSound(const VectorDrawing& drawing) {
  assert(drawing.curves.size() < 1000000);
  assert(drawing.shapes.size() < 1000000);
  const bool is_small =
      drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints) &&
      drawing.curves.size() <= static_cast<size_t>(kMaxVectorCurves) &&
      drawing.shapes.size() <= static_cast<size_t>(kMaxVectorShapes);
  if (!is_small) {
    return false;
  }
  const bool are_curves_sound =
      std::ranges::all_of(drawing.curves, [&drawing](const VectorCurve& c) {
        return IsSoundCurve(drawing, c);
      });
  const int curve_count = static_cast<int>(drawing.curves.size());
  const bool are_shapes_sound = std::ranges::all_of(
      drawing.shapes, [curve_count](const VectorShape& shape) {
        return std::ranges::all_of(shape.curves, [curve_count](int curve) {
          return curve >= 0 && curve < curve_count;
        });
      });
  return are_curves_sound && are_shapes_sound;
}

}  // namespace snapper
