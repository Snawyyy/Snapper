// RigCanvas's hang mode: choosing the part a joint's piece hangs from,
// by a click after a double-click, or by a rope dragged with Alt.

#include <QMouseEvent>

#include <cassert>
#include <cmath>
#include <vector>

#include "edit/history_manager.h"
#include "edit/rig_manager.h"
#include "render/stage_hit.h"
#include "ui/problem.h"
#include "ui/rig_canvas.h"

namespace snapper {

void RigCanvas::StopHanging() {
  assert(managers_.history != nullptr);
  const bool was_hanging = is_hanging_;
  is_hanging_ = false;
  unsetCursor();
  assert(!is_hanging_);
  if (was_hanging) {
    emit Hint(QString());
    update();
  }
}

void RigCanvas::FinishHang(QPointF at) {
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  assert(managers_.rig != nullptr);
  const Doll* doll =
      doll_.isEmpty() ? nullptr : FindDoll(managers_.history->current(), doll_);
  const bool has_doll = doll != nullptr;
  if (!has_doll) {
    StopHanging();
    return;
  }
  const auto hit = HitDollPiece(*doll, PoseMap(), World(), at, &cache_);
  const QString parent = hit.value_or(QString());
  StopHanging();
  // Every picked piece hangs from the one hit, except itself.
  std::vector<QString> children;
  for (const QString& child : picked_) {
    const bool is_self = child == parent;
    if (!is_self) {
      children.push_back(child);
    }
  }
  emit Problem(
      ProblemOf(managers_.rig->SetParentAll(doll_, children, parent)));
}

void RigCanvas::StartRope(const QString& joint, QPointF at) {
  assert(!joint.isEmpty());
  assert(std::isfinite(at.x()) && std::isfinite(at.y()));
  picked_ = {joint};
  piece_ = joint;
  emit PickChanged(QStringList(picked_.begin(), picked_.end()), piece_);
  is_hanging_ = true;
  is_rope_ = true;
  cursor_ = at;
  setCursor(Qt::CrossCursor);
  emit Hint(tr("Let go over the part %1 hangs from, or over empty space to "
               "make it a root; Escape cancels.").arg(joint));
  update();
}

void RigCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const QString joint = JointAt(event->position());
  const bool is_joint = !joint.isEmpty() && event->button() == Qt::LeftButton;
  if (!is_joint) {
    return;
  }
  scope_.reset();
  feedback_.Cancel();
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

}  // namespace snapper
