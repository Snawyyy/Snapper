#include "anim/mesh_outline.h"

#include <QLineF>
#include <QRectF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/delaunay.h"

namespace snapper {
namespace {

// Two outline dots closer than this, in pixels, are one dot.
constexpr double kSameSpot = 0.5;
// The most rows and dots across a lattice is laid out over.
constexpr int kMaxLatticeSide = 4096;

double Distance(QPointF a, QPointF b) {
  assert(std::isfinite(a.x() + a.y()));
  assert(std::isfinite(b.x() + b.y()));
  return QLineF(a, b).length();
}

// The index of the outline dot at spot, adding one when there is none.
int DotAt(QPointF spot, std::vector<QPointF>* points) {
  assert(points != nullptr);
  assert(points->size() <= static_cast<size_t>(kMaxDelaunayPoints));
  for (size_t i = 0; i < points->size(); ++i) {
    const bool is_same = Distance((*points)[i], spot) < kSameSpot;
    if (is_same) {
      return static_cast<int>(i);
    }
  }
  points->push_back(spot);
  return static_cast<int>(points->size()) - 1;
}

double FromSide(QPointF spot, QPointF a, QPointF b) {
  assert(std::isfinite(spot.x() + spot.y()));
  assert(std::isfinite(a.x() + b.x()));
  const QPointF ab = b - a;
  const double length2 = ab.x() * ab.x() + ab.y() * ab.y();
  const bool is_point = length2 < 1e-12;
  const QPointF as = spot - a;
  const double t =
      is_point ? 0.0
               : std::clamp((as.x() * ab.x() + as.y() * ab.y()) / length2,
                            0.0, 1.0);
  return Distance(spot, a + ab * t);
}

}  // namespace

MeshOutline OutlineOf(const std::vector<std::vector<QPointF>>& rings,
                      double spacing) {
  assert(spacing > 0.0);
  assert(rings.size() <= static_cast<size_t>(kMaxMeshPoints));
  MeshOutline outline;
  for (const std::vector<QPointF>& ring : rings) {
    const bool is_ring = ring.size() >= 3;
    if (!is_ring) {
      continue;
    }
    const size_t first = outline.sides.size();
    int previous = DotAt(ring[0], &outline.points);
    const int start = previous;
    for (size_t i = 0; i < ring.size(); ++i) {
      const QPointF from = ring[i];
      const QPointF to = ring[(i + 1) % ring.size()];
      const int pieces = std::clamp(
          static_cast<int>(std::ceil(Distance(from, to) / spacing)), 1,
          kMaxMeshPoints);
      for (int k = 1; k <= pieces; ++k) {
        const bool is_back = i + 1 == ring.size() && k == pieces;
        const int dot =
            is_back ? start
                    : DotAt(from + (to - from) * (k / double(pieces)),
                            &outline.points);
        const bool is_side = dot != previous;
        if (is_side) {
          outline.sides.emplace_back(previous, dot);
        }
        previous = dot;
      }
      const bool is_too_many =
          outline.points.size() > static_cast<size_t>(kMaxDelaunayPoints) / 2;
      if (is_too_many) {
        break;
      }
    }
    assert(outline.sides.size() >= first);
  }
  return outline;
}

double AreaOf(const std::vector<std::vector<QPointF>>& rings) {
  assert(rings.size() <= static_cast<size_t>(kMaxMeshPoints));
  double area = 0.0;
  for (const std::vector<QPointF>& ring : rings) {
    for (size_t i = 0; i < ring.size(); ++i) {
      const QPointF a = ring[i];
      const QPointF b = ring[(i + 1) % ring.size()];
      area += a.x() * b.y() - b.x() * a.y();
    }
  }
  assert(std::isfinite(area));
  return std::abs(area) / 2.0;
}

double LengthOf(const std::vector<std::vector<QPointF>>& rings) {
  assert(rings.size() <= static_cast<size_t>(kMaxMeshPoints));
  double length = 0.0;
  for (const std::vector<QPointF>& ring : rings) {
    for (size_t i = 0; i < ring.size(); ++i) {
      length += Distance(ring[i], ring[(i + 1) % ring.size()]);
    }
  }
  assert(std::isfinite(length));
  return length;
}

bool IsInsideOutline(const MeshOutline& outline, QPointF spot) {
  assert(std::isfinite(spot.x() + spot.y()));
  assert(outline.sides.size() <= static_cast<size_t>(kMaxDelaunayPoints));
  bool is_inside = false;
  for (const MeshEdge& side : outline.sides) {
    const QPointF a = outline.points[static_cast<size_t>(side.first)];
    const QPointF b = outline.points[static_cast<size_t>(side.second)];
    const bool is_spanning = (a.y() > spot.y()) != (b.y() > spot.y());
    if (!is_spanning) {
      continue;
    }
    const double x = a.x() + (spot.y() - a.y()) / (b.y() - a.y()) *
                                 (b.x() - a.x());
    const bool is_crossed = x > spot.x();
    if (is_crossed) {
      is_inside = !is_inside;
    }
  }
  return is_inside;
}

double FromOutline(const MeshOutline& outline, QPointF spot) {
  assert(std::isfinite(spot.x() + spot.y()));
  assert(outline.sides.size() <= static_cast<size_t>(kMaxDelaunayPoints));
  double nearest = 1e300;
  for (const MeshEdge& side : outline.sides) {
    nearest = std::min(
        nearest, FromSide(spot, outline.points[static_cast<size_t>(side.first)],
                          outline.points[static_cast<size_t>(side.second)]));
  }
  return nearest;
}

std::vector<QPointF> SpreadInside(const MeshOutline& outline,
                                  double spacing, double clearance,
                                  int most) {
  assert(spacing > 0.0 && clearance >= 0.0);
  assert(most >= 0);
  std::vector<QPointF> inside;
  const bool has_outline = !outline.points.empty();
  if (!has_outline) {
    return inside;
  }
  const QRectF box = BoundsOf(outline.points);
  const double rise = spacing * std::sqrt(3.0) / 2.0;
  const int rows = std::min(static_cast<int>(box.height() / rise) + 1,
                            kMaxLatticeSide);
  const int across = std::min(static_cast<int>(box.width() / spacing) + 2,
                              kMaxLatticeSide);
  // Centred on the box, so a shape gets the same spread wherever it is.
  const double top = box.center().y() - (rows - 1) * rise / 2.0;
  const double left = box.center().x() - (across - 1) * spacing / 2.0;
  for (int row = 0; row < rows; ++row) {
    const double shift = row % 2 == 0 ? 0.0 : spacing / 2.0;
    for (int column = 0; column < across; ++column) {
      const QPointF spot(left + column * spacing + shift, top + row * rise);
      const bool is_kept = IsInsideOutline(outline, spot) &&
                           FromOutline(outline, spot) >= clearance;
      if (is_kept) {
        inside.push_back(spot);
      }
      const bool is_too_many = static_cast<int>(inside.size()) > most;
      if (is_too_many) {
        return inside;
      }
    }
  }
  return inside;
}

}  // namespace snapper
