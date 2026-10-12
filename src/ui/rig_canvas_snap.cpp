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
  if (own_drawing.isNull()) {
    return PivotSnap{wanted, std::nullopt, false};
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

void RigCanvas::PaintShapeSnap(
    QPainter* painter, const std::map<QString, QTransform>& pieces) const {
  assert(painter != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const bool shows = drag_ == Drag::kPivot && is_started_ &&
                     shape_snap_.has_value() && pieces.contains(snap_piece_);
  if (!shows) {
    return;
  }
  const QTransform at = pieces.at(snap_piece_) * World();
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(theme::kPick, 1.0));
  painter->drawLine(at.map(shape_snap_->edge_a), at.map(shape_snap_->edge_b));
  painter->setBrush(theme::kPick);
  painter->drawEllipse(at.map(shape_snap_->spot), 3.0, 3.0);
  const bool has_circle = shape_snap_->radius > 0.0;
  if (has_circle) {
    QPen ring(theme::kPick, 1.0);
    ring.setCosmetic(true);
    painter->setPen(ring);
    painter->setBrush(Qt::NoBrush);
    painter->setTransform(at, true);
    painter->drawEllipse(shape_snap_->spot, shape_snap_->radius,
                         shape_snap_->radius);
  }
  painter->restore();
}

void RigCanvas::SnapJoints(const std::vector<QString>& pieces) {
  assert(managers_.rig != nullptr);
  assert(pieces.size() <= static_cast<size_t>(kMaxDollPieces));
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  if (doll == nullptr) {
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

}  // namespace snapper
