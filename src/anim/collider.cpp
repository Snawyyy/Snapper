#include "anim/collider.h"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// dot (drawing pixels) carried by the cell of the warped grid it is in.
QPointF Carried(QPointF dot, WarpGrid grid, QSize size,
                const std::vector<QPointF>& points) {
  assert(grid.IsOn());
  assert(static_cast<int>(points.size()) == grid.PointCount());
  const double u = std::clamp(dot.x() / std::max(size.width(), 1), 0.0, 1.0) *
                   grid.columns;
  const double v = std::clamp(dot.y() / std::max(size.height(), 1), 0.0, 1.0) *
                   grid.rows;
  const int column = std::min(static_cast<int>(u), grid.columns - 1);
  const int row = std::min(static_cast<int>(v), grid.rows - 1);
  const double fu = u - column;
  const double fv = v - row;
  const auto at = [&](int c, int r) {
    return points[static_cast<size_t>(r * (grid.columns + 1) + c)];
  };
  const QPointF top = at(column, row) * (1.0 - fu) + at(column + 1, row) * fu;
  const QPointF bottom =
      at(column, row + 1) * (1.0 - fu) + at(column + 1, row + 1) * fu;
  return top * (1.0 - fv) + bottom * fv;
}

}  // namespace

std::vector<QPointF> ShotDots(const Collider& collider, WarpGrid grid,
                              QSize size, const std::vector<QPointF>& points,
                              const QTransform& to_shot) {
  assert(collider.dots.size() <= static_cast<size_t>(kMaxColliderDots));
  assert(size.width() >= 0 && size.height() >= 0);
  const bool is_bent =
      grid.IsOn() && static_cast<int>(points.size()) == grid.PointCount();
  std::vector<QPointF> shot;
  for (const ColliderDot& dot : collider.dots) {
    shot.push_back(is_bent ? Carried(dot.at, grid, size, points)
                           : to_shot.map(dot.at));
  }
  return shot;
}

std::vector<ColliderEdge> ColliderEdges(const Collider& collider,
                                        const std::vector<QPointF>& shot_dots,
                                        const QTransform& to_shot) {
  assert(shot_dots.size() == collider.dots.size());
  assert(collider.ring_size <= kMaxColliderDots);
  assert(collider.dots.empty() || collider.ring_size > 0);
  const double det = to_shot.determinant();
  const double scale = std::sqrt(std::abs(det));
  // The ring runs with the shape on its right; a mirror swaps sides.
  const double side = det < 0.0 ? -1.0 : 1.0;
  std::vector<ColliderEdge> edges;
  const size_t count = collider.dots.size();
  for (size_t i = 0; i < count; ++i) {
    const size_t next = (i + 1) % count;
    const int from = collider.dots[i].slot;
    const int to = collider.dots[next].slot;
    const bool is_neighbour = (from + 1) % collider.ring_size == to;
    const bool is_edge = count > 1 && is_neighbour;
    if (!is_edge) {
      continue;
    }
    const QPointF step = shot_dots[next] - shot_dots[i];
    const double length = std::hypot(step.x(), step.y());
    const bool is_flat = length < 1e-9;
    if (is_flat) {
      continue;
    }
    const QPointF out(step.y() / length * side, -step.x() / length * side);
    edges.push_back({shot_dots[i], shot_dots[next], out,
                     collider.depth * scale});
  }
  return edges;
}

QPointF PushedOut(const std::vector<ColliderEdge>& edges, QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(edges.size() <= kMaxColliderEdges);
  double least = 0.0;
  QPointF pushed = point;
  bool has_hit = false;
  for (const ColliderEdge& edge : edges) {
    const QPointF along = edge.b - edge.a;
    const QPointF rel = point - edge.a;
    const double len2 = along.x() * along.x() + along.y() * along.y();
    const double t = (rel.x() * along.x() + rel.y() * along.y()) / len2;
    const double behind = -(rel.x() * edge.out.x() + rel.y() * edge.out.y());
    const bool is_in_strip = t >= 0.0 && t <= 1.0;
    const bool is_inside = behind > 0.0 && behind <= edge.depth;
    const bool is_nearer = !has_hit || behind < least;
    const bool is_hit = is_in_strip && is_inside && is_nearer;
    if (is_hit) {
      has_hit = true;
      least = behind;
      pushed = point + edge.out * behind;
    }
  }
  return pushed;
}

QPointF PushedOutDeep(const std::vector<ColliderEdge>& edges, QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(edges.size() <= kMaxColliderEdges);
  const ColliderEdge* nearest = nullptr;
  double least = 0.0;
  double foot = 0.0;
  for (const ColliderEdge& edge : edges) {
    const QPointF along = edge.b - edge.a;
    const QPointF rel = point - edge.a;
    const double len2 = along.x() * along.x() + along.y() * along.y();
    const double t = (rel.x() * along.x() + rel.y() * along.y()) / len2;
    const QPointF spot = edge.a + along * std::clamp(t, 0.0, 1.0);
    const double away = std::hypot(point.x() - spot.x(), point.y() - spot.y());
    const bool is_nearer = nearest == nullptr || away < least;
    if (is_nearer) {
      nearest = &edge;
      least = away;
      foot = t;
    }
  }
  const bool is_along = nearest != nullptr && foot >= 0.0 && foot <= 1.0;
  if (!is_along) {
    return point;
  }
  const double behind = -((point - nearest->a).x() * nearest->out.x() +
                          (point - nearest->a).y() * nearest->out.y());
  return behind > 0.0 ? point + nearest->out * behind : point;
}

}  // namespace snapper
