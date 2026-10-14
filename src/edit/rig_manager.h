#ifndef SNAPPER_EDIT_RIG_MANAGER_H_
#define SNAPPER_EDIT_RIG_MANAGER_H_

#include <QColor>
#include <QImage>
#include <QPointF>
#include <QString>

#include <map>
#include <vector>

#include "anim/mesh_carry.h"
#include "anim/node_feel.h"
#include "base/error.h"
#include "model/doll.h"

namespace snapper {

class HistoryManager;

// A warp dot of a doll's piece, by the doll's name. With a layer, edits
// to its drag node, motion or reach change that layer's own copy (one
// shot's doll), leaving every other layer alone; with none they change
// the doll's default, which new layers start with.
struct DotRef final {
  QString doll;
  QString piece;
  int point = -1;
  LayerId layer = LayerId();

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
  // The name of the step SetWarpReach makes ("Set reach on 6 dots").
  static QString ReachLabel(size_t dots);
  // How far pulling each of dots drags its neighbours, in cells (0
  // moves it alone), kept from 0 to kMaxWarpCells: one undo step.
  Result<void> SetWarpReach(const std::vector<DotRef>& dots, double reach);
  // The same, but math is typed text worked out on each dot's own
  // reach: "+0.5" adds to each, "*2" doubles each, "1" sets all.
  Result<void> SetWarpReachMath(const std::vector<DotRef>& dots,
                                const QString& math);
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
  // Makes every dot a node of kind (drag, cloth or jelly), adding the
  // node where a dot had none; settings a node had stay.
  Result<void> SetNodeKindAll(const std::vector<DotRef>& dots, NodeKind kind);
  // The name of the step SetFeelAll makes ("Set feel on 6 dots").
  static QString FeelLabel(size_t dots);
  // Sets every moving dot among dots to feel (see WithFeel), each by its
  // own kind; plain dots are left alone.
  Result<void> SetFeelAll(const std::vector<DotRef>& dots, const Feel& feel);
  // Whether layer has changed any warp dot settings of doll for itself.
  bool HasOwnDots(const QString& doll, LayerId layer) const;
  // Makes the warp dot settings layer shows (reach, drag nodes, motions)
  // the doll's default, which every new layer starts with. Every other
  // layer of the doll keeps the look it has now. Refused when layer has
  // no settings of its own.
  Result<void> SetDotsDefault(const QString& doll, LayerId layer);
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
  // Colliders belong to one drawing of a piece, named by drawing_name
  // (see DrawingName); a drawing without its own uses the piece's
  // (RigPiece::collider), and an empty drawing_name edits that one.
  // Each edit is one undo step.
  //
  // Makes piece's drawing a collider: the outline of the visible pixels
  // of drawing (that drawing, as an image) as a ring of dots that
  // pushes the warp dots of the other pieces. Refused when nothing is
  // visible. Replaces an outline it had.
  Result<void> MakeCollider(const QString& doll, const QString& piece,
                            const QImage& drawing,
                            const QString& drawing_name = QString());
  // Takes the dots on these ring slots (gone) out of the collider of
  // piece's drawing; the gaps push nothing. Refused when none of them is
  // there.
  Result<void> DeleteColliderDots(const QString& doll, const QString& piece,
                                  const std::vector<int>& gone,
                                  const QString& drawing_name = QString());
  // Leaves piece's drawing without a collider.
  Result<void> RemoveCollider(const QString& doll, const QString& piece,
                              const QString& drawing_name = QString());
  // Whether the collider of piece's drawing pushes the warp dots of
  // other (it does until spared).
  Result<void> SetColliderPushes(const QString& doll, const QString& piece,
                                 const QString& other, bool pushes,
                                 const QString& drawing_name = QString());
  // Why MirrorNodes can't act on piece; empty when it can.
  QString WhyNoMirror(const QString& doll, const QString& piece) const;
  // Copies piece's drag and physics nodes, point motions and its
  // colliders, per drawing too (and which pieces they push; a drawing's
  // goes to the twin's drawing of the same name with the side word
  // swapped, or of the same name), onto its left or right twin,
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
  // Makes pieces draw Crisp whatever the doll's look, or follow it
  // again; one undo step.
  Result<void> SetStaysCrispAll(const QString& doll,
                                const std::vector<QString>& pieces,
                                bool stays_crisp);
  // How doll's drawings look when moved (Crisp, Smooth, Smooth flat),
  // and whether smooth edges cut out; one undo step each.
  Result<void> SetLook(const QString& doll, DrawingLook look);
  Result<void> SetHardEdges(const QString& doll, bool is_hard);
  // Each piece's joint moves by its own offset (drawing pixels).
  Result<void> MovePivots(const QString& doll,
                          const std::map<QString, QPointF>& offsets);
  Result<void> CopyAllToOtherSide(const QString& doll,
                                  const std::vector<QString>& pieces);
  // Moves each piece's joint onto its drawing's shape (NearestShapeSnap,
  // anim/joint_snap.h: the end or overlap center whose circle it is in,
  // else the middle of the limb's width through it), as one undo step.
  // drawings holds each piece's drawing at rest, by piece. Says which
  // pieces moved; refused when none did.
  Result<std::vector<QString>> SnapJointsToShape(
      const QString& doll, const std::vector<QString>& pieces,
      const std::map<QString, QImage>& drawings);
  // Turns piece's joint circle on or off, one undo step. Turned on for
  // the first time it takes drawing's (the piece's drawing at rest) most
  // common color at the joint (JointColorAt).
  Result<void> ToggleJointCircle(const QString& doll, const QString& piece,
                                 const QImage& drawing);
  // Gives piece's joint circle color (a palette pick); one undo step. Refused while the circle is off.
  Result<void> SetJointCircleColor(const QString& doll, const QString& piece,
                                   const QColor& color);

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

// What a look is called on its tile and in undo ("Smooth flat").
QString LookName(DrawingLook look);

}  // namespace snapper

#endif  // SNAPPER_EDIT_RIG_MANAGER_H_
