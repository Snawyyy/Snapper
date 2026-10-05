// RigCanvas's mouse and keys.

#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPainterPath>

#include <cassert>
#include <map>

#include "anim/doll_pose.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "render/stage_hit.h"
#include "ui/pose_tool.h"
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

bool RigCanvas::PressHandle(QPointF point) {
  assert(std::isfinite(point.x()));
  const Doll* doll = FindDoll(managers_.history->current(), doll_);
  assert(doll != nullptr);
  const QTransform world = World();
  const auto pieces = PieceTransforms(*doll, PoseMap());
  for (const IkChain& chain : doll->rig.chains) {
    const auto lower = pieces.find(chain.lower);
    const bool is_near =
        lower != pieces.end() &&
        QLineF((lower->second * world).map(chain.tip), point).length() <=
            kHandleReach;
    if (is_near) {
      drag_ = Drag::kTip;
      chain_ = chain.name;
      scope_ = std::make_unique<EditScope>(
          managers_.history, tr("Move IK tip of %1").arg(chain.name));
      return true;
    }
  }
  // Any joint can be grabbed; a picked one drags every picked joint.
  const QString joint = JointAt(point);
  const bool is_on_joint = !joint.isEmpty();
  if (is_on_joint) {
    const bool is_in_pick = picked_.contains(joint);
    if (!is_in_pick) {
      ApplyPick({joint}, PickMode::kReplace, joint);
    }
    drag_ = Drag::kPivot;
    last_ = point;
    scope_ = std::make_unique<EditScope>(
        managers_.history, picked_.size() > 1
                               ? tr("Move %1 joints").arg(picked_.size())
                               : tr("Move pivot of %1").arg(joint));
  }
  return is_on_joint;
}

QString RigCanvas::JointAt(QPointF point) const {
  assert(std::isfinite(point.x()));
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return QString();
  }
  const QTransform world = World();
  const auto pieces = PieceTransforms(*doll, PoseMap());
  for (const RigPiece& rig : doll->rig.pieces) {
    const bool is_near =
        pieces.contains(rig.name) &&
        QLineF((pieces.at(rig.name) * world).map(rig.pivot), point)
                .length() <= kHandleReach;
    if (is_near) {
      return rig.name;
    }
  }
  assert(doll->rig.pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  return QString();
}

void RigCanvas::StopHanging() {
  const bool was_hanging = is_hanging_;
  is_hanging_ = false;
  unsetCursor();
  assert(!is_hanging_);
  if (was_hanging) {
    emit Hint(QString());
    update();
  }
}

void RigCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QString joint = JointAt(event->position());
  const bool is_joint = !joint.isEmpty() && event->button() == Qt::LeftButton;
  if (!is_joint) {
    return;
  }
  scope_.reset();
  drag_ = Drag::kNone;
  const bool is_in_pick = picked_.contains(joint);
  if (!is_in_pick) {
    picked_ = {joint};
  }
  piece_ = joint;
  emit PickChanged(QStringList(picked_.begin(), picked_.end()), piece_);
  is_hanging_ = true;
  cursor_ = event->position();
  setCursor(Qt::CrossCursor);
  emit Hint(tr("Click the part %1 hangs from; click empty space to make "
               "it a root; Escape cancels.").arg(joint));
  update();
  assert(is_hanging_);
}

void RigCanvas::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool is_usable = doll != nullptr && event->button() == Qt::LeftButton;
  if (!is_usable) {
    return;
  }
  const DollLayer rest{doll_, {}, false};
  if (is_hanging_) {
    const auto hit = HitDollPiece(*doll, rest, Frame(0), World(),
                                  event->position(), &cache_);
    const QString parent = hit.value_or(QString());
    StopHanging();
    // Every picked piece hangs from the clicked one, except itself.
    std::vector<QString> children;
    for (const QString& child : picked_) {
      const bool is_self = child == parent;
      if (!is_self) {
        children.push_back(child);
      }
    }
    emit Problem(ProblemOf(
        managers_.rig->SetParentAll(doll_, children, parent)));
    return;
  }
  const bool is_handle = PressHandle(event->position());
  if (is_handle) {
    return;
  }
  const auto hit = HitDollPiece(*doll, rest, Frame(0), World(),
                                event->position(), &cache_);
  const bool is_shift = event->modifiers().testFlag(Qt::ShiftModifier);
  const bool is_ctrl = event->modifiers().testFlag(Qt::ControlModifier);
  const PickMode mode = is_shift  ? PickMode::kAdd
                        : is_ctrl ? PickMode::kToggle
                                  : PickMode::kReplace;
  if (!hit) {
    // Empty space starts a pick box.
    is_boxing_ = true;
    box_from_ = event->position();
    box_to_ = box_from_;
    box_mode_ = is_shift  ? PickMode::kAdd
                : is_ctrl ? PickMode::kRemove
                          : PickMode::kReplace;
    return;
  }
  ApplyPick({*hit}, mode, *hit);
}

void RigCanvas::ApplyPick(const std::set<QString>& pieces, PickMode mode,
                          const QString& focus) {
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  assert(focus.size() < 100000);
  picked_ = Combine(picked_, pieces, mode);
  const bool keeps_focus = picked_.contains(focus);
  piece_ = keeps_focus           ? focus
           : picked_.empty()     ? QString()
                                 : *picked_.begin();
  emit PickChanged(QStringList(picked_.begin(), picked_.end()), piece_);
  update();
}

void RigCanvas::FinishBox() {
  assert(is_boxing_);
  is_boxing_ = false;
  const QRectF box = QRectF(box_from_, box_to_).normalized();
  const Doll* doll = doll_.isEmpty()
                         ? nullptr
                         : FindDoll(managers_.history->current(), doll_);
  const bool is_click = box.width() < 3.0 && box.height() < 3.0;
  std::set<QString> caught;
  const bool is_drawn = doll != nullptr && !is_click;
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
    return;
  }
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
  // Each joint moves by the cursor's step, in its own drawing's pixels.
  std::map<QString, QPointF> offsets;
  for (const QString& piece : picked_) {
    const auto now = IntoPiece(*doll, piece, World(), event->position());
    const auto before = IntoPiece(*doll, piece, World(), last_);
    const bool is_mapped = now.has_value() && before.has_value();
    if (is_mapped) {
      offsets[piece] = *now - *before;
    }
  }
  last_ = event->position();
  emit Problem(ProblemOf(managers_.rig->MovePivots(doll_, offsets)));
}

void RigCanvas::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  if (is_boxing_) {
    FinishBox();
  }
  scope_.reset();
  drag_ = Drag::kNone;
  chain_.clear();
  assert(scope_ == nullptr);
}

void RigCanvas::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool stops_hang = event->key() == Qt::Key_Escape && is_hanging_;
  const bool clears = event->key() == Qt::Key_Escape && scope_ == nullptr &&
                      !is_hanging_;
  const bool is_all = event->matches(QKeySequence::SelectAll);
  if (stops_hang) {
    StopHanging();
    return;
  }
  if (clears) {
    ApplyPick({}, PickMode::kReplace, QString());
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
  const bool is_cancel = event->key() == Qt::Key_Escape && scope_ != nullptr;
  if (is_cancel) {
    scope_->Cancel();
    scope_.reset();
    drag_ = Drag::kNone;
    return;
  }
  QWidget::keyPressEvent(event);
  assert(!is_cancel);
}

}  // namespace snapper
