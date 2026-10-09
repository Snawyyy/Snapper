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
  ShrinkLeft();
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

void HandleHover::ShrinkLeft() {
  assert(motion_ != nullptr);
  assert(scale_ >= 1.0);
  if (out_animation_) {
    out_animation_->stop();
  }
  out_kind_ = kind_;
  out_key_ = key_;
  out_scale_ = scale_;
  const bool is_leaving = kind_ != GrabKind::kNone && scale_ > 1.0;
  if (!is_leaving) {
    out_kind_ = GrabKind::kNone;
    return;
  }
  out_animation_ = motion_->Animate(
      this, MotionKind::kHover, scale_, 1.0, [this](const QVariant& value) {
        out_scale_ = value.toDouble();
        const bool is_done = out_scale_ <= 1.0;
        if (is_done) {
          out_kind_ = GrabKind::kNone;
        }
        emit Changed();
      });
}

double HandleHover::ScaleOf(GrabKind kind, const QString& key) const {
  assert(kind != GrabKind::kNone);
  assert(scale_ >= 1.0);
  const bool is_hovered = kind == kind_ && key == key_;
  const bool is_leaving = kind == out_kind_ && key == out_key_;
  return is_hovered ? scale_ : is_leaving ? out_scale_ : 1.0;
}

}  // namespace snapper
