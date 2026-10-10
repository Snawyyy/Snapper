#include "anim/grid_fold.h"

#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// A drawn triangle squashed below this share of its area before, or
// turned over, counts as folded; a fold is undone by taking back this
// share of its corners' moves, at most kUnfoldRounds times.
constexpr double kLeastArea = 0.01;
constexpr double kTakeBack = 0.25;
constexpr int kUnfoldRounds = 16;

// Twice the signed area of the triangle a, b, c.
double Area(QPointF a, QPointF b, QPointF c) {
  assert(std::isfinite(a.x() + a.y()));
  assert(std::isfinite(b.x() + b.y() + c.x() + c.y()));
  return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}

// Whether a drawn triangle of the cell at top-left corner tl is folded:
// the cell is drawn as the triangles tl, tr, br and tl, br, bl.
bool IsFolded(WarpGrid grid, const std::vector<QPointF>& before,
              const std::vector<QPointF>& points, size_t tl) {
  assert(points.size() == before.size());
  assert(tl + static_cast<size_t>(grid.columns) + 2 < points.size());
  const size_t tr = tl + 1;
  const size_t bl = tl + static_cast<size_t>(grid.columns) + 1;
  const size_t br = bl + 1;
  const size_t corners[2][3] = {{tl, tr, br}, {tl, br, bl}};
  for (const auto& c : corners) {
    const double was = Area(before[c[0]], before[c[1]], before[c[2]]);
    const double now = Area(points[c[0]], points[c[1]], points[c[2]]);
    const bool is_drawn = std::abs(was) > 1e-9;
    const bool is_folded = is_drawn && now / was < kLeastArea;
    if (is_folded) {
      return true;
    }
  }
  return false;
}

}  // namespace

void Unfold(WarpGrid grid, const std::vector<QPointF>& before,
            std::vector<QPointF>* points) {
  assert(points != nullptr && points->size() == before.size());
  assert(grid.IsOn());
  const size_t across = static_cast<size_t>(grid.columns) + 1;
  for (int round = 0; round < kUnfoldRounds; ++round) {
    std::vector<bool> is_eased(points->size(), false);
    bool is_any = false;
    for (int row = 0; row < grid.rows; ++row) {
      for (int column = 0; column < grid.columns; ++column) {
        const size_t tl = static_cast<size_t>(row) * across +
                          static_cast<size_t>(column);
        const bool is_folded = IsFolded(grid, before, *points, tl);
        if (is_folded) {
          is_any = true;
          for (const size_t i : {tl, tl + 1, tl + across, tl + across + 1}) {
            is_eased[i] = true;
          }
        }
      }
    }
    if (!is_any) {
      return;
    }
    for (size_t i = 0; i < points->size(); ++i) {
      const bool is_easing = is_eased[i];
      if (is_easing) {
        (*points)[i] -= ((*points)[i] - before[i]) * kTakeBack;
      }
    }
  }
}

}  // namespace snapper
