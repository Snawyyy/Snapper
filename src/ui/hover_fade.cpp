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
  target_->setProperty(kHoverProperty, 0.0);
  target_->installEventFilter(this);
}

bool HoverFade::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched == target_);
  const bool is_enter = event->type() == QEvent::Enter;
  const bool is_leave = event->type() == QEvent::Leave;
  if (is_enter) {
    FadeTo(1.0);
  }
  if (is_leave) {
    FadeTo(0.0);
  }
  return false;
}

void HoverFade::FadeTo(double to) {
  assert(to == 0.0 || to == 1.0);
  assert(motion_ != nullptr);
  const bool is_running = !animation_.isNull();
  if (is_running) {
    animation_->stop();
  }
  const auto set_level = [this](const QVariant& value) {
    level_ = value.toDouble();
    target_->setProperty(kHoverProperty, level_);
    target_->update();
  };
  animation_ = motion_->Animate(this, MotionKind::kHover, level_, to,
                                set_level);
}

}  // namespace snapper
