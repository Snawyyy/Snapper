#include "anim/grid_fold.h"

#include <array>
#include <cassert>
#include <cmath>

#include "anim/warp_shape.h"

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

// What folds together: a grid cell (its two drawn triangles, eased by
// its four corners) or one mesh triangle (eased by its three).
struct Patch final {
  std::array<MeshTriangle, 2> triangles{};
  int triangle_count = 0;
  std::array<int, 4> corners{};
  int corner_count = 0;
};

std::vector<Patch> PatchesOf(const WarpGrid& grid) {
  assert(grid.IsOn());
  assert(grid.PointCount() <= kMaxWarpPoints);
  const std::vector<MeshTriangle> triangles = WarpTriangles(grid);
  std::vector<Patch> patches;
  const bool is_mesh = grid.IsMesh();
  if (is_mesh) {
    for (const MeshTriangle& t : triangles) {
      patches.push_back({{t, MeshTriangle{}}, 1, {t[0], t[1], t[2], 0}, 3});
    }
    return patches;
  }
  // A grid's cells come as pairs: top-left, top-right, bottom-right and
  // top-left, bottom-right, bottom-left.
  for (size_t i = 0; i + 1 < triangles.size(); i += 2) {
    const MeshTriangle& upper = triangles[i];
    const MeshTriangle& lower = triangles[i + 1];
    patches.push_back(
        {{upper, lower}, 2, {upper[0], upper[1], lower[2], upper[2]}, 4});
  }
  return patches;
}

// Whether a drawn triangle of patch is folded.
bool IsFolded(const Patch& patch, const std::vector<QPointF>& before,
              const std::vector<QPointF>& points) {
  assert(points.size() == before.size());
  assert(patch.triangle_count >= 1 && patch.triangle_count <= 2);
  for (int i = 0; i < patch.triangle_count; ++i) {
    const MeshTriangle& c = patch.triangles[static_cast<size_t>(i)];
    const auto at = [](const std::vector<QPointF>& p, int corner) {
      return p[static_cast<size_t>(corner)];
    };
    const double was = Area(at(before, c[0]), at(before, c[1]),
                            at(before, c[2]));
    const double now = Area(at(points, c[0]), at(points, c[1]),
                            at(points, c[2]));
    const bool is_drawn = std::abs(was) > 1e-9;
    const bool is_folded = is_drawn && now / was < kLeastArea;
    if (is_folded) {
      return true;
    }
  }
  return false;
}

}  // namespace

void Unfold(const WarpGrid& grid, const std::vector<QPointF>& before,
            std::vector<QPointF>* points) {
  assert(points != nullptr && points->size() == before.size());
  assert(static_cast<int>(before.size()) == grid.PointCount());
  const std::vector<Patch> patches = PatchesOf(grid);
  for (int round = 0; round < kUnfoldRounds; ++round) {
    std::vector<bool> is_eased(points->size(), false);
    bool is_any = false;
    for (const Patch& patch : patches) {
      const bool is_folded = IsFolded(patch, before, *points);
      if (is_folded) {
        is_any = true;
        for (int i = 0; i < patch.corner_count; ++i) {
          is_eased[static_cast<size_t>(patch.corners[static_cast<size_t>(i)])] =
              true;
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
