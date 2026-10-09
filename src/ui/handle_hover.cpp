#include "ui/handle_hover.h"

#include <cassert>

#include "ui/motion.h"

namespace snapper {

HandleHover::HandleHover(MotionManager* motion, QObject* parent)
    : QObject(parent), motion_(motion) {
  assert(motion_ != nullptr);
  assert(scale_ == 1.0);
}

void HandleHover::Set(GrabKind kind, const QString& key) {
  assert(kind != GrabKind::kNone || key.isEmpty());
  assert(motion_ != nullptr);
  const bool is_same = kind == kind_ && key == key_;
  if (is_same) {
    return;
  }
  if (animation_) {
    animation_->stop();
  }
  kind_ = kind;
  key_ = key;
  scale_ = 1.0;
  const bool is_hovering = kind != GrabKind::kNone;
  if (is_hovering) {
    animation_ = motion_->Animate(
        this, MotionKind::kHover, 1.0, kHoverGrow,
        [this](const QVariant& value) {
          scale_ = value.toDouble();
          emit Changed();
        });
  }
  emit Changed();
}

double HandleHover::ScaleOf(GrabKind kind, const QString& key) const {
  assert(kind != GrabKind::kNone);
  assert(scale_ >= 1.0);
  const bool is_hovered = kind == kind_ && key == key_;
  return is_hovered ? scale_ : 1.0;
}

}  // namespace snapper
