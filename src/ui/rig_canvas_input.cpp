// RigCanvas's mouse and keys.

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainterPath>

#include <cassert>
#include <map>

#include "anim/doll_pose.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "render/stage_hit.h"
#include "ui/drag_text.h"
#include "ui/grab.h"
#include "ui/pick_keys.h"
#include "ui/problem.h"
#include "ui/rig_canvas.h"

namespace snapper {
namespace {

// point mapped into a piece's own drawing pixels, at rest.
std::optional<QPointF> IntoPiece(const Doll& doll, const QString& piece,
                                 const QTransform& world, QPointF point) {
  assert(!piece.isEmpty());
  assert(std::isfinite(point.x()));
  const auto pieces = PieceTransforms(doll, PoseMap());
  const auto found = pieces.find(piece);
  bool is_invertible = false;
  const QTransform back = found == pieces.end()
                              ? QTransform()
                              : (found->second * world).inverted(
                                    &is_invertible);
  return is_invertible ? std::optional<QPointF>(back.map(point))
                       : std::nullopt;
}

}  // namespace

void RigCanvas::GrabPivot(const QString& joint, QPointF point) {
  assert(!joint.isEmpty());
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  drag_ = Drag::kPivot;
  const Doll* grabbed =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const RigPiece* rig_piece =
      grabbed != nullptr && picked_.size() == 1 ? FindRig(grabbed->rig, joint)
                                                : nullptr;
  const auto under = rig_piece != nullptr
                         ? IntoPiece(*grabbed, joint, World(), point)
                         : std::nullopt;
  if (under) {
    grab_cursor_ = *under;
    grab_pivot_ = rig_piece->pivot;
  }
  scope_ = std::make_unique<EditScope>(
      managers_.history, picked_.size() > 1
                             ? tr("Move %1 joints").arg(picked_.size())
                             : tr("Move pivot of %1").arg(joint));
}

bool RigCanvas::PressHandle(QPointF point,
                            Qt::KeyboardModifiers modifiers) {
  assert(std::isfinite(point.x()));
  assert(managers_.history != nullptr);
  const auto handle = HandleAt(point);
  if (!handle) {
    return false;
  }
  const bool is_dot = handle->kind == GrabKind::kColliderDot;
  if (is_dot) {
    // A dot click does not drag, so a plain click narrows at once.
    PickDot(handle->key,
            ClickMode(modifiers, picked_dots_.contains(handle->key))
                .value_or(PickMode::kReplace));
    return true;
  }
  press_ = point;
  is_started_ = false;
  // The handle jumps under the cursor: moves measure from its centre.
  last_ = handle->spot;
  start_ = handle->spot;
  const bool is_tip = handle->kind == GrabKind::kIkTip;
  if (is_tip) {
    drag_ = Drag::kTip;
    chain_ = handle->key;
    scope_ = std::make_unique<EditScope>(
        managers_.history, tr("Move IK tip of %1").arg(handle->key));
    GrabGhost();
    return true;
  }
  // Any joint can be grabbed; a picked one drags every picked joint.
  const QString joint = handle->key;
  const bool is_rope = modifiers.testFlag(Qt::AltModifier);
  if (is_rope) {
    StartRope(joint, point);
    return true;
  }
  // The pick rule: what would take the joint out or narrow the pick
  // waits for the release, so the press can still drag every pick.
  const bool is_in_pick = picked_.contains(joint);
  const auto mode = ClickMode(modifiers, is_in_pick);
  const bool is_later =
      !mode || (*mode == PickMode::kToggle && is_in_pick);
  later_joint_ = is_later ? joint : QString();
  later_mode_ = mode.value_or(PickMode::kReplace);
  if (!is_later) {
    ApplyPick({joint}, *mode, joint);
  }
  GrabPivot(joint, point);
  GrabGhost();
  return true;
}

void RigCanvas::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  if (PanPress(event)) {
    return;
  }
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool is_usable = doll != nullptr && event->button() == Qt::LeftButton;
  if (!is_usable) {
    return;
  }
  if (is_hanging_) {
    FinishHang(event->position());
    return;
  }
  const bool is_handle = PressHandle(event->position(), event->modifiers());
  if (is_handle) {
    return;
  }
  const auto hit = HitDollPiece(*doll, PoseMap(), World(),
                                event->position(), &cache_);
  if (!hit) {
    // Empty space starts a pick box.
    is_boxing_ = true;
    box_from_ = event->position();
    box_to_ = box_from_;
    box_mode_ = BoxMode(event->modifiers());
    return;
  }
  // Pieces do not drag here, so a plain click on a picked one narrows
  // the pick at once.
  ApplyPick({*hit},
            ClickMode(event->modifiers(), picked_.contains(*hit))
                .value_or(PickMode::kReplace),
            *hit);
}

void RigCanvas::ApplyPick(const std::set<QString>& pieces, PickMode mode,
                          const QString& focus) {
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(focus.size() < 100000);
  picked_ = Combine(picked_, pieces, mode);
  PruneDots();
  const bool keeps_focus = picked_.contains(focus);
  piece_ = keeps_focus           ? focus
           : picked_.empty()     ? QString()
                                 : *picked_.begin();
  emit PickChanged(QStringList(picked_.begin(), picked_.end()), piece_);
  update();
}

void RigCanvas::FinishBox() {
  assert(is_boxing_);
  assert(managers_.history != nullptr);
  is_boxing_ = false;
  const QRectF box = QRectF(box_from_, box_to_).normalized();
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const bool is_click =
      box.width() < kDragDeadZone && box.height() < kDragDeadZone;
  std::set<QString> caught;
  const bool is_drawn = doll != nullptr && !is_click;
  // A box over collider dots of picked pieces picks those dots; over
  // none it picks pieces as it always did.
  std::set<QString> dots;
  for (const Handle& handle : is_drawn ? HandlesOf() : std::vector<Handle>()) {
    const bool is_dot_in = handle.kind == GrabKind::kColliderDot &&
                           box.contains(handle.spot);
    if (is_dot_in) {
      dots.insert(handle.key);
    }
  }
  const bool has_dots = !dots.empty();
  if (has_dots) {
    picked_dots_ = Combine(picked_dots_, dots, box_mode_);
    update();
    return;
  }
  if (is_drawn) {
    const QTransform world = World();
    QPainterPath area;
    area.addRect(box);
    for (const auto& [name, at] : PieceTransforms(*doll, PoseMap())) {
      const ArtPiece* art = FindArt(*doll, name);
      QPainterPath shape;
      shape.addPolygon((at * world).map(
          QPolygonF(QRectF(QPointF(), QSizeF(art->size)))));
      const bool is_in = area.intersects(shape);
      if (is_in) {
        caught.insert(name);
      }
    }
  }
  ApplyPick(caught, box_mode_,
            caught.empty() ? piece_ : *caught.begin());
}

void RigCanvas::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(managers_.history != nullptr);
  if (PanMove(event)) {
    return;
  }
  cursor_ = event->position();
  if (is_hanging_) {
    update();
    return;
  }
  if (is_boxing_) {
    box_to_ = event->position();
    update();
    return;
  }
  const bool is_dragging = drag_ != Drag::kNone;
  if (!is_dragging) {
    UpdateHover(event->position());
    return;
  }
  is_started_ = is_started_ || IsPastDeadZone(press_, event->position());
  if (!is_started_) {
    return;
  }
  ShowDrag(event->position());
  const Doll* doll = FindDoll(managers_.history->current(), doll_);
  const IkChain* chain =
      chain_.isEmpty() || doll == nullptr ? nullptr
                                          : FindChain(doll->rig, chain_);
  const bool is_tip = drag_ == Drag::kTip && chain != nullptr;
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return;
  }
  if (is_tip) {
    const auto spot =
        IntoPiece(*doll, chain->lower, World(), event->position());
    if (spot) {
      emit Problem(
          ProblemOf(managers_.rig->SetChainTip(doll_, chain_, *spot)));
    }
    return;
  }
  MovePivots(*doll, event->position(),
             event->modifiers().testFlag(Qt::ControlModifier));
}

void RigCanvas::MovePivots(const Doll& doll, QPointF cursor, bool is_free) {
  assert(std::isfinite(cursor.x()) && std::isfinite(cursor.y()));
  assert(managers_.rig != nullptr);
  // Each joint moves by the cursor's step, in its own drawing's pixels.
  std::map<QString, QPointF> offsets;
  PivotSnap snap;
  const std::optional<ShapeSnapKind> was =
      shape_snap_ ? std::optional<ShapeSnapKind>(shape_snap_->kind)
                  : std::nullopt;
  shape_snap_.reset();
  for (const QString& piece : picked_) {
    const auto now = IntoPiece(doll, piece, World(), cursor);
    const auto before = IntoPiece(doll, piece, World(), last_);
    const bool is_mapped = now.has_value() && before.has_value();
    if (is_mapped) {
      offsets[piece] = *now - *before;
    }
    // A lone pivot catches on its drawn limb and its drawing's box,
    // unless Ctrl is held.
    const RigPiece* rig_piece = FindRig(doll.rig, piece);
    const bool is_lone = picked_.size() == 1 && now.has_value() &&
                         rig_piece != nullptr;
    if (is_lone) {
      const QPointF wanted = grab_pivot_ + (*now - grab_cursor_);
      snap = is_free ? PivotSnap{wanted, std::nullopt, false}
                     : SnappedPivot(doll, piece, wanted);
      offsets[piece] = snap.spot - rig_piece->pivot;
      shape_snap_ = snap.shape;
      snap_piece_ = piece;
    }
  }
  last_ = cursor;
  const bool is_shape = shape_snap_.has_value();
  if (is_shape) {
    // The chip names what it caught and flashes when that changes.
    const bool is_new = !was.has_value() || *was != shape_snap_->kind;
    feedback_.Update(start_, cursor, SnapName(shape_snap_->kind), is_new);
  }
  emit Problem(ProblemOf(managers_.rig->MovePivots(doll_, offsets)));
}

QString RigCanvas::SnapName(ShapeSnapKind kind) const {
  assert(kind == ShapeSnapKind::kWidthMiddle ||
         kind == ShapeSnapKind::kEndCenter ||
         kind == ShapeSnapKind::kOverlapCenter);
  assert(drag_ != Drag::kNone);
  return kind == ShapeSnapKind::kWidthMiddle ? tr("Width middle")
         : kind == ShapeSnapKind::kEndCenter ? tr("End center")
                                             : tr("Overlap center");
}

void RigCanvas::ShowDrag(QPointF cursor) {
  assert(drag_ != Drag::kNone);
  assert(std::isfinite(cursor.x()) && std::isfinite(cursor.y()));
  feedback_.Update(start_, cursor, OffsetText(cursor - start_), false);
}

void RigCanvas::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  if (PanRelease(event)) {
    return;
  }
  const bool was_rope = is_rope_ && is_hanging_;
  is_rope_ = false;
  if (was_rope) {
    FinishHang(event->position());
    return;
  }
  if (is_boxing_) {
    FinishBox();
  }
  const bool is_late_pick = !later_joint_.isEmpty() && !is_started_;
  if (is_late_pick) {
    if (scope_ != nullptr) {
      scope_->Cancel();
    }
    ApplyPick({later_joint_}, later_mode_, later_joint_);
  }
  later_joint_.clear();
  feedback_.Settle();
  scope_.reset();
  shape_snap_.reset();
  drag_ = Drag::kNone;
  chain_.clear();
  is_started_ = false;
  assert(scope_ == nullptr);
}

void RigCanvas::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool stops_box = event->key() == Qt::Key_Escape && is_boxing_;
  if (stops_box) {
    is_boxing_ = false;
    update();
    return;
  }
  const bool stops_hang = event->key() == Qt::Key_Escape && is_hanging_;
  const bool clears = event->key() == Qt::Key_Escape && scope_ == nullptr &&
                      !is_hanging_;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (stops_hang) {
    StopHanging();
    return;
  }
  const bool clears_dots = clears && !picked_dots_.empty();
  if (clears_dots) {
    picked_dots_.clear();
    update();
    return;
  }
  if (clears) {
    ApplyPick({}, PickMode::kReplace, QString());
    return;
  }
  const bool is_delete = (event->key() == Qt::Key_Delete ||
                          event->key() == Qt::Key_Backspace) &&
                         !picked_dots_.empty() && scope_ == nullptr;
  if (is_delete) {
    DeletePickedDots();
    return;
  }
  if (is_all) {
    const Doll* doll = doll_.isEmpty()
                           ? nullptr
                           : FindDoll(managers_.history->current(), doll_);
    std::set<QString> all;
    for (const RigPiece& piece :
         doll != nullptr ? doll->rig.pieces : std::vector<RigPiece>()) {
      all.insert(piece.name);
    }
    ApplyPick(all, PickMode::kReplace, piece_);
    return;
  }
  const bool is_circle = event->key() == Qt::Key_J &&
                         event->modifiers() == Qt::NoModifier &&
                         scope_ == nullptr && !piece_.isEmpty();
  if (is_circle) {
    ToggleJointCircle(piece_);
    return;
  }
  const bool is_cancel = event->key() == Qt::Key_Escape && scope_ != nullptr;
  if (is_cancel) {
    scope_->Cancel();
    scope_.reset();
    later_joint_.clear();
    feedback_.Cancel();
    shape_snap_.reset();
    drag_ = Drag::kNone;
    update();
    return;
  }
  QWidget::keyPressEvent(event);
  assert(!is_cancel);
}

}  // namespace snapper
