#include "ui/success_burst.h"

#include <QEvent>
#include <QPainter>

#include <cassert>
#include <cmath>
#include <numbers>

#include "ui/motion.h"
#include "ui/theme.h"
#include "ui/toast_manager.h"

namespace snapper {
namespace {

constexpr int kFullPixels = 24;
constexpr int kCalmPixels = 10;
constexpr double kPixel = 5.0;
constexpr double kReach = 140.0;

}  // namespace

SuccessBurst::SuccessBurst(ToastManager* toasts, MotionManager* motion,
                           QWidget* host)
    : QWidget(host), motion_(motion), host_(host) {
  assert(toasts != nullptr);
  assert(motion != nullptr && host != nullptr);
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_NoSystemBackground);
  host_->installEventFilter(this);
  setGeometry(host_->rect());
  raise();
  connect(toasts, &ToastManager::Cheered, this, [this] { Burst(); });
}

void SuccessBurst::Burst() {
  assert(motion_ != nullptr);
  assert(host_ != nullptr);
  setGeometry(host_->rect());
  raise();
  const bool is_running = !animation_.isNull();
  if (is_running) {
    animation_->stop();
  }
  animation_ = motion_->Animate(
      this, MotionKind::kCap, 0.0, 1.0, [this](const QVariant& value) {
        progress_ = value.toDouble();
        update();
      });
  const bool is_instant = animation_.isNull();
  if (is_instant) {
    // Off: nothing flies.
    progress_ = -1.0;
    return;
  }
  connect(animation_, &QVariantAnimation::finished, this, [this] {
    progress_ = -1.0;
    update();
  });
}

void SuccessBurst::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  const bool is_flying = progress_ >= 0.0;
  if (!is_flying) {
    return;
  }
  QPainter painter(this);
  const int count = motion_->mode() == MotionMode::kFull ? kFullPixels
                                                         : kCalmPixels;
  const QPointF origin(width() / 2.0, height() - 40.0);
  QColor colour = theme::kPick;
  colour.setAlphaF(static_cast<float>(std::max(0.0, 1.0 - progress_ * 0.7)));
  painter.setPen(Qt::NoPen);
  painter.setBrush(colour);
  for (int i = 0; i < count; ++i) {
    // Fanned up from the bottom, each a little farther than the last.
    const double angle = std::numbers::pi *
                         (1.1 + 0.8 * i / std::max(1, count - 1));
    const double far = kReach * progress_ * (0.6 + 0.4 * (i % 3) / 2.0);
    const QPointF at = origin + QPointF(std::cos(angle), std::sin(angle)) * far;
    painter.drawRect(QRectF(at - QPointF(kPixel / 2, kPixel / 2),
                            QSizeF(kPixel, kPixel)));
  }
}

bool SuccessBurst::eventFilter(QObject* watched, QEvent* event) {
  assert(event != nullptr);
  assert(watched != nullptr);
  const bool is_resize = watched == host_ && event->type() == QEvent::Resize;
  if (is_resize) {
    setGeometry(host_->rect());
  }
  return QWidget::eventFilter(watched, event);
}

}  // namespace snapper
