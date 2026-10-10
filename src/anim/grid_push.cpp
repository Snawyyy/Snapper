#include "anim/grid_push.h"

#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/grid_fold.h"
#include "anim/warp.h"
#include "anim/warp_shape.h"

namespace snapper {
namespace {

// Spots tested along each drawn edge between two grid points, evenly
// between its ends.
constexpr int kEdgeSpots = 7;
// Rounds of pushing before the push spreads, and after, so pushes of
// neighbours settle together; never a loop without an end.
constexpr int kPushRounds = 4;
constexpr int kFixRounds = 4;
// The grid a shape mesh's least push spread matches: this many cells
// each way over its drawing.
constexpr double kMeshGridCells = 3.0;
// A push shorter than this, in shot pixels, is no push.
constexpr double kStill = 1e-6;

double Length(QPointF v) { return std::hypot(v.x(), v.y()); }

// Where the spot that walked from ghost to point ends up under field.
QPointF Pushed(const PushField& field, QPointF ghost, QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(!field.edges.empty());
  const bool is_near =
      QRectF(ghost, point).normalized().adjusted(-1, -1, 1, 1).intersects(
          field.box);
  return is_near ? PushedFromRest(field.edges, ghost, point) : point;
}

// Pushes each grid point out; true when one moved.
bool PushPoints(const std::vector<PushField>& fields,
                std::vector<QPointF>* points) {
  assert(points != nullptr);
  assert(points->size() <= static_cast<size_t>(kMaxWarpPoints));
  bool is_moved = false;
  for (const PushField& field : fields) {
    for (size_t i = 0; i < points->size(); ++i) {
      const QPointF to = Pushed(field, field.ghosts[i], (*points)[i]);
      is_moved = is_moved || Length(to - (*points)[i]) > kStill;
      (*points)[i] = to;
    }
  }
  return is_moved;
}

// Pushes the drawn edge from point a to point b out at each spot along
// it, moving both ends so the spot moves by what it needs (the end
// nearer the spot more); true when one moved.
bool PushEdge(const PushField& field, size_t a, size_t b,
              std::vector<QPointF>* points) {
  assert(points != nullptr);
  assert(a < points->size() && b < points->size());
  bool is_moved = false;
  for (int spot = 1; spot <= kEdgeSpots; ++spot) {
    const double t = spot / (kEdgeSpots + 1.0);
    const QPointF ghost =
        field.ghosts[a] * (1.0 - t) + field.ghosts[b] * t;
    const QPointF at = (*points)[a] * (1.0 - t) + (*points)[b] * t;
    const QPointF push = Pushed(field, ghost, at) - at;
    const bool is_pushed = Length(push) > kStill;
    if (is_pushed) {
      const double share = (1.0 - t) * (1.0 - t) + t * t;
      (*points)[a] += push * ((1.0 - t) / share);
      (*points)[b] += push * (t / share);
      is_moved = true;
    }
  }
  return is_moved;
}

// Pushes every drawn edge of the grid out; true when one moved.
bool PushEdges(const std::vector<MeshEdge>& edges,
               const std::vector<PushField>& fields,
               std::vector<QPointF>* points) {
  assert(points != nullptr);
  assert(edges.size() <= static_cast<size_t>(3 * kMaxWarpPoints));
  bool is_moved = false;
  for (const PushField& field : fields) {
    for (const MeshEdge& edge : edges) {
      is_moved = PushEdge(field, static_cast<size_t>(edge.first),
                          static_cast<size_t>(edge.second), points) ||
                 is_moved;
    }
  }
  return is_moved;
}

// Pushes points and edges until nothing moves, at most rounds times.
void Settle(const std::vector<MeshEdge>& edges,
            const std::vector<PushField>& fields, int rounds,
            std::vector<QPointF>* points) {
  assert(points != nullptr);
  assert(rounds > 0 && rounds <= kPushRounds + kFixRounds);
  for (int round = 0; round < rounds; ++round) {
    const bool is_points_moved = PushPoints(fields, points);
    const bool is_edges_moved = PushEdges(edges, fields, points);
    const bool is_settled = !is_points_moved && !is_edges_moved;
    if (is_settled) {
      return;
    }
  }
}

// Each point's push carried to its neighbours by spread: per axis the
// strongest push each way wins, so two pushes the same way do not add
// up to more than either.
void Spread(const WarpGrid& grid, const std::vector<double>& reach,
            const std::vector<QPointF>& before,
            std::vector<QPointF>* points) {
  assert(points != nullptr && points->size() == before.size());
  assert(reach.empty() || reach.size() == before.size());
  const size_t count = before.size();
  const double least = MinPushSpread(grid);
  std::vector<QPointF> push(count);
  for (size_t i = 0; i < count; ++i) {
    push[i] = (*points)[i] - before[i];
  }
  for (size_t j = 0; j < count; ++j) {
    double right = 0.0;
    double left = 0.0;
    double down = 0.0;
    double up = 0.0;
    for (size_t i = 0; i < count; ++i) {
      const double spread =
          std::max(reach.empty() ? 0.0 : reach[i], least);
      const QPointF part =
          push[i] * PullWeight(grid, static_cast<int>(i),
                               static_cast<int>(j), spread);
      right = std::max(right, part.x());
      left = std::min(left, part.x());
      down = std::max(down, part.y());
      up = std::min(up, part.y());
    }
    (*points)[j] = before[j] + QPointF(right + left, down + up);
  }
}

// Pushes each point out once more, but keeps any triangle from
// turning over against before: its corners go back to where they were.
void PushOutWhole(const WarpGrid& grid, const std::vector<QPointF>& before,
                  const std::vector<PushField>& fields,
                  std::vector<QPointF>* points) {
  assert(points != nullptr && points->size() == before.size());
  assert(grid.IsMesh());
  const std::vector<QPointF> eased = *points;
  PushPoints(fields, points);
  const auto turn = [](const std::vector<QPointF>& p, const MeshTriangle& t) {
    const QPointF a = p[static_cast<size_t>(t[0])];
    const QPointF b = p[static_cast<size_t>(t[1])];
    const QPointF c = p[static_cast<size_t>(t[2])];
    return (b.x() - a.x()) * (c.y() - a.y()) -
           (b.y() - a.y()) * (c.x() - a.x());
  };
  for (int round = 0; round < kFixRounds; ++round) {
    bool is_any = false;
    for (const MeshTriangle& t : grid.mesh->triangles) {
      const bool is_turned = turn(before, t) * turn(*points, t) <= 0.0;
      if (is_turned) {
        for (const int corner : t) {
          const auto c = static_cast<size_t>(corner);
          is_any = is_any || (*points)[c] != eased[c];
          (*points)[c] = eased[c];
        }
      }
    }
    if (!is_any) {
      return;
    }
  }
}

}  // namespace

double MinPushSpread(const WarpGrid& grid) {
  assert(grid.IsOn());
  assert(kMinPushSpread > 0.0);
  const bool is_mesh = grid.IsMesh();
  if (!is_mesh) {
    return kMinPushSpread;
  }
  const QSize size = grid.mesh->size;
  const double grid_cell = (size.width() + size.height()) / 2.0 /
                           kMeshGridCells;
  return std::max(kMinPushSpread, kMinPushSpread * grid_cell /
                                      grid.mesh->cell);
}

std::vector<double> ReachesOf(const RigPiece& rig) {
  assert(rig.warp.PointCount() <= kMaxWarpPoints);
  assert(rig.warp_reach.size() <= static_cast<size_t>(kMaxWarpPoints));
  const bool has_reach =
      static_cast<int>(rig.warp_reach.size()) == rig.warp.PointCount();
  return has_reach ? rig.warp_reach : std::vector<double>();
}

void PushGrid(const WarpGrid& grid, const std::vector<double>& reach,
              const std::vector<PushField>& fields,
              std::vector<QPointF>* points) {
  assert(points != nullptr);
  assert(static_cast<int>(points->size()) == grid.PointCount());
  const bool has_fields = !fields.empty();
  if (!has_fields) {
    return;
  }
  for (const PushField& field : fields) {
    assert(field.ghosts.size() == points->size());
  }
  const std::vector<QPointF> before = *points;
  const std::vector<MeshEdge> edges = WarpEdges(grid);
  Settle(edges, fields, kPushRounds, points);
  const bool is_moved = *points != before;
  if (!is_moved) {
    return;
  }
  const bool has_reach =
      reach.size() == before.size() || reach.empty();
  Spread(grid, has_reach ? reach : std::vector<double>(), before, points);
  Settle(edges, fields, kFixRounds, points);
  Unfold(grid, before, points);
  // A mesh's triangles are small, so easing a squashed one back can
  // leave a dot in the collider; it goes back out, squashed but whole.
  const bool is_mesh = grid.IsMesh();
  if (is_mesh) {
    PushOutWhole(grid, before, fields, points);
  }
}

}  // namespace snapper
