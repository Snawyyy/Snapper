#include "ui/stage_poke.h"

#include <cassert>
#include <cmath>

#include "edit/poke_manager.h"
#include "ui/toast_manager.h"

namespace snapper {

StagePoke::StagePoke(const Managers& managers) : managers_(managers) {
  assert(managers_.poke != nullptr);
  assert(managers_.toasts != nullptr);
}

bool StagePoke::IsPoking() const {
  assert(managers_.poke != nullptr);
  assert(managers_.toasts != nullptr);
  return managers_.poke->IsActive() && managers_.poke->IsHeld();
}

QPointF StagePoke::ShotPoint(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  return (point - frame.corner) / frame.scale;
}

void StagePoke::Arm(const StageFrame& frame, LayerId under) {
  assert(frame.scale > 0.0);
  assert(!is_armed_ || managers_.poke != nullptr);
  is_armed_ = true;
  const QString why = managers_.poke->WhyNot(frame.shot, under);
  managers_.toasts->Problem(why);
}

void StagePoke::Disarm() {
  assert(managers_.poke != nullptr);
  assert(managers_.toasts != nullptr);
  is_armed_ = false;
  managers_.poke->Release();
}

bool StagePoke::Press(QPointF point, LayerId under,
                      const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  managers_.poke->End();
  if (!is_armed_) {
    return false;
  }
  const QString why = managers_.poke->Begin(frame.shot, under, frame.local,
                                            ShotPoint(point, frame));
  managers_.toasts->Problem(why);
  return true;
}

void StagePoke::Move(QPointF point, const StageFrame& frame) {
  assert(frame.scale > 0.0);
  assert(std::isfinite(point.x()) && std::isfinite(point.y()));
  managers_.poke->Move(ShotPoint(point, frame));
}

void StagePoke::Release() {
  assert(managers_.poke != nullptr);
  assert(managers_.toasts != nullptr);
  managers_.poke->Release();
}

bool StagePoke::Cancel() {
  assert(managers_.poke != nullptr);
  assert(managers_.toasts != nullptr);
  const bool was_active = managers_.poke->IsActive();
  managers_.poke->End();
  return was_active;
}

Qt::CursorShape StagePoke::CursorOver(LayerId under,
                                      const StageFrame& frame) const {
  assert(frame.scale > 0.0);
  assert(managers_.poke != nullptr);
  const bool is_poking = IsPoking();
  const bool is_pokable =
      is_armed_ && managers_.poke->WhyNot(frame.shot, under).isEmpty();
  if (is_poking) {
    return Qt::ClosedHandCursor;
  }
  return is_pokable ? Qt::OpenHandCursor : Qt::ArrowCursor;
}

void StagePoke::Sync(PoseMemo* memo) {
  assert(memo != nullptr);
  assert(managers_.poke != nullptr);
  const bool was_synced = synced_.IsValid();
  if (was_synced) {
    memo->ClearOverride(synced_);
    synced_ = LayerId();
  }
  const bool is_live = managers_.poke->IsActive();
  if (is_live) {
    synced_ = managers_.poke->layer();
    memo->SetOverride(synced_, managers_.poke->poses());
  }
}

}  // namespace snapper
