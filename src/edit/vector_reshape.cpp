#include "edit/vector_reshape.h"

#include <QPainterPathStroker>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <set>
#include <vector>

#include "anim/vector_path.h"
#include "base/text.h"

namespace snapper {
namespace {

// Drops curves with fewer than two points (re-pointing shapes at the
// curves that stay) and points on no curve.
void DropLoose(VectorDrawing* drawing) {
  assert(drawing != nullptr);
  assert(drawing->curves.size() <= static_cast<size_t>(kMaxVectorCurves) + 1);
  std::vector<int> new_index(drawing->curves.size(), -1);
  std::vector<VectorCurve> kept;
  for (size_t c = 0; c < drawing->curves.size(); ++c) {
    VectorCurve curve = drawing->curves[c];
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
  std::set<PointId> used;
  for (const VectorCurve& curve : drawing->curves) {
    used.insert(curve.points.begin(), curve.points.end());
  }
  std::erase_if(drawing->points, [&used](const VectorPoint& point) {
    return !used.contains(point.id);
  });
}

}  // namespace

std::optional<VectorEdge> EdgeAt(const VectorDrawing& drawing,
                                 std::span<const QPointF> places, QPointF at,
                                 double reach) {
  assert(places.size() == drawing.points.size());
  assert(reach >= 0.0);
  QPainterPathStroker stroker;
  stroker.setWidth(std::max(reach * 2.0, 1.0));
  for (int c = static_cast<int>(drawing.curves.size()) - 1; c >= 0; --c) {
    const VectorCurve& curve = drawing.curves[static_cast<size_t>(c)];
    const int count = static_cast<int>(curve.points.size());
    const int segments = curve.is_closed && count > 2 ? count : count - 1;
    for (int s = 0; s < segments; ++s) {
      const QPainterPath path = SegmentPath(drawing, curve, s, places);
      if (stroker.createStroke(path).contains(at)) {
        return VectorEdge{c, s};
      }
    }
  }
  return std::nullopt;
}

Result<void> DeleteEdge(VectorDrawing* drawing, VectorEdge edge) {
  assert(drawing != nullptr);
  assert(drawing->curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  const bool has_curve =
      edge.curve >= 0 &&
      static_cast<size_t>(edge.curve) < drawing->curves.size();
  const VectorCurve* curve =
      has_curve ? &drawing->curves[static_cast<size_t>(edge.curve)] : nullptr;
  const int count =
      curve != nullptr ? static_cast<int>(curve->points.size()) : 0;
  const int segments = curve != nullptr && curve->is_closed && count > 2
                           ? count
                           : count - 1;
  if (curve == nullptr || edge.segment < 0 || edge.segment >= segments) {
    return std::unexpected(Error{Tr("That line is gone.")});
  }
  // The shapes the line outlined lose their outline.
  std::erase_if(drawing->shapes, [&edge](const VectorShape& shape) {
    return std::ranges::find(shape.curves, edge.curve) != shape.curves.end();
  });
  const std::vector<PointId> old = curve->points;
  const auto cut = static_cast<size_t>(edge.segment) + 1;
  VectorCurve& changed = drawing->curves[static_cast<size_t>(edge.curve)];
  if (curve->is_closed) {
    // Open at the edge: start after it, wrap round to its first point.
    std::vector<PointId> opened(old.begin() + static_cast<long>(cut % old.size()),
                                old.end());
    opened.insert(opened.end(), old.begin(),
                  old.begin() + static_cast<long>(cut % old.size()));
    changed.points = std::move(opened);
    changed.is_closed = false;
  } else {
    VectorCurve after{std::vector<PointId>(old.begin() + static_cast<long>(cut),
                                           old.end()),
                      false};
    changed.points.assign(old.begin(), old.begin() + static_cast<long>(cut));
    drawing->curves.push_back(std::move(after));
  }
  DropLoose(drawing);
  return {};
}

std::vector<std::pair<PointId, double>> MagnetWeights(
    const VectorDrawing& drawing, std::span<const QPointF> places,
    QPointF center, double radius) {
  assert(places.size() == drawing.points.size());
  assert(radius > 0.0);
  std::vector<std::pair<PointId, double>> weights;
  for (size_t i = 0; i < drawing.points.size(); ++i) {
    const QPointF gap = places[i] - center;
    const double distance = std::hypot(gap.x(), gap.y());
    if (distance >= radius) {
      continue;
    }
    // A smooth bump: full pull at the middle, none at the edge, with no
    // kink at either.
    const double t = distance / radius;
    const double weight = (1.0 - t * t) * (1.0 - t * t);
    weights.emplace_back(drawing.points[i].id, weight);
  }
  return weights;
}

Result<void> ShiftPoints(VectorDrawing* drawing,
                         std::span<const std::pair<PointId, QPointF>> moves) {
  assert(drawing != nullptr);
  assert(moves.size() <= static_cast<size_t>(kMaxVectorPoints));
  for (const auto& [id, step] : moves) {
    const bool is_there =
        std::ranges::any_of(drawing->points, [id](const VectorPoint& point) {
          return point.id == id;
        });
    if (!is_there) {
      return std::unexpected(Error{Tr("A point to move is gone.")});
    }
  }
  for (const auto& [id, step] : moves) {
    for (VectorPoint& point : drawing->points) {
      if (point.id == id) {
        point.at += step;
      }
    }
  }
  return {};
}

}  // namespace snapper
