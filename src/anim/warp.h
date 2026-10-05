#ifndef SNAPPER_ANIM_WARP_H_
#define SNAPPER_ANIM_WARP_H_

#include <QPointF>
#include <QSize>

#include <vector>

#include "model/doll.h"

namespace snapper {

// The grid's points at rest over a drawing of size, row by row.
std::vector<QPointF> RestPoints(QSize size, WarpGrid grid);

// Rest points pushed by offsets (one per point). Wrong-sized offsets
// leave the grid at rest.
std::vector<QPointF> WarpedPoints(QSize size, WarpGrid grid,
                                  const std::vector<QPointF>& offsets);

// Index of the grid point closest to spot (drawing pixels), or -1 when
// none is within reach.
int NearestPoint(const std::vector<QPointF>& points, QPointF spot,
                 double reach);

}  // namespace snapper

#endif  // SNAPPER_ANIM_WARP_H_
