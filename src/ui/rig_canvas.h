#ifndef SNAPPER_UI_RIG_CANVAS_H_
#define SNAPPER_UI_RIG_CANVAS_H_

#include <QString>
#include <QStringList>
#include <QTransform>
#include <QWidget>

#include <memory>
#include <optional>
#include <set>
#include <vector>

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
// pops on letting go. Escape cancels a drag or a hang.
class RigCanvas final : public QWidget {
  Q_OBJECT

 public:
  explicit RigCanvas(const Managers& managers);

  void SetDoll(const QString& doll);
  void SetPiece(const QString& piece);
  // Picks pieces with focus shown in the panel, as the panel's list does.
  void SetPick(const QStringList& picked, const QString& focus);
  // Doll space to widget pixels.
  QTransform World() const;
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
  void leaveEvent(QEvent* event) override;

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
  bool PressHandle(QPointF point);
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
  void ApplyPick(const std::set<QString>& pieces, PickMode mode,
                 const QString& focus);
  void FinishBox();
  // Shows what the joint or tip drag says: chip, ghost and snap flash.
  void ShowDrag(QPointF cursor);

  Managers managers_;
  HandleHover hover_;
  DragFeedback feedback_;
  // The piece a click would pick, outlined thinly; empty when none.
  QString hover_piece_;
  ImageCache cache_;
  QString doll_;
  QString piece_;
  std::set<QString> picked_;
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
  QPointF cursor_;
  QString chain_;
  std::unique_ptr<EditScope> scope_;
};

}  // namespace snapper

#endif  // SNAPPER_UI_RIG_CANVAS_H_
