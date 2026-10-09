#include "ui/bump_label.h"

#include <algorithm>
#include <cassert>

#include "ui/motion.h"

namespace snapper {

BumpLabel::BumpLabel(MotionManager* motion) : motion_(motion) {
  assert(motion != nullptr);
  assert(lift_ == 0);
}

void BumpLabel::SetText(const QString& text, bool is_bumping) {
  assert(motion_ != nullptr);
  assert(kBumpPx > 0);
  const bool is_new = text != this->text();
  setText(text);
  const bool is_hopping = is_new && is_bumping;
  if (!is_hopping) {
    return;
  }
  const bool is_running = !animation_.isNull();
  if (is_running) {
    animation_->stop();
  }
  // The hop is the bottom margin shrinking from kBumpPx to nothing; the
  // label is fixed in size, so nothing around it moves.
  const bool is_instant = motion_->duration(MotionKind::kPop) == 0;
  if (!is_instant) {
    SetLift(kBumpPx);
  }
  animation_ = motion_->Animate(
      this, MotionKind::kPop, kBumpPx, 0,
      [this](const QVariant& value) { SetLift(value.toInt()); });
}

void BumpLabel::SetLift(int lift) {
  assert(lift >= -kBumpPx);
  assert(lift <= kBumpPx * 2);
  lift_ = std::max(lift, 0);
  setContentsMargins(0, 0, 0, lift_);
}

}  // namespace snapper
