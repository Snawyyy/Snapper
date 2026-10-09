#include "ui/drag_feedback.h"

#include <QColor>
#include <QFontMetricsF>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/drag_text.h"
#include "ui/grab.h"
#include "ui/motion.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kChipPadX = 6.0;
constexpr double kChipPadY = 3.0;
constexpr double kChipRadius = 4.0;
constexpr int kGhostAlpha = 90;
constexpr int kChipAlpha = 230;
constexpr int kFullAlpha = 255;
// The flash ring and the settle ring open this far past a handle.
constexpr double kFlashReach = 14.0;
constexpr double kPopReach = 10.0;

QColor WithAlpha(QColor color, double fraction) {
  assert(fraction >= -1.0 && fraction <= 2.0);
  assert(color.isValid());
  color.setAlpha(static_cast<int>(
      kFullAlpha * std::clamp(fraction, 0.0, 1.0)));
  return color;
}

}  // namespace

DragFeedback::DragFeedback(MotionManager* motion, QObject* parent)
    : QObject(parent), motion_(motion) {
  assert(motion_ != nullptr);
  assert(!is_active_);
  rest_timer_.setSingleShot(true);
  rest_timer_.setInterval(kRestMs);
  connect(&rest_timer_, &QTimer::timeout, this, [this] { StartFade(); });
}

void DragFeedback::Pulse(QPointF cursor, const QString& text) {
  assert(std::isfinite(cursor.x()) && std::isfinite(cursor.y()));
  assert(!text.isEmpty());
  if (pulse_animation_) {
    pulse_animation_->stop();
  }
  pulse_text_ = text;
  pulse_at_ = cursor;
  pulse_ = 1.0;
  rest_timer_.start();
  emit Changed();
}

void DragFeedback::StartFade() {
  assert(motion_ != nullptr);
  assert(pulse_ > 0.0);
  pulse_animation_ = motion_->Animate(
      this, MotionKind::kCap, 1.0, 0.0, [this](const QVariant& value) {
        pulse_ = std::max(0.0, value.toDouble());
        emit Changed();
      });
}

void DragFeedback::Update(QPointF start, QPointF cursor, const QString& text,
                          bool is_snapped) {
  assert(std::isfinite(start.x()) && std::isfinite(start.y()));
  assert(std::isfinite(cursor.x()) && std::isfinite(cursor.y()));
  const bool is_first = !is_active_;
  if (is_first) {
    ghost_ = start;
    was_snapped_ = false;
  }
  is_active_ = true;
  // A new snap target flashes; staying on the same one does not.
  const bool is_new_snap = is_snapped && (!was_snapped_ || text != text_);
  cursor_ = cursor;
  text_ = text;
  was_snapped_ = is_snapped;
  if (is_new_snap) {
    StartFlash();
  }
  emit Changed();
}

void DragFeedback::StartFlash() {
  assert(motion_ != nullptr);
  assert(is_active_);
  if (flash_animation_) {
    flash_animation_->stop();
  }
  flash_ = 1.0;
  flash_animation_ = motion_->Animate(
      this, MotionKind::kPress, 1.0, 0.0, [this](const QVariant& value) {
        flash_ = value.toDouble();
        emit Changed();
      });
}

void DragFeedback::Settle() {
  assert(motion_ != nullptr);
  const bool was_active = is_active_;
  if (!was_active) {
    return;
  }
  settle_at_ = cursor_;
  Cancel();
  StartSettle();
  assert(!is_active_);
}

void DragFeedback::StartSettle() {
  assert(motion_ != nullptr);
  assert(!is_active_);
  settle_ = 1.0;
  settle_animation_ = motion_->Animate(
      this, MotionKind::kPop, 1.0, 0.0, [this](const QVariant& value) {
        settle_ = value.toDouble();
        emit Changed();
      });
}

void DragFeedback::Cancel() {
  assert(motion_ != nullptr);
  if (flash_animation_) {
    flash_animation_->stop();
  }
  if (settle_animation_) {
    settle_animation_->stop();
  }
  if (pulse_animation_) {
    pulse_animation_->stop();
  }
  rest_timer_.stop();
  pulse_ = 0.0;
  is_active_ = false;
  was_snapped_ = false;
  flash_ = 0.0;
  settle_ = 0.0;
  text_.clear();
  emit Changed();
  assert(!is_active_ && text_.isEmpty());
}

void DragFeedback::Paint(QPainter* painter, const QRectF& bounds) const {
  assert(painter != nullptr);
  assert(bounds.isValid());
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  PaintPop(painter);
  if (is_active_) {
    painter->setPen(QPen(WithAlpha(theme::kText, kGhostAlpha / 255.0), 1.0,
                         Qt::DashLine));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(ghost_, kHandleDraw, kHandleDraw);
    PaintChip(painter, bounds, cursor_, text_);
  }
  const bool is_pulsing = pulse_ > 0.0 && !is_active_;
  if (is_pulsing) {
    painter->setOpacity(std::min(pulse_, 1.0));
    PaintChip(painter, bounds, pulse_at_, pulse_text_);
  }
  painter->restore();
}

void DragFeedback::PaintChip(QPainter* painter, const QRectF& bounds,
                             QPointF cursor, const QString& text) const {
  assert(painter != nullptr);
  assert(!text.isEmpty());
  const QFontMetricsF metrics(painter->font());
  const QSizeF size(metrics.horizontalAdvance(text) + 2.0 * kChipPadX,
                    metrics.height() + 2.0 * kChipPadY);
  const QRectF chip = ChipRect(cursor, size, bounds);
  QColor border = theme::kFaceLight;
  border = flash_ > 0.0 ? theme::kHandle : border;
  painter->setPen(QPen(border, 1.0 + flash_));
  painter->setBrush(WithAlpha(theme::kShadow, kChipAlpha / 255.0));
  painter->drawRoundedRect(chip, kChipRadius, kChipRadius);
  painter->setPen(theme::kText);
  painter->drawText(chip, Qt::AlignCenter, text);
  const bool is_flashing = flash_ > 0.0 && is_active_;
  if (is_flashing) {
    const double reach = kHandleDraw + kFlashReach * (1.0 - flash_);
    painter->setPen(QPen(WithAlpha(theme::kHandle, flash_), 2.0));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(cursor, reach, reach);
  }
}

void DragFeedback::PaintPop(QPainter* painter) const {
  assert(painter != nullptr);
  assert(std::isfinite(settle_));
  const bool is_popping = settle_ != 0.0;
  if (!is_popping) {
    return;
  }
  const double reach = kHandleDraw + kPopReach * settle_;
  painter->setPen(QPen(WithAlpha(theme::kHandle, settle_), 2.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawEllipse(settle_at_, reach, reach);
}

}  // namespace snapper
