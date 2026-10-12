#ifndef SNAPPER_RENDER_STAGE_GEOMETRY_H_
#define SNAPPER_RENDER_STAGE_GEOMETRY_H_

#include <QPointF>
#include <QPolygonF>
#include <QSize>
#include <QString>
#include <QTransform>

#include <optional>
#include <vector>

#include "anim/pose_memo.h"
#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where things sit on a frame rendered at scale, for tools that draw
// handles and turn mouse moves into poses. "Screen" is output pixels.

// A layer's space to screen.
std::optional<QTransform> LayerToScreen(const Project& project,
                                        const Shot& shot, LayerId layer,
                                        Frame local, double scale,
                                        PoseMemo* memo = nullptr);
// The space a layer's own move is keyed in, to screen: the shot's, as
// moved by the hold on the layer, so a held object drags under the
// cursor whichever way its holder turned it.
QTransform LayerParentToScreen(const Project& project, const Shot& shot,
                               const Layer& layer, Frame local, double scale,
                               PoseMemo* memo = nullptr);

// The space a piece's offset moves in (its parent's, or the layer's for
// a root piece) to screen. Mouse moves are mapped back through it so a
// piece follows the cursor.
std::optional<QTransform> PieceParentToScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale,
                                              PoseMemo* memo = nullptr);

// A piece's drawing outline and joint on screen.
struct PieceOutline final {
  QPolygonF outline;
  QPointF pivot;
};
std::optional<PieceOutline> PieceOnScreen(const Project& project,
                                          const Shot& shot, LayerId layer,
                                          const QString& piece, Frame local,
                                          double scale,
                                          PoseMemo* memo = nullptr);

// A piece's warp grid on screen: its points row by row as the drawing
// shows them at the frame (keys, drag and physics nodes, point motions
// and collider pushes all in, in drawing pixels), where the keys alone
// put them, the grid's size, each point's rubber reach in cells, its
// drag nodes and moving points, the drawing's size, and drawing pixels
// to screen. Nothing when the piece has no grid.
struct WarpOnScreen final {
  WarpGrid grid;
  std::vector<QPointF> points;
  // Where the keys put each point; dragging a dot moves this.
  std::vector<QPointF> keyed;
  std::vector<double> reach;
  // Each point's drag node kind; nothing for a dot that stays still.
  std::vector<std::optional<NodeKind>> kinds;
  // Points that move by themselves.
  std::vector<int> moves;
  QSize size;
  QTransform to_screen;
};
std::optional<WarpOnScreen> PieceWarpOnScreen(const Project& project,
                                              const Shot& shot, LayerId layer,
                                              const QString& piece,
                                              Frame local, double scale,
                                              PoseMemo* memo = nullptr);

// The draggable tip of each IK chain of a doll layer.
struct IkHandle final {
  QString chain;
  QPointF point;
};
std::vector<IkHandle> IkHandles(const Project& project, const Shot& shot,
                                LayerId layer, Frame local, double scale,
                                PoseMemo* memo = nullptr);

}  // namespace snapper

#endif  // SNAPPER_RENDER_STAGE_GEOMETRY_H_
