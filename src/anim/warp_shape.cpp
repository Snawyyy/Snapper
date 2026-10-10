#include "anim/warp_shape.h"

#include <QLineF>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

#include "anim/warp.h"

namespace snapper {
namespace {

// A spot this far outside a triangle (as a share of its weights) still
// counts as in it, so a spot on a shared side picks one.
constexpr double kOnSide = 1e-9;

// value on a grid at spot, mixed in both directions inside the cell it
// is in; off the drawing, the nearest edge cell carried on past it.
QPointF GridCarried(const WarpGrid& grid, QSize size,
                    const std::vector<QPointF>& values, QPointF spot) {
  assert(grid.IsOn() && !grid.IsMesh());
  assert(static_cast<int>(values.size()) == grid.PointCount());
  const double u = spot.x() / std::max(size.width(), 1) * grid.columns;
  const double v = spot.y() / std::max(size.height(), 1) * grid.rows;
  const int column = std::clamp(static_cast<int>(std::floor(u)), 0,
                                grid.columns - 1);
  const int row =
      std::clamp(static_cast<int>(std::floor(v)), 0, grid.rows - 1);
  const double fu = u - column;
  const double fv = v - row;
  const auto at = [&](int c, int r) {
    return values[static_cast<size_t>(r * (grid.columns + 1) + c)];
  };
  const QPointF top = at(column, row) * (1.0 - fu) + at(column + 1, row) * fu;
  const QPointF bottom =
      at(column, row + 1) * (1.0 - fu) + at(column + 1, row + 1) * fu;
  return top * (1.0 - fv) + bottom * fv;
}

// The weights of spot against triangle a, b, c (they add up to 1), or
// all zero for a flat triangle.
std::array<double, 3> Weights(QPointF a, QPointF b, QPointF c,
                              QPointF spot) {
  assert(std::isfinite(a.x() + a.y() + b.x() + b.y()));
  assert(std::isfinite(c.x() + c.y() + spot.x() + spot.y()));
  const double area =
      (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
  const bool is_flat = std::abs(area) < 1e-12;
  if (is_flat) {
    return {0.0, 0.0, 0.0};
  }
  const double wa = ((b.x() - spot.x()) * (c.y() - spot.y()) -
                     (c.x() - spot.x()) * (b.y() - spot.y())) /
                    area;
  const double wb = ((c.x() - spot.x()) * (a.y() - spot.y()) -
                     (a.x() - spot.x()) * (c.y() - spot.y())) /
                    area;
  return {wa, wb, 1.0 - wa - wb};
}

// How far spot is from the triangle a, b, c (0 inside).
double Away(QPointF a, QPointF b, QPointF c, QPointF spot) {
  assert(std::isfinite(spot.x() + spot.y()));
  assert(std::isfinite(a.x() + b.x() + c.x()));
  const auto to_side = [spot](QPointF p, QPointF q) {
    const QPointF pq = q - p;
    const double length2 = pq.x() * pq.x() + pq.y() * pq.y();
    const double t =
        length2 < 1e-12
            ? 0.0
            : std::clamp(((spot - p).x() * pq.x() + (spot - p).y() * pq.y()) /
                             length2,
                         0.0, 1.0);
    return QLineF(spot, p + pq * t).length();
  };
  return std::min({to_side(a, b), to_side(b, c), to_side(c, a)});
}

QPointF MeshCarried(const WarpGrid& grid, QSize size,
                    const std::vector<QPointF>& values, QPointF spot) {
  assert(grid.IsMesh());
  assert(static_cast<int>(values.size()) == grid.PointCount());
  const std::vector<QPointF> rest = MeshRest(*grid.mesh, size);
  double nearest = -1.0;
  std::array<double, 3> best{0.0, 0.0, 0.0};
  const MeshTriangle* found = nullptr;
  for (const MeshTriangle& t : grid.mesh->triangles) {
    const QPointF a = rest[static_cast<size_t>(t[0])];
    const QPointF b = rest[static_cast<size_t>(t[1])];
    const QPointF c = rest[static_cast<size_t>(t[2])];
    const std::array<double, 3> w = Weights(a, b, c, spot);
    const bool is_inside =
        w[0] >= -kOnSide && w[1] >= -kOnSide && w[2] >= -kOnSide &&
        w[0] + w[1] + w[2] > 0.5;
    const double away = is_inside ? 0.0 : Away(a, b, c, spot);
    const bool is_nearer = nearest < 0.0 || away < nearest;
    if (is_nearer) {
      nearest = away;
      best = w;
      found = &t;
    }
    if (is_inside) {
      break;
    }
  }
  assert(found != nullptr);
  return values[static_cast<size_t>((*found)[0])] * best[0] +
         values[static_cast<size_t>((*found)[1])] * best[1] +
         values[static_cast<size_t>((*found)[2])] * best[2];
}

}  // namespace

std::vector<MeshEdge> WarpEdges(const WarpGrid& grid) {
  assert(grid.columns >= 0 && grid.rows >= 0);
  assert(grid.PointCount() <= kMaxWarpPoints);
  const bool is_mesh = grid.IsMesh();
  if (is_mesh) {
    return grid.mesh->edges;
  }
  std::vector<MeshEdge> edges;
  const int across = grid.columns + 1;
  for (int point = 0; point < grid.PointCount(); ++point) {
    const bool has_right = point % across < grid.columns;
    const bool has_below = point / across < grid.rows;
    if (has_right) {
      edges.emplace_back(point, point + 1);
    }
    if (has_below) {
      edges.emplace_back(point, point + across);
    }
  }
  return edges;
}

std::vector<MeshEdge> WarpBends(const WarpGrid& grid) {
  assert(grid.columns >= 0 && grid.rows >= 0);
  assert(grid.PointCount() <= kMaxWarpPoints);
  std::vector<MeshEdge> bends;
  const bool is_mesh = grid.IsMesh();
  if (!is_mesh) {
    return bends;
  }
  // Each side with the corner across from it, sorted so the two
  // triangles of a side sit next to each other.
  std::vector<std::pair<MeshEdge, int>> sides;
  for (const MeshTriangle& t : grid.mesh->triangles) {
    for (size_t k = 0; k < 3; ++k) {
      const int a = t[k];
      const int b = t[(k + 1) % 3];
      sides.push_back({{std::min(a, b), std::max(a, b)}, t[(k + 2) % 3]});
    }
  }
  std::sort(sides.begin(), sides.end());
  for (size_t i = 0; i + 1 < sides.size(); ++i) {
    const bool is_shared = sides[i].first == sides[i + 1].first;
    if (is_shared) {
      const int a = sides[i].second;
      const int b = sides[i + 1].second;
      bends.emplace_back(std::min(a, b), std::max(a, b));
    }
  }
  return bends;
}

std::vector<MeshTriangle> WarpTriangles(const WarpGrid& grid) {
  assert(grid.columns >= 0 && grid.rows >= 0);
  assert(grid.PointCount() <= kMaxWarpPoints);
  const bool is_mesh = grid.IsMesh();
  if (is_mesh) {
    return grid.mesh->triangles;
  }
  std::vector<MeshTriangle> triangles;
  const int width = grid.columns + 1;
  for (int row = 0; row < grid.rows; ++row) {
    for (int column = 0; column < grid.columns; ++column) {
      const int tl = row * width + column;
      const int tr = tl + 1;
      const int bl = tl + width;
      const int br = bl + 1;
      triangles.push_back({tl, tr, br});
      triangles.push_back({tl, br, bl});
    }
  }
  return triangles;
}

QSizeF CellSize(const WarpGrid& grid, QSize size) {
  assert(grid.IsOn());
  assert(size.width() >= 0 && size.height() >= 0);
  const bool is_mesh = grid.IsMesh();
  if (is_mesh) {
    const double x = grid.mesh->size.isEmpty()
                         ? 1.0
                         : size.width() /
                               static_cast<double>(grid.mesh->size.width());
    return QSizeF(grid.mesh->cell * x, grid.mesh->cell * x);
  }
  return QSizeF(size.width() / static_cast<double>(grid.columns),
                size.height() / static_cast<double>(grid.rows));
}

QPointF CarriedAt(const WarpGrid& grid, QSize size,
                  const std::vector<QPointF>& values, QPointF spot) {
  assert(std::isfinite(spot.x()) && std::isfinite(spot.y()));
  assert(size.width() >= 0 && size.height() >= 0);
  const bool is_fitting = grid.IsOn() && static_cast<int>(values.size()) ==
                                             grid.PointCount();
  if (!is_fitting) {
    return QPointF();
  }
  const bool is_mesh = grid.IsMesh();
  return is_mesh ? MeshCarried(grid, size, values, spot)
                 : GridCarried(grid, size, values, spot);
}

std::vector<int> MirroredPoints(const WarpGrid& from, QSize from_size,
                                const WarpGrid& to, QSize to_size) {
  assert(from.PointCount() <= kMaxWarpPoints);
  assert(to.PointCount() <= kMaxWarpPoints);
  const int count = from.PointCount();
  std::vector<int> paired(static_cast<size_t>(count), -1);
  const bool is_same_grid = from == to && !from.IsMesh() && from.IsOn();
  if (is_same_grid) {
    for (int point = 0; point < count; ++point) {
      paired[static_cast<size_t>(point)] = MirroredPoint(from, point);
    }
    return paired;
  }
  const bool is_both_on = from.IsOn() && to.IsOn();
  if (!is_both_on) {
    return paired;
  }
  const std::vector<QPointF> rest = RestPoints(from_size, from);
  const std::vector<QPointF> targets = RestPoints(to_size, to);
  const QSizeF cell = CellSize(to, to_size);
  const double reach = std::max(cell.width(), cell.height());
  const double x = from_size.width() > 0
                       ? to_size.width() /
                             static_cast<double>(from_size.width())
                       : 1.0;
  const double y = from_size.height() > 0
                       ? to_size.height() /
                             static_cast<double>(from_size.height())
                       : 1.0;
  for (size_t i = 0; i < rest.size(); ++i) {
    const QPointF spot((from_size.width() - rest[i].x()) * x,
                       rest[i].y() * y);
    paired[i] = NearestPoint(targets, spot, reach);
  }
  return paired;
}

}  // namespace snapper
