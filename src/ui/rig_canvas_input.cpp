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

void RigCanvas::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool is_usable = doll != nullptr && event->button() == Qt::LeftButton;
  if (!is_usable) {
    return;
  }
  const bool is_handle = PressHandle(event->position());
  if (is_handle) {
    return;
  }
  const DollLayer rest{doll_, {}, false};
  const auto hit = HitDollPiece(*doll, rest, Frame(0), World(),
                                event->position(), &cache_);
  piece_ = hit.value_or(QString());
  emit PiecePicked(piece_);
  update();
}

void RigCanvas::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
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
