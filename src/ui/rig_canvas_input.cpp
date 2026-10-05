// RigCanvas's mouse and keys.

#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>

#include <cassert>

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
  const RigPiece* rig =
      piece_.isEmpty() ? nullptr : FindRig(doll->rig, piece_);
  const bool is_on_joint =
      rig != nullptr && pieces.contains(piece_) &&
      QLineF((pieces.at(piece_) * world).map(rig->pivot), point).length() <=
          kHandleReach;
  if (is_on_joint) {
    drag_ = Drag::kPivot;
    scope_ = std::make_unique<EditScope>(
        managers_.history, tr("Move pivot of %1").arg(piece_));
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
  piece_ = joint;
  emit PiecePicked(piece_);
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
    const QString child = piece_;
    StopHanging();
    const bool is_self = parent == child;
    if (!is_self) {
      emit Problem(ProblemOf(managers_.rig->SetParent(doll_, child, parent)));
    }
    return;
  }
  const bool is_handle = PressHandle(event->position());
  if (is_handle) {
    return;
  }
  const auto hit = HitDollPiece(*doll, rest, Frame(0), World(),
                                event->position(), &cache_);
  piece_ = hit.value_or(QString());
  emit PiecePicked(piece_);
  update();
}

void RigCanvas::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  cursor_ = event->position();
  if (is_hanging_) {
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
  const QString& piece = is_tip ? chain->lower : piece_;
  const auto spot = doll != nullptr
                        ? IntoPiece(*doll, piece, World(), event->position())
                        : std::nullopt;
  if (!spot) {
    return;
  }
  const Result<void> moved =
      is_tip ? managers_.rig->SetChainTip(doll_, chain_, *spot)
             : managers_.rig->SetPivot(doll_, piece_, *spot);
  emit Problem(ProblemOf(moved));
}

void RigCanvas::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  scope_.reset();
  drag_ = Drag::kNone;
  chain_.clear();
  assert(scope_ == nullptr);
}

void RigCanvas::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool stops_hang = event->key() == Qt::Key_Escape && is_hanging_;
  if (stops_hang) {
    StopHanging();
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
