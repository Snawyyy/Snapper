#ifndef SNAPPER_ANIM_DELAUNAY_H_
#define SNAPPER_ANIM_DELAUNAY_H_

#include <QPointF>
#include <QRectF>

#include <vector>

#include "model/warp_mesh.h"

namespace snapper {

// The most points Delaunay takes.
inline constexpr int kMaxDelaunayPoints = 4 * kMaxMeshPoints;

// Joins points (at most kMaxDelaunayPoints, no two on the same spot)
// into triangles that cover the space around them with no triangle's
// circle holding another point, so the triangles are as fat as the
// points allow. Each triangle runs clockwise on a screen (y down).
// Points added one at a time in their order, so the same points give
// the same triangles. Empty with fewer than three points, or when the
// points all lie on one line.
std::vector<MeshTriangle> Delaunay(const std::vector<QPointF>& points);

// The smallest box holding points (not empty).
QRectF BoundsOf(const std::vector<QPointF>& points);

// Whether the triangles never overlap and none is turned over or flat:
// what Delaunay gives, checked.
bool IsTiled(const std::vector<QPointF>& points,
             const std::vector<MeshTriangle>& triangles);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DELAUNAY_H_
