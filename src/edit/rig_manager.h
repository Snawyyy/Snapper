#ifndef SNAPPER_EDIT_RIG_MANAGER_H_
#define SNAPPER_EDIT_RIG_MANAGER_H_

#include <QPointF>
#include <QString>

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

  // Copies piece's joint, rest turn, parent and warp grid onto its
  // partner on the other side (arm_l to arm_r), mirrored, so a doll is
  // jointed once. With whole_side, every piece on that side goes over,
  // IK chains too.
  Result<void> CopyToOtherSide(const QString& doll, const QString& piece,
                               bool whole_side);
  // Why CopyToOtherSide can't act on piece; empty when it can.
  QString WhyNoCopy(const QString& doll, const QString& piece) const;

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
