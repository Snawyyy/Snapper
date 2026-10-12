#ifndef SNAPPER_UI_RIG_CANVAS_H_
#define SNAPPER_UI_RIG_CANVAS_H_

#include <QString>
#include <QStringList>
#include <QTransform>
#include <QWidget>

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "anim/joint_snap.h"
#include "edit/edit_scope.h"
#include "edit/selection_manager.h"
#include "render/image_cache.h"
#include "ui/drag_feedback.h"
#include "ui/grab.h"
#include "ui/handle_hover.h"
#include "ui/managers.h"

namespace snapper {

// A doll at rest, for jointing it. Click a piece to pick it; drag its
// yellow joint to move the pivot; Shift adds pieces, Ctrl flips them,
// a box on empty space picks what it touches, and joints of all picked
// pieces drag together; double-click a joint, then click the part the
// picked pieces should hang from; drag a square IK tip to set where the chain
// reaches from. Lines join each joint to its parent's. A press grabs the
// handle nearest to it within kGrabReach; nothing moves until the mouse
// has gone kDragDeadZone, then the handle jumps under the cursor. Before
// any press the handle under the cursor grows, shows its cursor, and a
// thin dashed outline marks the piece a click would pick. A drag shows
// a chip with how far it went, a faint ghost where the handle was, and
// pops on letting go. Escape cancels a drag or a hang. A lone joint
// dragged catches on its drawn limb (the middle of its width, an end's
// circle center, where it crosses its parent) with a red line across the
// limb, or on its drawing's box; Ctrl drags it freely.
class RigCanvas final : public QWidget {
  Q_OBJECT

 public:
  explicit RigCanvas(const Managers& managers);

  void SetDoll(const QString& doll);
  // Shows the doll wearing skin (empty: none), while it is being changed.
  void SetSkin(const QString& skin);
  void SetPiece(const QString& piece);
  // Picks pieces with focus shown in the panel, as the panel's list does.
  void SetPick(const QStringList& picked, const QString& focus);
  // Doll space to widget pixels.
  QTransform World() const;
  // Makes piece a collider from its drawing (as right-click > Make
  // collider does) and picks it, so its dots show.
  void MakeCollider(const QString& piece);
  // Makes piece a plain piece again.
  void RemoveCollider(const QString& piece);
  // Mirrors piece's drag and physics dots and collider onto its twin.
  void MirrorToOtherSide(const QString& piece);
  // Deletes the picked collider dots (Delete does); one undo step.
  void DeletePickedDots();
  // The picked collider dots, as "piece:slot".
  const std::set<QString>& picked_dots() const { return picked_dots_; }
  // The chip, ghost and pop of the drag, for tests to look at.
  const DragFeedback& feedback() const { return feedback_; }

 signals:
  // The picked pieces and which one the panel shows.
  void PickChanged(const QStringList& picked, const QString& focus);
  // What the canvas is waiting for; empty when nothing.
  void Hint(const QString& hint);
  void Problem(const QString& why);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  // The wheel zooms toward the cursor (so does a touchpad pinch), the
  // middle button slides the view, Shift+1 fits the doll again.
  void wheelEvent(QWheelEvent* event) override;
  bool event(QEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;

 private:
  enum class Drag { kNone, kPivot, kTip };

  // A joint or IK tip: what it is, its name, and where it is drawn.
  struct Handle final {
    GrabKind kind = GrabKind::kNone;
    QString key;
    QPointF spot;
  };

  void PaintOverlay(QPainter* painter) const;
  // One joint: its bone to the parent, the piece outline, the dot.
  void PaintJoint(QPainter* painter, const Doll& doll,
                  const std::map<QString, QTransform>& pieces,
                  const RigPiece& rig) const;
  // One IK tip square.
  void PaintTip(QPainter* painter,
                const std::map<QString, QTransform>& pieces,
                const IkChain& chain) const;
  // As a joint or tip is grabbed: leaves a faint copy of what drags,
  // drawn by PaintJoint or PaintTip, where it sits now.
  void GrabGhost();
  bool PressHandle(QPointF point, Qt::KeyboardModifiers modifiers);
  // One piece's collider outline, in the rig colour for colliders.
  void PaintCollider(QPainter* painter,
                     const std::map<QString, QTransform>& pieces,
                     const RigPiece& rig) const;
  // Picks or flips a collider dot by its key.
  void PickDot(const QString& key, PickMode mode);
  // Forgets picked dots of pieces that are no longer picked.
  void PruneDots();
  // Every joint and IK tip, in widget pixels.
  std::vector<Handle> HandlesOf() const;
  // The handle nearest to point within reach.
  std::optional<Handle> HandleAt(QPointF point) const;
  // The piece whose joint is nearest to point within reach, or empty.
  QString JointAt(QPointF point) const;
  // Follows the cursor to the handle or piece under it.
  void UpdateHover(QPointF point);
  void ClearHover();
  // Ends hang mode, telling the window.
  void StopHanging();
  // Hangs the picked pieces from the piece under at (a root when there
  // is none); ends hang mode.
  void FinishHang(QPointF at);
  // As a pivot is grabbed: notes where the cursor and the pivot are, and
  // opens the one undo step of the drag.
  void GrabPivot(const QString& joint, QPointF point);
  // The cursor moved with pivots grabbed: moves each by its step (a lone
  // pivot catches on its drawing, unless is_free: Ctrl held).
  void MovePivots(const Doll& doll, QPointF cursor, bool is_free);
  // Alt on a joint: pulls a rope from it; letting go over a piece hangs
  // the joint's piece from it.
  void StartRope(const QString& joint, QPointF at);
  // wanted (a pivot in piece's drawing pixels) caught on the drawn
  // limb (width middle, end center, overlap with its parent) or the
  // middle and edges of the drawing within a few screen pixels
  // (SnapPivot, anim/joint_snap.h).
  PivotSnap SnappedPivot(const Doll& doll, const QString& piece,
                         QPointF wanted);
  // The shape snap caught by the pivot drag: the line across the limb,
  // its middle and an end's circle.
  void PaintShapeSnap(QPainter* painter,
                      const std::map<QString, QTransform>& pieces) const;
  // Right click > Snap to shape or Snap all joints: moves the joints
  // onto their drawn limbs (RigManager::SnapJointsToShape).
  void SnapJoints(const std::vector<QString>& pieces);
  // J or the right click entry: the focused joint's circle on or off.
  void ToggleJointCircle(const QString& piece);
  // A palette pick while the canvas shows and the focused joint has its
  // circle on colors that circle.
  void ColorJointCircle(const QColor& color);
  void ApplyPick(const std::set<QString>& pieces, PickMode mode,
                 const QString& focus);
  void FinishBox();
  // Shows what the joint or tip drag says: chip, ghost and snap flash.
  void ShowDrag(QPointF cursor);
  // The chip's name for a shape snap ("Width middle").
  QString SnapName(ShapeSnapKind kind) const;

  Managers managers_;
  HandleHover hover_;
  DragFeedback feedback_;
  // The piece a click would pick, outlined thinly; empty when none.
  QString hover_piece_;
  ImageCache cache_;
  QString doll_;
  QString skin_;
  QString piece_;
  std::set<QString> picked_;
  std::set<QString> picked_dots_;
  QPointF last_;
  // Where the dragged handle sat when it was grabbed.
  QPointF start_;
  // Where the mouse went down; a drag waits until it leaves the dead
  // zone around it.
  QPointF press_;
  bool is_started_ = false;
  bool is_boxing_ = false;
  QPointF box_from_;
  QPointF box_to_;
  PickMode box_mode_ = PickMode::kReplace;
  Drag drag_ = Drag::kNone;
  // Set after a joint is double-clicked: the next click picks its
  // parent. The line follows the cursor meanwhile.
  bool is_hanging_ = false;
  // The hang was started by a rope drag: it ends on letting go.
  bool is_rope_ = false;
  // A single pivot drag: where the cursor and the pivot were when it was
  // grabbed, in the piece's drawing pixels.
  QPointF grab_cursor_;
  QPointF grab_pivot_;
  QPointF cursor_;
  // Zooms by factor toward cursor (widget pixels).
  void ZoomAt(QPointF cursor, double factor);
  // The middle button: true when event started, moved or ended a slide
  // of the view and is dealt with.
  bool PanPress(QMouseEvent* event);
  bool PanMove(QMouseEvent* event);
  bool PanRelease(QMouseEvent* event);
  // How the view is zoomed (1 fits the doll) and slid (widget pixels)
  // on top of fitting the doll.
  double zoom_ = 1.0;
  QPointF pan_;
  bool is_panning_ = false;
  QPointF pan_from_;
  // The drawings measured for joint snap.
  LimbMemo limbs_;
  // What the lone pivot drag caught on its piece's shape, if anything.
  std::optional<ShapeSnap> shape_snap_;
  QString snap_piece_;
  QString chain_;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_CANVAS_H_
