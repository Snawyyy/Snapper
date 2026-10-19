#include "edit/vector_edits.h"

#include <QLineF>
#include <QPainterPathStroker>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>

#include "anim/vector_path.h"
#include "base/text.h"

namespace snapper {
namespace {

Error TooBig() { return Error{Tr("The drawing has too many points.")}; }

Error NoPoint() { return Error{Tr("That point is gone.")}; }

PointId NewPoint(VectorDrawing* drawing, int* next_id, QPointF at,
                 double curvature) {
  assert(drawing != nullptr && next_id != nullptr);
  assert(*next_id >= 1);
  const PointId id(*next_id);
  ++*next_id;
  drawing->points.push_back(VectorPoint{id, at, curvature, 1.0});
  return id;
}

bool HasRoom(const VectorDrawing& drawing, size_t points) {
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(points < 1000000);
  return drawing.points.size() + points <=
             static_cast<size_t>(kMaxVectorPoints) &&
         drawing.curves.size() < static_cast<size_t>(kMaxVectorCurves);
}

bool AreAllThere(const VectorDrawing& drawing, std::span<const PointId> ids) {
  assert(ids.size() <= static_cast<size_t>(kMaxVectorPoints));
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  return std::ranges::all_of(
      ids, [&drawing](PointId id) { return FindPoint(drawing, id) != nullptr; });
}

// Drops curves that no longer make a line and the shapes left empty,
// renumbering what shapes point to.
void Tidy(VectorDrawing* drawing) {
  assert(drawing != nullptr);
  assert(drawing->curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  std::vector<int> new_index(drawing->curves.size(), -1);
  std::vector<VectorCurve> kept;
  for (size_t c = 0; c < drawing->curves.size(); ++c) {
    VectorCurve curve = drawing->curves[c];
    const auto repeat = std::ranges::unique(curve.points);
    curve.points.erase(repeat.begin(), repeat.end());
    const bool is_loop_end = curve.points.size() > 1 &&
                             curve.points.front() == curve.points.back();
    if (is_loop_end) {
      curve.points.pop_back();
      curve.is_closed = true;
    }
    curve.is_closed = curve.is_closed && curve.points.size() >= 3;
    if (curve.points.size() >= 2) {
      new_index[c] = static_cast<int>(kept.size());
      kept.push_back(std::move(curve));
    }
  }
  drawing->curves = std::move(kept);
  for (VectorShape& shape : drawing->shapes) {
    std::vector<int> curves;
    for (int curve : shape.curves) {
      if (new_index[static_cast<size_t>(curve)] >= 0) {
        curves.push_back(new_index[static_cast<size_t>(curve)]);
      }
    }
    shape.curves = std::move(curves);
  }
  std::erase_if(drawing->shapes,
                [](const VectorShape& shape) { return shape.curves.empty(); });
}

}  // namespace

PointId PointNear(const VectorDrawing& drawing, QPointF at, double reach,
                  PointId except) {
  assert(reach >= 0.0);
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  PointId best;
  double best_gap = reach;
  for (const VectorPoint& point : drawing.points) {
    const double gap = QLineF(point.at, at).length();
    if (point.id != except && gap <= best_gap) {
      best = point.id;
      best_gap = gap;
    }
  }
  return best;
}

std::vector<int> CurvesThrough(const VectorDrawing& drawing, PointId point) {
  assert(point.IsValid());
  assert(drawing.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  std::vector<int> curves;
  for (size_t c = 0; c < drawing.curves.size(); ++c) {
    if (std::ranges::contains(drawing.curves[c].points, point)) {
      curves.push_back(static_cast<int>(c));
    }
  }
  return curves;
}

Result<std::vector<PointId>> AddCurve(VectorDrawing* drawing, int* next_id,
                                      std::span<const QPointF> places,
                                      bool is_closed, double curvature,
                                      double weld) {
  assert(drawing != nullptr && next_id != nullptr);
  assert(weld >= 0.0);
  if (places.size() < 2) {
    return std::unexpected(Error{Tr("A line needs two points.")});
  }
  if (!HasRoom(*drawing, places.size()) ||
      places.size() > static_cast<size_t>(kMaxCurvePoints)) {
    return std::unexpected(TooBig());
  }
  const PointId first = is_closed ? PointId()
                                  : PointNear(*drawing, places.front(), weld);
  const PointId last =
      is_closed ? PointId() : PointNear(*drawing, places.back(), weld, first);
  VectorCurve curve{{}, is_closed};
  for (size_t i = 0; i < places.size(); ++i) {
    const bool is_first = i == 0 && first.IsValid();
    const bool is_last = i + 1 == places.size() && last.IsValid();
    curve.points.push_back(is_first  ? first
                           : is_last ? last
                                     : NewPoint(drawing, next_id, places[i],
                                                curvature));
  }
  const std::vector<PointId> ids = curve.points;
  drawing->curves.push_back(std::move(curve));
  Tidy(drawing);
  return ids;
}

Result<PointId> ExtendFrom(VectorDrawing* drawing, int* next_id, PointId from,
                           QPointF at, double curvature) {
  assert(drawing != nullptr && next_id != nullptr);
  assert(from.value() >= 0);
  if (FindPoint(*drawing, from) == nullptr) {
    return std::unexpected(NoPoint());
  }
  if (!HasRoom(*drawing, 1)) {
    return std::unexpected(TooBig());
  }
  const PointId added = NewPoint(drawing, next_id, at, curvature);
  for (VectorCurve& curve : drawing->curves) {
    if (curve.is_closed || curve.points.empty()) {
      continue;
    }
    if (curve.points.back() == from) {
      curve.points.push_back(added);
      return added;
    }
    if (curve.points.front() == from) {
      curve.points.insert(curve.points.begin(), added);
      return added;
    }
  }
  drawing->curves.push_back(VectorCurve{{from, added}, false});
  return added;
}

Result<void> TransformPoints(VectorDrawing* drawing,
                             std::span<const PointId> ids,
                             const QTransform& transform) {
  assert(drawing != nullptr);
  assert(transform.isAffine());
  if (!AreAllThere(*drawing, ids)) {
    return std::unexpected(NoPoint());
  }
  for (PointId id : ids) {
    VectorPoint* point = FindPoint(drawing, id);
    point->at = transform.map(point->at);
  }
  return {};
}

Result<void> SetCurvature(VectorDrawing* drawing, std::span<const PointId> ids,
                          double curvature) {
  assert(drawing != nullptr);
  assert(std::isfinite(curvature));
  if (!AreAllThere(*drawing, ids)) {
    return std::unexpected(NoPoint());
  }
  for (PointId id : ids) {
    FindPoint(drawing, id)->curvature = std::clamp(curvature, 0.0, kMaxCurvature);
  }
  return {};
}

Result<void> SetWidth(VectorDrawing* drawing, std::span<const PointId> ids,
                      double width) {
  assert(drawing != nullptr);
  assert(std::isfinite(width));
  if (!AreAllThere(*drawing, ids)) {
    return std::unexpected(NoPoint());
  }
  for (PointId id : ids) {
    FindPoint(drawing, id)->width = std::clamp(width, 0.0, 10.0);
  }
  return {};
}

Result<void> DeletePoints(VectorDrawing* drawing,
                          std::span<const PointId> ids) {
  assert(drawing != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxVectorPoints));
  const std::set<PointId> gone(ids.begin(), ids.end());
  std::erase_if(drawing->points, [&gone](const VectorPoint& point) {
    return gone.contains(point.id);
  });
  for (VectorCurve& curve : drawing->curves) {
    std::erase_if(curve.points, [&gone](PointId id) { return gone.contains(id); });
  }
  Tidy(drawing);
  return {};
}

Result<void> Weld(VectorDrawing* drawing, PointId keep, PointId drop) {
  assert(drawing != nullptr);
  assert(keep.value() >= 0 && drop.value() >= 0);
  if (FindPoint(*drawing, keep) == nullptr ||
      FindPoint(*drawing, drop) == nullptr) {
    return std::unexpected(NoPoint());
  }
  if (keep == drop) {
    return {};
  }
  for (VectorCurve& curve : drawing->curves) {
    std::ranges::replace(curve.points, drop, keep);
  }
  std::erase_if(drawing->points,
                [drop](const VectorPoint& point) { return point.id == drop; });
  Tidy(drawing);
  return {};
}

Result<int> MakeShape(VectorDrawing* drawing, std::span<const PointId> ids,
                      const VectorShape& style) {
  assert(drawing != nullptr);
  assert(ids.size() <= static_cast<size_t>(kMaxVectorPoints));
  const std::set<PointId> picked(ids.begin(), ids.end());
  VectorShape shape = style;
  shape.curves.clear();
  for (size_t c = 0; c < drawing->curves.size(); ++c) {
    const bool is_inside = std::ranges::all_of(
        drawing->curves[c].points,
        [&picked](PointId id) { return picked.contains(id); });
    if (is_inside) {
      shape.curves.push_back(static_cast<int>(c));
    }
  }
  if (shape.curves.empty()) {
    return std::unexpected(
        Error{Tr("Pick the points of a whole line to make a shape.")});
  }
  if (drawing->shapes.size() >= static_cast<size_t>(kMaxVectorShapes)) {
    return std::unexpected(Error{Tr("The drawing has too many shapes.")});
  }
  const bool has_closed = std::ranges::any_of(shape.curves, [&](int c) {
    return drawing->curves[static_cast<size_t>(c)].is_closed;
  });
  shape.has_fill = shape.has_fill && has_closed;
  drawing->shapes.push_back(std::move(shape));
  return static_cast<int>(drawing->shapes.size()) - 1;
}

namespace {

Result<VectorShape*> ShapeOf(VectorDrawing* drawing, int shape) {
  assert(drawing != nullptr);
  assert(drawing->shapes.size() <= static_cast<size_t>(kMaxVectorShapes));
  const bool is_there =
      shape >= 0 && static_cast<size_t>(shape) < drawing->shapes.size();
  if (!is_there) {
    return std::unexpected(Error{Tr("That shape is gone.")});
  }
  return &drawing->shapes[static_cast<size_t>(shape)];
}

}  // namespace

Result<void> SetShapeStyle(VectorDrawing* drawing, int shape,
                           const VectorShape& style) {
  assert(drawing != nullptr);
  assert(style.stroke_width >= 0.0);
  const Result<VectorShape*> found = ShapeOf(drawing, shape);
  if (!found) {
    return std::unexpected(found.error());
  }
  VectorShape changed = style;
  changed.curves = (*found)->curves;
  changed.stroke_width = std::clamp(changed.stroke_width, 0.0,
                                    static_cast<double>(kMaxStrokeWidth));
  **found = std::move(changed);
  return {};
}

Result<void> RemoveShape(VectorDrawing* drawing, int shape) {
  assert(drawing != nullptr);
  assert(drawing->shapes.size() <= static_cast<size_t>(kMaxVectorShapes));
  const Result<VectorShape*> found = ShapeOf(drawing, shape);
  if (!found) {
    return std::unexpected(found.error());
  }
  drawing->shapes.erase(drawing->shapes.begin() + shape);
  return {};
}

Result<void> RaiseShape(VectorDrawing* drawing, int shape, int by) {
  assert(drawing != nullptr);
  assert(drawing->shapes.size() <= static_cast<size_t>(kMaxVectorShapes));
  const Result<VectorShape*> found = ShapeOf(drawing, shape);
  if (!found) {
    return std::unexpected(found.error());
  }
  const int last = static_cast<int>(drawing->shapes.size()) - 1;
  const int to = std::clamp(shape + by, 0, last);
  VectorShape moved = std::move(**found);
  drawing->shapes.erase(drawing->shapes.begin() + shape);
  drawing->shapes.insert(drawing->shapes.begin() + to, std::move(moved));
  return {};
}

int ShapeAt(const VectorDrawing& drawing, std::span<const QPointF> places,
            QPointF at, double reach) {
  assert(places.size() == drawing.points.size());
  assert(reach >= 0.0);
  for (int s = static_cast<int>(drawing.shapes.size()) - 1; s >= 0; --s) {
    const VectorShape& shape = drawing.shapes[static_cast<size_t>(s)];
    if (shape.has_fill && ShapeFillPath(drawing, shape, places).contains(at)) {
      return s;
    }
    QPainterPathStroker stroker;
    stroker.setWidth(std::max(reach * 2.0, shape.stroke_width));
    if (stroker.createStroke(ShapeStrokePath(drawing, shape, places))
            .contains(at)) {
      return s;
    }
  }
  return -1;
}

}  // namespace snapper
