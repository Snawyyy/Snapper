#ifndef SNAPPER_ANIM_JELLY_H_
#define SNAPPER_ANIM_JELLY_H_

#include <QPointF>

#include <vector>

#include "anim/cloth.h"
#include "model/doll.h"

namespace snapper {

// The area points make on grid's triangles (cells or mesh triangles),
// in square pixels of the space points are in.
double ShapeArea(const WarpGrid& grid, const std::vector<QPointF>& points);

// Runs one frame of the jelly of piece: its nodes wobble as one soft
// blob. Each frame the shape the body carries (every point of the grid,
// the nodes where they are) is fitted with one smooth stretch, squash
// and turn of itself, scaled so its area is the drawn area; each node
// is pulled toward its place on that fit by its bounce. So the blob
// never goes jagged, and squeezed one way (by a collider or a key) it
// bulges the other. Squish (the stretch setting) is how much of the
// squash and stretch the fit keeps: 0 only turns, 1 squishes freely.
// Inertia, air, gravity and colliders work as for cloth.
// Deterministic: the same input gives the same output.
void StepJelly(const std::vector<QPointF>& carried, ClothPiece* piece);

}  // namespace snapper

#endif  // SNAPPER_ANIM_JELLY_H_
