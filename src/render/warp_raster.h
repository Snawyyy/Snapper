#ifndef SNAPPER_RENDER_WARP_RASTER_H_
#define SNAPPER_RENDER_WARP_RASTER_H_

#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QSize>
#include <QPointF>

#include <vector>

#include "model/doll.h"

namespace snapper {

// A drawing bent by its warp grid, and where its top-left sits in the
// drawing's own pixel space (the warp can push it past the edges).
struct WarpedImage final {
  QImage image;
  QPointF origin;
  // Pixels of image per pixel of the drawing: 1 unless it was bent
  // small to save time (see WarpCache). origin is in drawing pixels.
  double detail = 1.0;
};

// How a bend reads its source (see DrawingLook). The default reads the
// nearest pixel (Crisp).
struct WarpLook final {
  // Blends the four pixels around each spot instead of picking one.
  bool is_blended = false;
  // Fully covered pixels take the nearest pixel, so fills stay the
  // drawing's own colors and only the edges soften.
  bool is_flat = false;
  // Edge pixels are kept or dropped whole instead of blended.
  bool is_hard = false;
  // Where the piece's frame lies in the source, as shares of its width
  // and height: the whole of it for a drawing the frame's size; less
  // for one that reaches past its frame (OwnDrawing::offset), whose
  // overhang bends along with the grid's edge cells. Shares, so a
  // shrunk or upscaled source keeps the same frame.
  QRectF frame = QRectF(0.0, 0.0, 1.0, 1.0);

  bool operator==(const WarpLook&) const = default;
};

// The frame's place in a drawing of image_size whose frame of
// frame_size starts at offset (WarpLook::frame).
QRectF FrameShare(QPoint offset, QSize frame_size, QSize image_size);

// The bend look for a doll's look and edges.
WarpLook WarpLookFor(DrawingLook look, bool hard_edges);

// Bends source so each grid cell's rest corners land on points (one per
// grid point, row by row). Each cell is drawn as two triangles (a shape
// mesh as its own triangles), sampled back into the source, so the
// result has no seams or holes; nothing is drawn outside a mesh. The
// rest corners are measured on source, so an upscaled source bends to
// the same points.
WarpedImage WarpImage(const QImage& source, const WarpGrid& grid,
                      const std::vector<QPointF>& points,
                      const WarpLook& look = {});

}  // namespace snapper

#endif  // SNAPPER_RENDER_WARP_RASTER_H_
