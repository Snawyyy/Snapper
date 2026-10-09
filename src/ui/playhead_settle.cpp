#include "ui/playhead_settle.h"

#include <cassert>
#include <cmath>

#include "ui/motion.h"

namespace snapper {

PlayheadSettle::PlayheadSettle(MotionManager* motion, Frame start,
                               QObject* parent)
    : QObject(parent), motion_(motion), target_(start) {
  assert(motion_ != nullptr);
  assert(parent != nullptr);
}

void PlayheadSettle::MoveTo(Frame to, bool is_playing) {
  assert(motion_ != nullptr);
  assert(to.index() >= 0);
  // Where it is drawn now, so a move in mid-settle carries on from there.
  const double shown = target_.index() + offset_;
  const double start_offset = shown - to.index();
  target_ = to;
  if (animation_) {
    animation_->stop();
  }
  const bool is_settling =
      !is_playing && std::abs(start_offset) <= kMaxSettleFrames &&
      start_offset != 0.0;
  if (!is_settling) {
    offset_ = 0.0;
    emit Changed();
    return;
  }
  // Drawn at the old place from this very paint, not from the first
  // tick of the animation.
  offset_ = start_offset;
  // Starts at the old place (progress 0) and passes the new one by a
  // little (the curve's overshoot) before resting on it.
  animation_ = motion_->Animate(
      this, MotionKind::kPop, 0.0, 1.0,
      [this, start_offset](const QVariant& value) {
        offset_ = start_offset * (1.0 - value.toDouble());
        emit Changed();
      });
}

}  // namespace snapper
