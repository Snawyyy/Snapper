#include "ui/hover_fade.h"

#include <QEvent>
#include <QWidget>

#include <cassert>

#include "ui/motion.h"
#include "ui/snapper_style.h"

namespace snapper {

HoverFade::HoverFade(QWidget* target, MotionManager* motion)
    : QObject(target), target_(target), motion_(motion) {
  assert(target != nullptr);
  assert(motion != nullptr);
  hover_.property = kHoverProperty;
  press_.property = kPressProperty;
  target_->setProperty(kHoverProperty, 0.0);
  target_->setProperty(kPressProperty, 0.0);
  target_->installEventFilter(this);
}

bool HoverFade::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched == target_);
  const QEvent::Type type = event->type();
  const bool is_enter = type == QEvent::Enter;
  const bool is_leave = type == QEvent::Leave;
  const bool is_press = type == QEvent::MouseButtonPress;
  const bool is_release = type == QEvent::MouseButtonRelease;
  if (is_enter) {
    EaseTo(&hover_, MotionKind::kHover, 1.0);
  }
  if (is_leave) {
    EaseTo(&hover_, MotionKind::kHover, 0.0);
  }
  if (is_press) {
    EaseTo(&press_, MotionKind::kPress, 1.0);
  }
  if (is_release) {
    EaseTo(&press_, MotionKind::kPop, 0.0);
  }
  return false;
}

void HoverFade::EaseTo(Level* level, MotionKind kind, double to) {
  assert(level != nullptr);
  assert(motion_ != nullptr);
  const bool is_running = !level->animation.isNull();
  if (is_running) {
    level->animation->stop();
  }
  // Off holds a press still: a squash that never moves is no state.
  const bool is_still = kind != MotionKind::kHover &&
                        motion_->duration(kind) == 0;
  if (is_still) {
    return;
  }
  const auto set_level = [this, level](const QVariant& value) {
    level->value = value.toDouble();
    target_->setProperty(level->property, level->value);
    target_->update();
  };
  level->animation =
      motion_->Animate(this, kind, level->value, to, set_level);
}

}  // namespace snapper
