#ifndef SNAPPER_RENDER_STAGE_GEOMETRY_H_
#define SNAPPER_RENDER_STAGE_GEOMETRY_H_

#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QTransform>

#include <optional>
#include <vector>

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where things sit on a frame rendered at scale, for tools that draw
// handles and turn mouse moves into poses. "Screen" is output pixels.

// A layer's space to screen.
std::optional<QTransform> LayerToScreen(const Project& project,
                                        const Shot& shot, LayerId layer,
                                        Frame local, double scale);

// The space a piece's offset moves in (its parent's, or the layer's for
// a root piece) to screen. Mouse moves are mapped back through it so a
// piece follows the cursor.
std::optional<QTransform> PieceParentToScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale);

// A piece's drawing outline and joint on screen.
struct PieceOutline final {
  QPolygonF outline;
  QPointF pivot;
};
std::optional<PieceOutline> PieceOnScreen(const Project& project,
                                          const Shot& shot, LayerId layer,
                                          const QString& piece, Frame local,
                                          double scale);

// A piece's warp grid on screen: its points row by row (as shown,
// pushed by the pose's warp, in drawing pixels), the grid's size, each
// point's rubber reach in cells, its drag nodes and moving points, the
// drawing's size, and drawing pixels to screen. Nothing when the piece
// has no grid.
struct WarpOnScreen final {
  WarpGrid grid;
  std::vector<QPointF> points;
  std::vector<double> reach;
  // Points that are drag nodes, and points that move by themselves.
  std::vector<int> drags;
  std::vector<int> moves;
  QSize size;
  QTransform to_screen;
};
std::optional<WarpOnScreen> PieceWarpOnScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale);

// The draggable tip of each IK chain of a doll layer.
struct IkHandle final {
  QString chain;
  QPointF point;
};
std::vector<IkHandle> IkHandles(const Project& project, const Shot& shot,
                                LayerId layer, Frame local, double scale);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_GEOMETRY_H_
