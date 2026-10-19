#include "anim/vector_path.h"

#include <QLineF>

#include <cassert>
#include <cmath>

namespace snapper {

CurveHandles HandlesAt(std::span<const QPointF> places,
                       std::span<const double> curvature, int i,
                       bool is_closed) {
  assert(places.size() == curvature.size());
  assert(i >= 0 && static_cast<size_t>(i) < places.size());
  const int count = static_cast<int>(places.size());
  const QPointF at = places[static_cast<size_t>(i)];
  const bool is_end = !is_closed && (i == 0 || i == count - 1);
  const double round = curvature[static_cast<size_t>(i)];
  if (count < 3 || is_end || round <= 0.0) {
    return {at, at};
  }
  const QPointF before = places[static_cast<size_t>((i + count - 1) % count)];
  const QPointF after = places[static_cast<size_t>((i + 1) % count)];
  const QPointF across = after - before;
  const double span = QLineF(before, after).length();
  if (span <= 1e-9) {
    return {at, at};
  }
  const double in_side = QLineF(before, at).length();
  const double out_side = QLineF(at, after).length();
  const QPointF way = across / span;
  return {at - way * (round * in_side), at + way * (round * out_side)};
}

namespace {

// The places and roundness of curve's points, in curve order.
struct CurvePoints final {
  std::vector<QPointF> places;
  std::vector<double> curvature;
};

CurvePoints PointsOf(const VectorDrawing& drawing, const VectorCurve& curve,
                     std::span<const QPointF> places) {
  assert(places.size() == drawing.points.size());
  assert(curve.points.size() <= static_cast<size_t>(kMaxCurvePoints));
  CurvePoints out;
  for (PointId id : curve.points) {
    const int index = PointIndex(drawing, id);
    if (index < 0) {
      continue;
    }
    out.places.push_back(places[static_cast<size_t>(index)]);
    out.curvature.push_back(drawing.points[static_cast<size_t>(index)].curvature);
  }
  return out;
}

}  // namespace

QPainterPath CurvePath(const VectorDrawing& drawing, const VectorCurve& curve,
                       std::span<const QPointF> places) {
  assert(places.size() == drawing.points.size());
  assert(curve.points.size() <= static_cast<size_t>(kMaxCurvePoints));
  const CurvePoints points = PointsOf(drawing, curve, places);
  const int count = static_cast<int>(points.places.size());
  QPainterPath path;
  if (count == 0) {
    return path;
  }
  path.moveTo(points.places.front());
  const int segments = curve.is_closed && count > 2 ? count : count - 1;
  for (int s = 0; s < segments; ++s) {
    const int next = (s + 1) % count;
    const CurveHandles from = HandlesAt(points.places, points.curvature, s,
                                        curve.is_closed);
    const CurveHandles to = HandlesAt(points.places, points.curvature, next,
                                      curve.is_closed);
    path.cubicTo(from.out, to.in, points.places[static_cast<size_t>(next)]);
  }
  if (curve.is_closed && count > 2) {
    path.closeSubpath();
  }
  return path;
}

QPainterPath ShapeFillPath(const VectorDrawing& drawing,
                           const VectorShape& shape,
                           std::span<const QPointF> places) {
  assert(places.size() == drawing.points.size());
  assert(shape.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  QPainterPath path;
  path.setFillRule(Qt::OddEvenFill);
  for (int curve : shape.curves) {
    const VectorCurve& line = drawing.curves[static_cast<size_t>(curve)];
    if (line.is_closed) {
      path.addPath(CurvePath(drawing, line, places));
    }
  }
  return path;
}

QPainterPath ShapeStrokePath(const VectorDrawing& drawing,
                             const VectorShape& shape,
                             std::span<const QPointF> places) {
  assert(places.size() == drawing.points.size());
  assert(shape.curves.size() <= static_cast<size_t>(kMaxVectorCurves));
  QPainterPath path;
  for (int curve : shape.curves) {
    path.addPath(
        CurvePath(drawing, drawing.curves[static_cast<size_t>(curve)], places));
  }
  return path;
}

std::vector<QPointF> RestPlaces(const VectorDrawing& drawing) {
  assert(drawing.points.size() <= static_cast<size_t>(kMaxVectorPoints));
  std::vector<QPointF> places;
  places.reserve(drawing.points.size());
  for (const VectorPoint& point : drawing.points) {
    places.push_back(point.at);
  }
  assert(places.size() == drawing.points.size());
  return places;
}

}  // namespace snapper
