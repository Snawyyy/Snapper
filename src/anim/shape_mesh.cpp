#include "anim/shape_mesh.h"

#include <QRect>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/delaunay.h"
#include "anim/jitter.h"
#include "anim/mask_rings.h"
#include "anim/mesh_outline.h"
#include "anim/outline_trace.h"

namespace snapper {
namespace {

// Tries at a mesh, each with a looser outline and farther dots than
// the last, before the plain box.
constexpr int kRounds = 6;
// Times sides the triangles missed are split before they are let be.
constexpr int kSplitRounds = 6;
// The most separate outlines (shapes and holes) a mesh follows.
constexpr int kMaxRings = 64;
// Inner dots keep this share of the spacing from the outline, so every
// outline side stays a side of the triangles.
constexpr double kClearance = 0.55;
// Every dot is nudged this far (pixels, at most) so no four dots sit on
// one circle, which would leave the triangles to chance.
constexpr double kNudge = 1e-3;
// The closest dots are ever spaced, in pixels.
constexpr double kLeastSpacing = 3.0;
// A lattice triangle of side 1 has this area.
constexpr double kLatticeArea = 0.8660254037844386;
// The outline is straightened by this share of the dots' spacing, at
// most kMostStraighten pixels.
constexpr double kStraighten = 0.12;
constexpr double kMostStraighten = 6.0;
// The plain box: this many cells each way.
constexpr int kBoxCells = 4;

QPointF Nudged(QPointF spot, int i) {
  assert(i >= 0);
  assert(std::isfinite(spot.x() + spot.y()));
  return spot + QPointF(Jitter(i, 0), Jitter(i, 1)) * kNudge;
}

// The sides of outline the triangles do not have.
std::vector<MeshEdge> Missing(const std::vector<MeshEdge>& sides,
                              const std::vector<MeshTriangle>& triangles) {
  assert(sides.size() <= static_cast<size_t>(kMaxDelaunayPoints));
  assert(triangles.size() <= static_cast<size_t>(3 * kMaxDelaunayPoints));
  std::vector<MeshEdge> have;
  for (const MeshTriangle& t : triangles) {
    for (size_t k = 0; k < 3; ++k) {
      have.emplace_back(std::min(t[k], t[(k + 1) % 3]),
                        std::max(t[k], t[(k + 1) % 3]));
    }
  }
  std::sort(have.begin(), have.end());
  std::vector<MeshEdge> missing;
  for (const MeshEdge& side : sides) {
    const MeshEdge key(std::min(side.first, side.second),
                       std::max(side.first, side.second));
    const bool is_had = std::binary_search(have.begin(), have.end(), key);
    if (!is_had) {
      missing.push_back(side);
    }
  }
  return missing;
}

// Joins points into triangles. An outline side the triangles miss is
// split at its middle (a new dot) and they are made again, a few times;
// empty when the triangles do not tile.
std::vector<MeshTriangle> Triangulate(std::vector<QPointF>* points,
                                      std::vector<MeshEdge>* sides) {
  assert(points != nullptr && sides != nullptr);
  assert(points->size() <= static_cast<size_t>(kMaxDelaunayPoints));
  std::vector<MeshTriangle> triangles;
  for (int round = 0; round <= kSplitRounds; ++round) {
    triangles = Delaunay(*points);
    const std::vector<MeshEdge> missing = Missing(*sides, triangles);
    const bool is_done =
        missing.empty() || round == kSplitRounds ||
        points->size() + missing.size() > static_cast<size_t>(kMaxMeshPoints);
    if (is_done) {
      break;
    }
    for (const MeshEdge& side : missing) {
      const int middle = static_cast<int>(points->size());
      points->push_back(
          Nudged(((*points)[static_cast<size_t>(side.first)] +
                  (*points)[static_cast<size_t>(side.second)]) /
                     2.0,
                 middle));
      std::erase(*sides, side);
      sides->emplace_back(side.first, middle);
      sides->emplace_back(middle, side.second);
    }
  }
  const bool is_tiled = IsTiled(*points, triangles);
  return is_tiled ? triangles : std::vector<MeshTriangle>();
}

// Whether triangle a, b, c holds the middle of a visible pixel.
bool CoversVisible(QPointF a, QPointF b, QPointF c, const AlphaMask& mask) {
  assert(std::isfinite(a.x() + b.x() + c.x()));
  assert(std::isfinite(a.y() + b.y() + c.y()));
  const auto floor_of = [](double v) {
    return static_cast<int>(std::floor(v));
  };
  const QRect box =
      QRect(QPoint(floor_of(std::min({a.x(), b.x(), c.x()})),
                   floor_of(std::min({a.y(), b.y(), c.y()}))),
            QPoint(floor_of(std::max({a.x(), b.x(), c.x()})),
                   floor_of(std::max({a.y(), b.y(), c.y()}))))
          .intersected(mask.area());
  const auto side = [](QPointF p, QPointF q, QPointF r) {
    return (q.x() - p.x()) * (r.y() - p.y()) -
           (q.y() - p.y()) * (r.x() - p.x());
  };
  for (int y = box.top(); y <= box.bottom(); ++y) {
    for (int x = box.left(); x <= box.right(); ++x) {
      const QPointF middle(x + 0.5, y + 0.5);
      const bool is_in = side(a, b, middle) >= 0.0 &&
                         side(b, c, middle) >= 0.0 &&
                         side(c, a, middle) >= 0.0;
      const bool is_hit = is_in && mask.IsVisible(QPoint(x, y));
      if (is_hit) {
        return true;
      }
    }
  }
  return false;
}

// The triangles inside the outline, and any other that holds a visible
// pixel (where a side was missed), so nothing drawn is left out.
std::vector<MeshTriangle> Kept(const std::vector<QPointF>& points,
                               const std::vector<MeshTriangle>& triangles,
                               const MeshOutline& outline,
                               const AlphaMask& mask) {
  assert(!points.empty());
  assert(triangles.size() <= static_cast<size_t>(3 * kMaxDelaunayPoints));
  std::vector<MeshTriangle> kept;
  for (const MeshTriangle& t : triangles) {
    const QPointF a = points[static_cast<size_t>(t[0])];
    const QPointF b = points[static_cast<size_t>(t[1])];
    const QPointF c = points[static_cast<size_t>(t[2])];
    const bool is_kept = IsInsideOutline(outline, (a + b + c) / 3.0) ||
                         CoversVisible(a, b, c, mask);
    if (is_kept) {
      kept.push_back(t);
    }
  }
  return kept;
}

// The mesh of the dots the triangles use, in their order.
std::shared_ptr<const WarpMesh> Compacted(
    QSize size, MeshDetail detail, const std::vector<QPointF>& points,
    std::vector<MeshTriangle> triangles) {
  assert(!points.empty());
  assert(triangles.size() <= static_cast<size_t>(3 * kMaxDelaunayPoints));
  std::vector<int> index(points.size(), -1);
  std::vector<QPointF> used;
  for (const MeshTriangle& t : triangles) {
    for (const int corner : t) {
      const bool is_new = index[static_cast<size_t>(corner)] < 0;
      if (is_new) {
        index[static_cast<size_t>(corner)] = static_cast<int>(used.size());
        used.push_back(points[static_cast<size_t>(corner)]);
      }
    }
  }
  for (MeshTriangle& t : triangles) {
    for (int& corner : t) {
      corner = index[static_cast<size_t>(corner)];
    }
  }
  return MakeWarpMesh(size, detail, std::move(used), std::move(triangles));
}

// How far apart dots go for about count of them over a shape of area
// with an outline of length: lattice dots inside, outline dots along.
double SpacingFor(double area, double length, int count) {
  assert(area >= 0.0 && length >= 0.0);
  assert(count > 0);
  const double root =
      std::sqrt(length * length + 4.0 * count * area / kLatticeArea);
  const double spacing = (length + root) / (2.0 * count);
  return std::max(spacing, kLeastSpacing);
}

// How far the outline may be straightened, in pixels: a small part of
// the spacing the dots will roughly have, so the outline gets about as
// many dots as the inside, and never less than a pixel.
double Straightening(const AlphaMask& seen, MeshDetail detail) {
  assert(!seen.IsEmpty());
  assert(kLatticeArea > 0.0);
  const QRect area = seen.area();
  double pixels = 0.0;
  for (int y = area.top(); y <= area.bottom(); ++y) {
    for (int x = area.left(); x <= area.right(); ++x) {
      pixels += seen.IsVisible(QPoint(x, y)) ? 1.0 : 0.0;
    }
  }
  const double spacing = std::sqrt(pixels / (DotsFor(detail) * kLatticeArea));
  return std::clamp(kStraighten * spacing, 1.0, kMostStraighten);
}

// One try at the mesh, round loose: nothing when it would have too
// many dots or the triangles failed.
std::shared_ptr<const WarpMesh> TryRound(const std::vector<QImage>& drawings,
                                         QSize size, MeshDetail detail,
                                         int round, const AlphaMask& seen) {
  assert(round >= 0 && round < kRounds);
  assert(!drawings.empty());
  const double tolerance = Straightening(seen, detail) * std::pow(1.6, round);
  const AlphaMask grown(drawings, kMeshAlpha,
                        static_cast<int>(std::ceil(tolerance)) + 1);
  const std::vector<std::vector<QPointF>> traced =
      AllRings(grown, kMaxRings + 1);
  const bool is_too_scattered = traced.size() > static_cast<size_t>(kMaxRings);
  if (is_too_scattered) {
    return nullptr;
  }
  std::vector<std::vector<QPointF>> rings;
  for (const std::vector<QPointF>& ring : traced) {
    rings.push_back(SimplifyRing(ring, tolerance));
  }
  const double spacing = SpacingFor(AreaOf(rings), LengthOf(rings),
                                    DotsFor(detail)) *
                         std::pow(1.35, round);
  MeshOutline outline = OutlineOf(rings, spacing);
  const int room = kMaxMeshPoints - static_cast<int>(outline.points.size());
  const bool has_room = room >= 0 && outline.sides.size() >= 3;
  if (!has_room) {
    return nullptr;
  }
  std::vector<QPointF> points = outline.points;
  const std::vector<QPointF> inside =
      SpreadInside(outline, spacing, kClearance * spacing, room);
  points.insert(points.end(), inside.begin(), inside.end());
  const bool is_too_many = points.size() > static_cast<size_t>(kMaxMeshPoints);
  if (is_too_many) {
    return nullptr;
  }
  for (size_t i = 0; i < points.size(); ++i) {
    points[i] = Nudged(points[i], static_cast<int>(i));
  }
  std::vector<MeshEdge> sides = outline.sides;
  const std::vector<MeshTriangle> triangles = Triangulate(&points, &sides);
  const bool is_made = !triangles.empty();
  if (!is_made) {
    return nullptr;
  }
  outline.points = points;
  outline.sides = sides;
  return Compacted(size, detail, points,
                   Kept(points, triangles, outline, seen));
}

// A plain box of triangles round everything visible, for a drawing no
// round could follow.
std::shared_ptr<const WarpMesh> BoxMesh(QSize size, MeshDetail detail,
                                        const AlphaMask& seen) {
  assert(!seen.IsEmpty());
  assert(kBoxCells > 0);
  const QRect area = seen.area();
  QRect box;
  for (int y = area.top(); y <= area.bottom(); ++y) {
    for (int x = area.left(); x <= area.right(); ++x) {
      const bool is_seen = seen.IsVisible(QPoint(x, y));
      if (is_seen) {
        box = box.united(QRect(x, y, 1, 1));
      }
    }
  }
  const QRectF grown = QRectF(box).adjusted(-2, -2, 2, 2);
  std::vector<QPointF> points;
  for (int row = 0; row <= kBoxCells; ++row) {
    for (int column = 0; column <= kBoxCells; ++column) {
      points.emplace_back(grown.left() + grown.width() * column / kBoxCells,
                          grown.top() + grown.height() * row / kBoxCells);
    }
  }
  std::vector<MeshTriangle> triangles;
  for (int row = 0; row < kBoxCells; ++row) {
    for (int column = 0; column < kBoxCells; ++column) {
      const int tl = row * (kBoxCells + 1) + column;
      const int bl = tl + kBoxCells + 1;
      triangles.push_back({tl, tl + 1, bl + 1});
      triangles.push_back({tl, bl + 1, bl});
    }
  }
  return MakeWarpMesh(size, detail, std::move(points), std::move(triangles));
}

}  // namespace

std::shared_ptr<const WarpMesh> BuildShapeMesh(
    const std::vector<QImage>& drawings, MeshDetail detail) {
  assert(drawings.size() <= 1024);
  assert(static_cast<int>(detail) >= 0 &&
         static_cast<int>(detail) < kMeshDetailCount);
  QSize size(0, 0);
  for (const QImage& drawing : drawings) {
    size = size.expandedTo(drawing.size());
  }
  const AlphaMask seen(drawings, kMeshAlpha, 0);
  const bool is_visible = !drawings.empty() && !seen.IsEmpty();
  if (!is_visible) {
    return nullptr;
  }
  for (int round = 0; round < kRounds; ++round) {
    std::shared_ptr<const WarpMesh> mesh =
        TryRound(drawings, size, detail, round, seen);
    const bool is_made = mesh != nullptr;
    if (is_made) {
      return mesh;
    }
  }
  return BoxMesh(size, detail, seen);
}

int DotsFor(MeshDetail detail) {
  assert(static_cast<int>(detail) >= 0);
  assert(static_cast<int>(detail) < kMeshDetailCount);
  switch (detail) {
    case MeshDetail::kFew:
      return 24;
    case MeshDetail::kNormal:
      return 60;
    case MeshDetail::kMany:
      return 140;
  }
  return 60;
}

}  // namespace snapper
