#ifndef SNAPPER_RENDER_WARP_RASTER_H_
#define SNAPPER_RENDER_WARP_RASTER_H_

#include <QImage>
#include <QPointF>

#include <vector>

#include "model/doll.h"

namespace snapper {

// A drawing bent by its warp grid, and where its top-left sits in the
// drawing's own pixel space (the warp can push it past the edges).
struct WarpedImage final {
  QImage image;
  QPointF origin;
};

// Bends source so each grid cell's rest corners land on points (one per
// grid point, row by row). Each cell is drawn as two triangles, sampled
// back into the source, so the result has no seams or holes.
WarpedImage WarpImage(const QImage& source, WarpGrid grid,
                      const std::vector<QPointF>& points);

}  // namespace snapper

#endif  // SNAPPER_RENDER_WARP_RASTER_H_
