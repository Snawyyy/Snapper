#include "ui/wobble_blob.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/motion.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr int kFrameMs = 33;

void PaintJelly(QPainter* painter, const QRectF& box, double swing) {
  assert(painter != nullptr);
  assert(box.isValid());
  const double squash = 1.0 + 0.3 * swing;
  const double rx = box.width() * 0.22 * squash;
  const double ry = box.height() * 0.26 / squash;
  painter->drawEllipse(QPointF(box.center().x(), box.bottom() - ry - 4),
                       rx, ry);
}

void PaintDrag(QPainter* painter, const QRectF& box, double swing) {
  assert(painter != nullptr);
  assert(box.isValid());
  constexpr int kCoils = 7;
  const QPointF dot(box.center().x() + box.width() * 0.25 * swing,
                    box.center().y());
  const double left = box.left() + 4;
  QPainterPath spring(QPointF(left, dot.y()));
  for (int i = 1; i <= kCoils; ++i) {
    const double along = static_cast<double>(i) / (kCoils + 1);
    spring.lineTo(left + (dot.x() - left) * along,
                  dot.y() + (i % 2 == 0 ? -5.0 : 5.0));
  }
  spring.lineTo(dot);
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(spring);
  painter->setBrush(theme::kText);
  painter->drawEllipse(dot, 6.0, 6.0);
}

void PaintCloth(QPainter* painter, const QRectF& box, double swing) {
  assert(painter != nullptr);
  assert(box.isValid());
  const QRectF sheet = box.adjusted(18, 6, -18, -6);
  const double shift = sheet.width() * 0.4 * swing;
  QPainterPath path(sheet.topLeft());
  path.lineTo(sheet.topRight());
  path.cubicTo(sheet.topRight() + QPointF(shift * 0.3, sheet.height() * 0.4),
               sheet.bottomRight() + QPointF(shift, -sheet.height() * 0.3),
               sheet.bottomRight() + QPointF(shift, 0));
  path.lineTo(sheet.bottomLeft() + QPointF(shift, 0));
  path.cubicTo(sheet.bottomLeft() + QPointF(shift, -sheet.height() * 0.3),
               sheet.topLeft() + QPointF(shift * 0.3, sheet.height() * 0.4),
               sheet.topLeft());
  painter->drawPath(path);
}

}  // namespace

double WobbleAt(const Feel& feel, double seconds) {
  assert(std::isfinite(feel.firm + feel.heavy));
  assert(std::isfinite(seconds));
  const double t = std::fmod(std::max(seconds, 0.0), kWobbleLoopS);
  const double amplitude = 0.5 + 0.5 * std::clamp(feel.heavy, 0.0, 1.0);
  const double decay = 5.0 - 3.5 * std::clamp(feel.heavy, 0.0, 1.0);
  const double rate = 1.2 + 3.0 * std::clamp(feel.firm, 0.0, 1.0);
  return amplitude * std::exp(-decay * t) * std::cos(kTwoPi * rate * t);
}

WobbleBlob::WobbleBlob(MotionManager* motion, QWidget* parent)
    : QWidget(parent), motion_(motion) {
  assert(motion != nullptr);
  assert(kFrameMs > 0);
  setObjectName("feel_preview");
  setFixedSize(sizeHint());
  timer_.setInterval(kFrameMs);
  connect(&timer_, &QTimer::timeout, this, [this] { Step(); });
  connect(motion_, &MotionManager::ModeChanged, this,
          [this] { Sync(); });
}

QSize WobbleBlob::sizeHint() const {
  assert(kFrameMs > 0);
  assert(kWobbleLoopS > 0.0);
  return {96, 80};
}

void WobbleBlob::SetLook(Material material, const Feel& feel) {
  assert(std::isfinite(feel.firm + feel.heavy));
  assert(motion_ != nullptr);
  assert(static_cast<int>(material) < kMaterialCount);
  material_ = material;
  feel_ = feel;
  update();
}

void WobbleBlob::Sync() {
  assert(motion_ != nullptr);
  assert(kFrameMs > 0);
  const bool is_moving = isVisible() && isEnabled() &&
                         motion_->mode() != MotionMode::kOff;
  if (is_moving) {
    timer_.start();
    return;
  }
  timer_.stop();
  seconds_ = 0.0;
  update();
}

void WobbleBlob::Step() {
  assert(kFrameMs > 0);
  assert(kWobbleLoopS > 0.0);
  seconds_ = std::fmod(seconds_ + kFrameMs / 1000.0, kWobbleLoopS);
  update();
}

void WobbleBlob::showEvent(QShowEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QWidget::showEvent(event);
  Sync();
}

void WobbleBlob::hideEvent(QHideEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QWidget::hideEvent(event);
  Sync();
}

void WobbleBlob::changeEvent(QEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QWidget::changeEvent(event);
  const bool is_enable = event->type() == QEvent::EnabledChange;
  if (is_enable) {
    Sync();
  }
}

void WobbleBlob::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(motion_ != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
  painter.fillRect(box, theme::kField);
  painter.setPen(QPen(isEnabled() ? theme::kText : theme::kTextOff, 1.5));
  painter.setBrush(isEnabled() ? theme::kPick : theme::kFace);
  const double swing = WobbleAt(feel_, seconds_);
  switch (material_) {
    case Material::kDrag:
      PaintDrag(&painter, box, swing);
      break;
    case Material::kCloth:
      PaintCloth(&painter, box, swing);
      break;
    case Material::kStill:
    case Material::kJelly:
      PaintJelly(&painter, box, swing);
      break;
  }
}

}  // namespace snapper
