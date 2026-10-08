#ifndef SNAPPER_EDIT_RIG_MANAGER_H_
#define SNAPPER_EDIT_RIG_MANAGER_H_

#include <QPointF>
#include <QString>

#include <map>
#include <vector>

#include "base/error.h"
#include "model/doll.h"

namespace snapper {

class HistoryManager;

// How a project doll is jointed: parents, pivots, draw order, default
// drawings, warp grids and IK chains. Every change is one undo step,
// or part of a drag.
class RigManager final {
 public:
  explicit RigManager(HistoryManager* history);

  // Empty parent makes the piece a root.
  Result<void> SetParent(const QString& doll, const QString& piece,
                         const QString& parent);
  Result<void> SetPivot(const QString& doll, const QString& piece,
                        QPointF pivot);
  Result<void> SetOrder(const QString& doll, const QString& piece,
                        int order);
  // Degrees the piece is turned at rest; poses turn from there.
  Result<void> SetRestRotation(const QString& doll, const QString& piece,
                               double degrees);
  // -1 goes back to the drawing that was visible in Krita.
  Result<void> SetDefaultDrawing(const QString& doll, const QString& piece,
                                 int drawing);
  // 0 by 0 turns warping off. Warp keys made on the old grid no longer
  // fit, so they are cleared from the piece's poses.
  Result<void> SetWarpGrid(const QString& doll, const QString& piece,
                           WarpGrid grid);
  // How far pulling one grid point drags its neighbours, in cells (0
  // moves it alone), kept from 0 to kMaxWarpCells.
  Result<void> SetWarpReach(const QString& doll, const QString& piece,
                            int point, double reach);
  // Makes a warp grid point a drag node (with the default lag and
  // bounce), or a plain point again if it is one.
  Result<void> ToggleDragNode(const QString& doll, const QString& piece,
                              int point);
  // A drag node's lag and bounce, each kept 0 to 1.
  Result<void> SetDrag(const QString& doll, const QString& piece,
                       const DragNode& node);

  // Copies piece's joint, rest turn, parent and warp grid onto its
  // partner on the other side (arm_l to arm_r), mirrored, so a doll is
  // jointed once. With whole_side, every piece on that side goes over,
  // IK chains too.
  Result<void> CopyToOtherSide(const QString& doll, const QString& piece,
                               bool whole_side);
  // Why CopyToOtherSide can't act on piece; empty when it can.
  QString WhyNoCopy(const QString& doll, const QString& piece) const;

  // The same change to many pieces of a doll, as one undo step.
  // Numbers are added to each piece's own, so they keep differences.
  Result<void> ShiftRestAll(const QString& doll,
                            const std::vector<QString>& pieces,
                            double delta);
  Result<void> ShiftOrderAll(const QString& doll,
                             const std::vector<QString>& pieces, int delta);
  // Empty parent makes them all roots.
  Result<void> SetParentAll(const QString& doll,
                            const std::vector<QString>& pieces,
                            const QString& parent);
  Result<void> SetWarpAll(const QString& doll,
                          const std::vector<QString>& pieces, WarpGrid grid);
  Result<void> SetKeepShapeAll(const QString& doll,
                               const std::vector<QString>& pieces,
                               bool keeps_shape);
  // How the pieces' warp grids move by themselves (WarpMotionKind),
  // and the edge they hang from. Every piece needs a warp grid.
  Result<void> SetMotionKindAll(const QString& doll,
                                const std::vector<QString>& pieces,
                                WarpMotionKind kind);
  Result<void> SetMotionEdgeAll(const QString& doll,
                                const std::vector<QString>& pieces,
                                WarpEdge edge);
  // Adds to each piece's motion size (pixels) and cycle (frames), kept
  // in range.
  Result<void> ShiftMotionAll(const QString& doll,
                              const std::vector<QString>& pieces,
                              double size, int cycle);
  // Each piece's joint moves by its own offset (drawing pixels).
  Result<void> MovePivots(const QString& doll,
                          const std::map<QString, QPointF>& offsets);
  Result<void> CopyAllToOtherSide(const QString& doll,
                                  const std::vector<QString>& pieces);

  // upper must be lower's parent; the name must be new.
  Result<void> AddChain(const QString& doll, const IkChain& chain);
  Result<void> RemoveChain(const QString& doll, const QString& chain);
  Result<void> SetChainBend(const QString& doll, const QString& chain,
                            bool bends_clockwise);
  Result<void> SetChainTip(const QString& doll, const QString& chain,
                           QPointF tip);

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_RIG_MANAGER_H_
