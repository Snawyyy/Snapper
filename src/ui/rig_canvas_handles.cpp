// RigCanvas's handles: finding the one under the cursor and hovering it.

#include <QMouseEvent>

#include <cassert>

#include "anim/doll_pose.h"
#include "edit/history_manager.h"
#include "render/stage_hit.h"
#include "ui/collider_keys.h"
#include "ui/rig_canvas.h"

namespace snapper {

std::vector<RigCanvas::Handle> RigCanvas::HandlesOf() const {
  assert(managers_.history != nullptr);
  assert(World().isAffine());
  std::vector<Handle> handles;
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return handles;
  }
  const QTransform world = World();
  const auto pieces = PieceTransforms(*doll, PoseMap());
  for (const IkChain& chain : doll->rig.chains) {
    const auto lower = pieces.find(chain.lower);
    const bool is_placed = lower != pieces.end();
    if (is_placed) {
      handles.push_back({GrabKind::kIkTip, chain.name,
                         (lower->second * world).map(chain.tip)});
    }
  }
  for (const RigPiece& rig : doll->rig.pieces) {
    const auto at = pieces.find(rig.name);
    const bool is_placed = at != pieces.end();
    if (is_placed) {
      handles.push_back(
          {GrabKind::kPivot, rig.name, (at->second * world).map(rig.pivot)});
    }
  }
  for (const RigPiece& rig : doll->rig.pieces) {
    const auto at = pieces.find(rig.name);
    const bool is_shown = at != pieces.end() && picked_.contains(rig.name);
    if (!is_shown) {
      continue;
    }
    for (const ColliderDot& dot : RigCollider(*doll, rig).dots) {
      handles.push_back({GrabKind::kColliderDot, DotKey(rig.name, dot.slot),
                         (at->second * world).map(dot.at)});
    }
  }
  return handles;
}

std::optional<RigCanvas::Handle> RigCanvas::HandleAt(QPointF point) const {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(managers_.history != nullptr);
  const std::vector<Handle> handles = HandlesOf();
  std::vector<QPointF> spots;
  for (const Handle& handle : handles) {
    spots.push_back(handle.spot);
  }
  const int nearest = NearestGrab(spots, point);
  const bool is_hit = nearest >= 0;
  return is_hit ? std::optional<Handle>(handles[static_cast<size_t>(nearest)])
                : std::nullopt;
}

QString RigCanvas::JointAt(QPointF point) const {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(managers_.history != nullptr);
  const std::vector<Handle> handles = HandlesOf();
  std::vector<QPointF> spots;
  std::vector<QString> names;
  for (const Handle& handle : handles) {
    const bool is_joint = handle.kind == GrabKind::kPivot;
    if (is_joint) {
      spots.push_back(handle.spot);
      names.push_back(handle.key);
    }
  }
  const int nearest = NearestGrab(spots, point);
  const bool is_hit = nearest >= 0;
  return is_hit ? names[static_cast<size_t>(nearest)] : QString();
}

void RigCanvas::UpdateHover(QPointF point) {
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  assert(drag_ == Drag::kNone);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    ClearHover();
    return;
  }
  const auto handle = HandleAt(point);
  const auto piece =
      handle ? std::nullopt
             : HitDollPiece(*doll, PoseMap(), World(), point, &cache_);
  hover_.Set(handle ? handle->kind : GrabKind::kNone,
             handle ? handle->key : QString());
  const QString now = piece.value_or(QString());
  const bool is_piece_changed = now != hover_piece_;
  hover_piece_ = now;
  setCursor(handle                 ? CursorFor(handle->kind)
            : !hover_piece_.isEmpty() ? Qt::PointingHandCursor
                                      : Qt::ArrowCursor);
  if (is_piece_changed) {
    update();
  }
}

void RigCanvas::ClearHover() {
  hover_.Set(GrabKind::kNone, QString());
  hover_piece_.clear();
  assert(hover_.kind() == GrabKind::kNone);
  unsetCursor();
  update();
  assert(hover_piece_.isEmpty());
}

void RigCanvas::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  const bool is_idle = drag_ == Drag::kNone && !is_hanging_;
  assert(hover_.kind() >= GrabKind::kNone);
  if (is_idle) {
    ClearHover();
  }
  QWidget::leaveEvent(event);
}

void RigCanvas::GrabGhost() {
  assert(drag_ != Drag::kNone);
  assert(managers_.history != nullptr);
  const Doll* doll = FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return;
  }
  const auto pieces = PieceTransforms(*doll, PoseMap());
  const bool is_tip = drag_ == Drag::kTip;
  feedback_.SetGhost(MakeGhost(
      GhostKind::kRigHandle, size(), devicePixelRatioF(),
      [&](QPainter* painter) {
        for (const IkChain& chain : doll->rig.chains) {
          const bool is_dragged = is_tip && chain.name == chain_ &&
                                  pieces.contains(chain.lower);
          if (is_dragged) {
            PaintTip(painter, pieces, chain);
          }
        }
        for (const RigPiece& rig : doll->rig.pieces) {
          const bool is_dragged = !is_tip && picked_.contains(rig.name) &&
                                  pieces.contains(rig.name);
          if (is_dragged) {
            PaintJoint(painter, *doll, pieces, rig);
          }
        }
      }));
}

}  // namespace snapper
