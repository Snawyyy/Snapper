#include "ui/refusal_shaker.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QToolTip>

#include <cassert>
#include <cstddef>

#include "ui/motion.h"

namespace snapper {
namespace {

// Progress and offset (in units of kShakePx) of each jerk.
struct Jerk final {
  double at;
  double offset;
};
constexpr Jerk kJerks[] = {{0.0, 0.0},  {0.2, -1.0}, {0.4, 1.0},
                           {0.6, -0.75}, {0.8, 0.5},  {1.0, 0.0}};
constexpr size_t kJerkCount = sizeof(kJerks) / sizeof(kJerks[0]);

}  // namespace

double ShakeOffsetPx(double progress) {
  assert(kJerkCount >= 2);
  assert(kJerks[0].at == 0.0);
  const bool is_outside = progress <= 0.0 || progress >= 1.0;
  if (is_outside) {
    return 0.0;
  }
  for (size_t i = 1; i < kJerkCount; ++i) {
    const bool is_in_leg = progress <= kJerks[i].at;
    if (is_in_leg) {
      const Jerk& from = kJerks[i - 1];
      const Jerk& to = kJerks[i];
      const double along = (progress - from.at) / (to.at - from.at);
      return kShakePx * (from.offset + (to.offset - from.offset) * along);
    }
  }
  return 0.0;
}

QString RefusalReason(const QWidget* widget) {
  assert(widget != nullptr);
  constexpr int kMaxDepth = 32;
  const QWidget* at = widget;
  for (int depth = 0; depth < kMaxDepth && at != nullptr; ++depth) {
    const bool has_reason = !at->toolTip().isEmpty();
    if (has_reason) {
      return at->toolTip();
    }
    at = at->parentWidget();
  }
  assert(at == nullptr || at != widget);
  return QObject::tr("Not available right now");
}

RefusalShaker::RefusalShaker(MotionManager* motion) : motion_(motion) {
  assert(motion_ != nullptr);
  qApp->installEventFilter(this);
  assert(target_.isNull());
}

bool RefusalShaker::eventFilter(QObject* watched, QEvent* event) {
  assert(watched != nullptr);
  assert(event != nullptr);
  const QEvent::Type type = event->type();
  const bool is_press = type == QEvent::MouseButtonPress;
  const bool is_release = type == QEvent::MouseButtonRelease;
  auto* widget =
      is_press || is_release ? qobject_cast<QWidget*>(watched) : nullptr;
  const bool is_widget = widget != nullptr;
  const bool is_refused = is_widget && !widget->isEnabled();
  const bool is_shake = is_refused && is_press;
  // The reason waits for the release: any mouse event closes a tip.
  const bool is_say = is_refused && is_release;
  if (is_shake) {
    Shake(widget);
  }
  if (is_say) {
    const auto* mouse = static_cast<QMouseEvent*>(event);
    QToolTip::showText(mouse->globalPosition().toPoint(),
                       RefusalReason(widget), widget);
  }
  return false;
}

void RefusalShaker::Settle() {
  assert(motion_ != nullptr);
  assert(kJerkCount >= 2);
  // Home first: stopping the animation forgets the target.
  const bool is_shaking = !target_.isNull();
  if (is_shaking) {
    target_->move(origin_);
  }
  if (animation_) {
    animation_->stop();
  }
  target_.clear();
}

void RefusalShaker::Shake(QWidget* widget) {
  assert(widget != nullptr);
  assert(motion_ != nullptr);
  Settle();
  target_ = widget;
  origin_ = widget->pos();
  animation_ = motion_->Animate(
      this, MotionKind::kMove, 0.0, 1.0, [this](const QVariant& value) {
        const bool is_live = !target_.isNull();
        if (is_live) {
          const double dx = ShakeOffsetPx(value.toDouble());
          target_->move(origin_.x() + qRound(dx), origin_.y());
        }
      });
  // Done shaking: the widget is home, so forget it before a relayout
  // moves it and a later Settle would drag it back.
  const bool is_instant = animation_.isNull();
  if (is_instant) {
    target_.clear();
    return;
  }
  connect(animation_, &QVariantAnimation::finished, this,
          [this] { target_.clear(); });
}

}  // namespace snapper
