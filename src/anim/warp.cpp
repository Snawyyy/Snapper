#include "anim/warp.h"

#include <QLineF>

#include <cassert>

namespace snapper {

std::vector<QPointF> RestPoints(QSize size, WarpGrid grid) {
  assert(size.width() >= 0 && size.height() >= 0);
  assert(grid.columns <= kMaxWarpCells && grid.rows <= kMaxWarpCells);
  std::vector<QPointF> points;
  const bool is_off = !grid.IsOn();
  if (is_off) {
    return points;
  }
  points.reserve(static_cast<size_t>(grid.PointCount()));
  for (int row = 0; row <= grid.rows; ++row) {
    for (int column = 0; column <= grid.columns; ++column) {
      points.emplace_back(size.width() * column / double(grid.columns),
                          size.height() * row / double(grid.rows));
    }
  }
  return points;
}

std::vector<QPointF> WarpedPoints(QSize size, WarpGrid grid,
                                  const std::vector<QPointF>& offsets) {
  assert(grid.columns <= kMaxWarpCells && grid.rows <= kMaxWarpCells);
  assert(offsets.size() <= static_cast<size_t>(kMaxWarpPoints));
  std::vector<QPointF> points = RestPoints(size, grid);
  const bool is_matching = offsets.size() == points.size();
  if (is_matching) {
    for (size_t i = 0; i < points.size(); ++i) {
      points[i] += offsets[i];
    }
  }
  return points;
}

int NearestPoint(const std::vector<QPointF>& points, QPointF spot,
                 double reach) {
  assert(points.size() <= static_cast<size_t>(kMaxWarpPoints));
  assert(reach >= 0.0);
  int best = -1;
  double best_distance = reach;
  for (size_t i = 0; i < points.size(); ++i) {
    const double distance = QLineF(points[i], spot).length();
    const bool is_closer = distance <= best_distance;
    if (is_closer) {
      best = static_cast<int>(i);
      best_distance = distance;
    }
  }
  return best;
}

}  // namespace snapper
