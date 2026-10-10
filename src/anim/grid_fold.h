#ifndef SNAPPER_ANIM_GRID_FOLD_H_
#define SNAPPER_ANIM_GRID_FOLD_H_

#include <QPointF>

#include <vector>

#include "model/doll.h"

namespace snapper {

// Keeps a warp grid from folding over. points (one per grid point, any
// space) were moved from from; around every cell (or mesh triangle)
// whose drawn triangles have turned over, or squashed to almost
// nothing, against how they were in from, the points are taken part of
// the way back to from, again and again (a fixed number of times)
// until no cell is folded. A grid moved
// further than it can go without tearing the drawing gives way
// instead. Points that did not move stay put.
void Unfold(const WarpGrid& grid, const std::vector<QPointF>& from,
            std::vector<QPointF>* points);

}  // namespace snapper

#endif  // SNAPPER_ANIM_GRID_FOLD_H_
