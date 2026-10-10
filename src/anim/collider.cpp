#include "anim/collider.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

#include "anim/warp_shape.h"

namespace snapper {
namespace {

double Cross(QPointF a, QPointF b) { return a.x() * b.y() - a.y() * b.x(); }

double Dot(QPointF a, QPointF b) { return a.x() * b.x() + a.y() * b.y(); }

// The first and last edge of the unbroken run of edges that holds edge
// i: neighbours on the ring follow each other in edges, end to start.
std::pair<size_t, size_t> RunOf(const std::vector<ColliderEdge>& edges,
                                size_t i) {
  assert(i < edges.size());
  assert(edges.size() <= kMaxColliderEdges);
  const size_t count = edges.size();
  size_t first = i;
  size_t last = i;
  for (size_t step = 1; step < count; ++step) {
    const size_t before = (first + count - 1) % count;
    const bool is_joined = edges[before].b == edges[first].a && before != last;
    if (!is_joined) {
      break;
    }
    first = before;
  }
  for (size_t step = 1; step < count; ++step) {
    const size_t after = (last + 1) % count;
    const bool is_joined = edges[last].b == edges[after].a && after != first;
    if (!is_joined) {
      break;
    }
    last = after;
  }
  return {first, last};
}

// The spot just outside the run of edges (first to last, going round)
// nearest to point, on an edge that faces ghost: where a point that
// came in through it goes back out, straight across the outline and on
// the side it came from, never through to the far side of a thin limb.
// Any edge of the run will do when none faces ghost.
QPointF OutOfRun(const std::vector<ColliderEdge>& edges,
                 std::pair<size_t, size_t> run, QPointF ghost,
                 QPointF point) {
  assert(run.first < edges.size() && run.second < edges.size());
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  const size_t count = edges.size();
  double least = -1.0;
  QPointF best = point;
  for (int pass = 0; pass < 2 && least < 0.0; ++pass) {
    for (size_t step = 0; step < count; ++step) {
      const ColliderEdge& edge = edges[(run.first + step) % count];
      const bool is_facing = pass == 1 || Dot(ghost - edge.a, edge.out) > 0.0;
      const QPointF along = edge.b - edge.a;
      const double t = std::clamp(
          Dot(point - edge.a, along) / std::max(Dot(along, along), 1e-12),
          0.0, 1.0);
      const QPointF spot = edge.a + along * t;
      const double away = Dot(point - spot, point - spot);
      const bool is_nearer = is_facing && (least < 0.0 || away < least);
      if (is_nearer) {
        least = away;
        best = spot + edge.out * kColliderClearance;
      }
      const bool is_end = (run.first + step) % count == run.second;
      if (is_end) {
        break;
      }
    }
  }
  return best;
}

}  // namespace

std::vector<QPointF> ShotPoints(const std::vector<QPointF>& points,
                                const WarpGrid& grid, QSize size,
                                const std::vector<QPointF>& grid_points,
                                const QTransform& to_shot) {
  assert(size.width() >= 0 && size.height() >= 0);
  assert(points.size() <= kMaxColliderEdges);
  const bool is_bent = grid.IsOn() && static_cast<int>(grid_points.size()) ==
                                          grid.PointCount();
  std::vector<QPointF> shot;
  shot.reserve(points.size());
  for (const QPointF& point : points) {
    shot.push_back(is_bent ? CarriedAt(grid, size, grid_points, point)
                           : to_shot.map(point));
  }
  return shot;
}

std::vector<QPointF> ShotDots(const Collider& collider, const WarpGrid& grid,
                              QSize size, const std::vector<QPointF>& points,
                              const QTransform& to_shot) {
  assert(collider.dots.size() <= static_cast<size_t>(kMaxColliderDots));
  assert(size.width() >= 0 && size.height() >= 0);
  std::vector<QPointF> drawn;
  drawn.reserve(collider.dots.size());
  for (const ColliderDot& dot : collider.dots) {
    drawn.push_back(dot.at);
  }
  return ShotPoints(drawn, grid, size, points, to_shot);
}

std::vector<ColliderEdge> ColliderEdges(const Collider& collider,
                                        const std::vector<QPointF>& shot_dots,
                                        const QTransform& to_shot) {
  assert(shot_dots.size() == collider.dots.size());
  assert(collider.ring_size <= kMaxColliderDots);
  assert(collider.dots.empty() || collider.ring_size > 0);
  // The ring runs with the shape on its right; a mirror swaps sides.
  const double side = to_shot.determinant() < 0.0 ? -1.0 : 1.0;
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
    edges.push_back({shot_dots[i], shot_dots[next], out});
  }
  return edges;
}

QPointF PushedFromRest(const std::vector<ColliderEdge>& edges, QPointF ghost,
                       QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(std::isfinite(ghost.x()) && std::isfinite(ghost.y()));
  const QPointF walk = point - ghost;
  double last = -1.0;
  size_t crossed = edges.size();
  for (size_t i = 0; i < edges.size(); ++i) {
    const QPointF along = edges[i].b - edges[i].a;
    const double across = Cross(walk, along);
    const bool is_parallel = std::abs(across) < 1e-12;
    if (is_parallel) {
      continue;
    }
    const QPointF start = edges[i].a - ghost;
    const double s = Cross(start, along) / across;
    const double t = Cross(start, walk) / across;
    const bool is_crossed = s >= 0.0 && s <= 1.0 && t >= 0.0 && t <= 1.0;
    const bool is_last = is_crossed && s > last;
    if (is_last) {
      last = s;
      crossed = i;
    }
  }
  const bool is_in =
      crossed < edges.size() && Dot(walk, edges[crossed].out) < 0.0;
  return is_in ? OutOfRun(edges, RunOf(edges, crossed), ghost, point)
               : point;
}

QPointF OutToward(const std::vector<ColliderEdge>& edges, QPointF point,
                  QPointF toward) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(std::isfinite(toward.x()) && std::isfinite(toward.y()));
  const QPointF walk = toward - point;
  double first = 2.0;
  QPointF out;
  for (const ColliderEdge& edge : edges) {
    const QPointF along = edge.b - edge.a;
    const double across = Cross(walk, along);
    const bool is_leaving =
        std::abs(across) > 1e-12 && Dot(walk, edge.out) > 0.0;
    if (!is_leaving) {
      continue;
    }
    const QPointF start = edge.a - point;
    const double s = Cross(start, along) / across;
    const double t = Cross(start, walk) / across;
    const bool is_first =
        s >= 0.0 && s <= 1.0 && t >= 0.0 && t <= 1.0 && s < first;
    if (is_first) {
      first = s;
      out = edge.out;
    }
  }
  const bool is_found = first <= 1.0;
  return is_found ? point + walk * first + out * kColliderClearance : point;
}

}  // namespace snapper
