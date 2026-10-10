#ifndef SNAPPER_ANIM_WARP_SHAPE_H_
#define SNAPPER_ANIM_WARP_SHAPE_H_

#include <QPointF>
#include <QSize>
#include <QSizeF>

#include <vector>

#include "model/doll.h"

namespace snapper {

// What a warp is made of, the same questions for an old grid and a
// shape mesh, so everything built on warp points (drawing, folding,
// cloth, collider pushes, mirroring) asks here and never cares which.

// The drawn sides between neighbouring points: on a grid each point to
// the one at its right and the one below, in that order, row by row;
// on a mesh the sides of its triangles.
std::vector<MeshEdge> WarpEdges(const WarpGrid& grid);

// The far corners of each two triangles of a mesh that share a side,
// so cloth can be held from creasing flat across it (a grid has none:
// its cells keep their shape by its own rules).
std::vector<MeshEdge> WarpBends(const WarpGrid& grid);

// The triangles the drawing is mapped through: on a grid each cell, row
// by row, as top-left, top-right, bottom-right and top-left,
// bottom-right, bottom-left; on a mesh its own.
std::vector<MeshTriangle> WarpTriangles(const WarpGrid& grid);

// How big one cell (the unit of rubber reach) is over a drawing of
// size, across and down; a mesh's is its average side both ways.
QSizeF CellSize(const WarpGrid& grid, QSize size);

// What values (one per point) make at spot (drawing pixels, at rest)
// when mixed across the warp the way the drawing is: on a grid by the
// cell spot is in, mixed in both directions; on a mesh by the triangle
// it is in. A spot off the warp goes with the nearest cell or triangle,
// carried on past it. Values the wrong size give a zero.
QPointF CarriedAt(const WarpGrid& grid, QSize size,
                  const std::vector<QPointF>& values, QPointF spot);

// For each point of from (over a drawing of from_size), the point of to
// (over to_size) that sits on its spot mirrored left to right across
// the drawing (the nearest within a cell of it), or -1 when there is
// none. Two equal grids pair exactly, as MirroredPoint does.
std::vector<int> MirroredPoints(const WarpGrid& from, QSize from_size,
                                const WarpGrid& to, QSize to_size);

}  // namespace snapper

#endif  // SNAPPER_ANIM_WARP_SHAPE_H_
