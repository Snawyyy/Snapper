// RigCanvas's pivot snap: a joint catching on the middle of its drawn
// limb, an end's circle center, where it overlaps its parent, or the
// middle and edges of its drawing.

#include <QPainter>
#include <QStringList>

#include <cassert>
#include <cmath>
#include <map>

#include "anim/doll_pose.h"
#include "anim/guides.h"
#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "ui/drag_text.h"
#include "ui/grab.h"
#include "ui/problem.h"
#include "ui/rig_canvas.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"

namespace snapper {

PivotSnap RigCanvas::SnappedPivot(const Doll& doll, const QString& piece,
                                  QPointF wanted) {
  assert(std::isfinite(wanted.x()) && std::isfinite(wanted.y()));
  assert(!piece.isEmpty());
  const QImage own_drawing = cache_.Get(DrawingPath(doll, piece, -1));
  const bool is_undrawn = own_drawing.isNull();
  if (is_undrawn) {
    return PivotSnap{wanted, std::nullopt, false, std::nullopt};
  }
  const auto own = limbs_.Of(own_drawing);
  // A few screen pixels, in the drawing's own.
  const QTransform world = World();
  const double scale = std::hypot(world.m11(), world.m12());
  const double reach = scale > 1e-9 ? kGuideReach / scale : 0.0;
  const RigPiece* rig = FindRig(doll.rig, piece);
  const QString parent_name = rig != nullptr ? rig->parent : QString();
  const auto pieces = PieceTransforms(doll, PoseMap());
  const bool has_parent = !parent_name.isEmpty() &&
                          pieces.contains(parent_name) &&
                          pieces.contains(piece);
  const QImage parent_drawing =
      has_parent ? cache_.Get(DrawingPath(doll, parent_name, -1)) : QImage();
  const auto parent =
      parent_drawing.isNull() ? nullptr : limbs_.Of(parent_drawing);
  bool is_invertible = false;
  const QTransform parent_to_own =
      parent != nullptr ? pieces.at(parent_name) *
                              pieces.at(piece).inverted(&is_invertible)
                        : QTransform();
  return SnapPivot(*own, is_invertible ? parent.get() : nullptr,
                   parent_to_own, wanted, reach);
}

QPointF RigCanvas::CatchPivot(const Doll& doll, const QString& piece,
                              QPointF wanted, bool is_free) {
  assert(std::isfinite(wanted.x()) && std::isfinite(wanted.y()));
  assert(!piece.isEmpty());
  const PivotSnap snap =
      is_free ? PivotSnap{wanted, std::nullopt, false, std::nullopt}
              : SnappedPivot(doll, piece, wanted);
  is_snap_caught_ = snap.shape.has_value();
  shape_snap_ = is_snap_caught_ ? snap.shape : snap.near;
  snap_piece_ = piece;
  return snap.spot;
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

void RigCanvas::PaintShapeSnap(
    QPainter* painter, const std::map<QString, QTransform>& pieces) const {
  assert(painter != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const bool shows = drag_ == Drag::kPivot && is_started_ &&
                     shape_snap_.has_value() && pieces.contains(snap_piece_);
  if (!shows) {
    return;
  }
  // What it would catch shows faint until it catches.
  QColor color = theme::kPick;
  color.setAlpha(is_snap_caught_ ? 255 : 140);
  const QTransform at = pieces.at(snap_piece_) * World();
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(color, 1.0, Qt::DashLine));
  painter->drawLine(at.map(shape_snap_->middle_a),
                    at.map(shape_snap_->middle_b));
  painter->setPen(QPen(color, 1.0));
  painter->drawLine(at.map(shape_snap_->edge_a), at.map(shape_snap_->edge_b));
  const bool has_circle = shape_snap_->radius > 0.0;
  if (has_circle) {
    QPen ring(color, 1.0);
    ring.setCosmetic(true);
    painter->save();
    painter->setPen(ring);
    painter->setTransform(at, true);
    painter->drawEllipse(shape_snap_->spot, shape_snap_->radius,
                         shape_snap_->radius);
    painter->restore();
  }
  // A ring wider than the joint's own handle, so it shows around it.
  painter->setPen(QPen(color, 2.0));
  painter->drawEllipse(at.map(shape_snap_->spot), kHandleDraw, kHandleDraw);
  painter->restore();
}

void RigCanvas::SnapJoints(const std::vector<QString>& pieces) {
  assert(managers_.rig != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    return;
  }
  // Every drawing, so parents are there for the overlap center.
  std::map<QString, QImage> drawings;
  for (const RigPiece& rig : doll->rig.pieces) {
    drawings[rig.name] = cache_.Get(DrawingPath(*doll, rig.name, -1));
  }
  const auto moved =
      managers_.rig->SnapJointsToShape(doll_, pieces, drawings);
  emit Problem(ProblemOf(moved));
  const bool says = moved.has_value() && managers_.toasts != nullptr;
  if (says) {
    managers_.toasts->Done(
        tr("Snapped to shape: %1")
            .arg(QStringList(moved->begin(), moved->end()).join(", ")));
  }
  update();
}

void RigCanvas::ToggleJointCircle(const QString& piece) {
  assert(managers_.rig != nullptr);
  assert(piece.size() < 100000);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool can_act = doll != nullptr && !piece.isEmpty();
  if (!can_act) {
    return;
  }
  const QImage drawing = cache_.Get(DrawingPath(*doll, piece, -1));
  emit Problem(ProblemOf(
      managers_.rig->ToggleJointCircle(doll_, piece, drawing)));
}

void RigCanvas::ColorJointCircle(const QColor& color) {
  assert(managers_.rig != nullptr);
  assert(managers_.history != nullptr);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const RigPiece* rig = doll != nullptr && !piece_.isEmpty()
                            ? FindRig(doll->rig, piece_)
                            : nullptr;
  // The palette serves draw mode too: only a shown canvas with a circle
  // to color takes the pick.
  const bool takes = isVisible() && rig != nullptr && rig->joint_circle.is_on;
  if (takes) {
    emit Problem(ProblemOf(
        managers_.rig->SetJointCircleColor(doll_, piece_, color)));
  }
}

}  // namespace snapper
