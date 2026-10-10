#ifndef SNAPPER_ANIM_MESH_OUTLINE_H_
#define SNAPPER_ANIM_MESH_OUTLINE_H_

#include <QPointF>

#include <vector>

#include "model/warp_mesh.h"

namespace snapper {

// The outlines a shape mesh hugs: dots along every ring (outsides and
// holes) and the sides joining them, which the mesh keeps as its own
// sides.
struct MeshOutline final {
  std::vector<QPointF> points;
  std::vector<MeshEdge> sides;
};

// rings (closed, already straightened) as an outline with no side
// longer than spacing: long sides get evenly spaced dots. Rings of
// fewer than three corners are left out; a corner on the spot of an
// earlier dot is that dot.
MeshOutline OutlineOf(const std::vector<std::vector<QPointF>>& rings,
                      double spacing);

// The area inside rings and the length of their outlines, holes
// taken out of the area.
double AreaOf(const std::vector<std::vector<QPointF>>& rings);
double LengthOf(const std::vector<std::vector<QPointF>>& rings);

// Whether spot is inside outline (inside an outside ring and not in a
// hole), crossing count odd.
bool IsInsideOutline(const MeshOutline& outline, QPointF spot);

// How far spot is from the nearest side of outline.
double FromOutline(const MeshOutline& outline, QPointF spot);

// Dots spread evenly inside outline on rows of a triangle lattice
// spacing apart, each at least clearance from every side, the first
// rows first. It stops at most + 1, which tells a caller there are too
// many.
std::vector<QPointF> SpreadInside(const MeshOutline& outline,
                                  double spacing, double clearance,
                                  int most);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MESH_OUTLINE_H_
