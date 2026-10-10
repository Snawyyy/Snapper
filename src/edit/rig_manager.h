#ifndef SNAPPER_EDIT_RIG_MANAGER_H_
#define SNAPPER_EDIT_RIG_MANAGER_H_

#include <QImage>
#include <QPointF>
#include <QString>

#include <map>
#include <vector>

#include "anim/mesh_carry.h"
#include "base/error.h"
#include "model/doll.h"

namespace snapper {

class HistoryManager;

// A warp dot of a doll's piece, by the doll's name.
struct DotRef final {
  QString doll;
  QString piece;
  int point = -1;

  bool operator==(const DotRef&) const = default;
};

// The numbers of a drag node (kBounce is the drag's, kSpring the
// physics one's; the panel calls both Bounce).
enum class DragField {
  kLag,
  kBounce,
  kStretch,
  kInertia,
  kAir,
  kGravity,
  kSpring
};

// What a point motion is made of; kKind takes a WarpMotionKind number.
enum class MotionField { kKind, kSize, kCycle, kAngle, kDelay };

// How a project doll is jointed: parents, pivots, draw order, default
// drawings, warp grids, drag nodes, point motions and IK chains. Every
// change is one undo step, or part of a drag.
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
                           const WarpGrid& grid);
  // Gives every piece a shape mesh made from its drawings at detail:
  // turning its warp on, a new detail, or an old grid converted. Each
  // dot's settings carry to the nearest new dot and every warp key
  // carries by position; one undo step. Refused when a piece's
  // drawings can't be read or show nothing. What carried over comes
  // back; nothing changes when the mesh comes out the same.
  Result<CarryCount> SetShapeMeshAll(const QString& doll,
                                     const std::vector<QString>& pieces,
                                     MeshDetail detail);
  // What to tell the user after SetShapeMeshAll.
  static QString MeshDoneText(const CarryCount& count);
  // Why piece's mesh detail can't be set (no warp, or an old grid);
  // empty when it can.
  QString WhyNoDetail(const QString& doll, const QString& piece) const;
  // Why piece can't be converted to a shape mesh (only an old grid
  // can); empty when it can.
  QString WhyNoConvert(const QString& doll, const QString& piece) const;
  // How far pulling one grid point drags its neighbours, in cells (0
  // moves it alone), kept from 0 to kMaxWarpCells.
  Result<void> SetWarpReach(const QString& doll, const QString& piece,
                            int point, double reach);
  // The edits below take many dots at once (a single one is a list of
  // one) and are one undo step each, named with how many dots they
  // touched ("Set bounce on 6 dots").
  // The name of the step SetDragField makes, for a field that opens it
  // before it commits.
  static QString FieldLabel(DragField field, size_t dots);
  // Sets one number on every drag node among dots, kept 0 to 1; dots
  // that are not drag nodes are left alone. Refused when none is one.
  Result<void> SetDragField(const std::vector<DotRef>& dots, DragField field,
                            double value);
  // The same, but math is typed text worked out on each dot's own
  // number: "*2" doubles each, "+0.1" adds to each, "0.5" sets all.
  Result<void> SetDragFieldMath(const std::vector<DotRef>& dots,
                                DragField field, const QString& math);
  // Makes every dot a drag node (with the default lag and bounce), or a
  // plain point again.
  Result<void> SetDragNodes(const std::vector<DotRef>& dots, bool on);
  // Switches every drag node among dots to physics (cloth, soft flesh),
  // or back to plain drag; its settings stay.
  Result<void> SetPhysicsAll(const std::vector<DotRef>& dots, bool on);
  // Takes away the drag or physics node and the motion of every dot.
  Result<void> ClearDots(const std::vector<DotRef>& dots);
  // The name of the step SetMotionAll makes.
  static QString MotionLabel(MotionField field, size_t dots);
  // Sets one part of the motion of every dot, starting one where there
  // is none; kind None takes a motion away.
  Result<void> SetMotionAll(const std::vector<DotRef>& dots,
                            MotionField field, double value);
  // The same for a motion number (not its kind), the math worked out on
  // each dot's own number.
  Result<void> SetMotionMath(const std::vector<DotRef>& dots,
                             MotionField field, const QString& math);
  // Makes piece a collider: the outline of the visible pixels of
  // drawing (its default drawing, as an image) as a ring of dots that
  // pushes the warp dots of the pieces in front of it. Refused when
  // nothing is visible. Replaces an outline it had.
  Result<void> MakeCollider(const QString& doll, const QString& piece,
                            const QImage& drawing);
  // Takes the dots on these ring slots (gone) out of piece's collider;
  // the gaps push nothing. Refused when none of them is there.
  Result<void> DeleteColliderDots(const QString& doll, const QString& piece,
                                  const std::vector<int>& gone);
  // Makes piece a plain piece again.
  Result<void> RemoveCollider(const QString& doll, const QString& piece);
  // Whether piece's collider pushes the warp dots of other (it does
  // until spared).
  Result<void> SetColliderPushes(const QString& doll, const QString& piece,
                                 const QString& other, bool pushes);
  // Why MirrorNodes can't act on piece; empty when it can.
  QString WhyNoMirror(const QString& doll, const QString& piece) const;
  // Copies piece's drag and physics nodes, point motions and its
  // collider (and which pieces it pushes) onto its left or right twin,
  // mirrored, replacing what the twin had there: one undo step. Needs
  // the same warp grid on both.
  Result<void> MirrorNodes(const QString& doll, const QString& piece);
  // Copies just these dots' drag or physics nodes and motions to the
  // mirrored dots of their pieces' twins (a dot with none clears the
  // twin's).
  Result<void> MirrorDots(const std::vector<DotRef>& dots);
  // Gives a warp grid point its motion, or changes it; kind kNone stops
  // it. Size, cycle and delay are kept in range.
  Result<void> SetPointMotion(const QString& doll, const QString& piece,
                              const PointMotion& motion);

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
                          const std::vector<QString>& pieces,
                          const WarpGrid& grid);
  Result<void> SetKeepShapeAll(const QString& doll,
                               const std::vector<QString>& pieces,
                               bool keeps_shape);
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
  const RigPiece* RigOf(const QString& doll, const QString& piece) const;

  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_RIG_MANAGER_H_
