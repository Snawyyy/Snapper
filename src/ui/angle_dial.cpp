#include "ui/angle_dial.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>
#include <numbers>

#include "base/dial_angle.h"
#include "ui/live_edit.h"
#include "ui/scroll_guard.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// From this size a dial shows its snap ticks.
constexpr int kTickedDiameter = 36;
// A press turns nothing until the mouse moves this far.
constexpr int kStartPx = 3;
// Degrees a wheel notch turns, and with Shift.
constexpr double kWheelDegrees = 5.0;
constexpr double kFineWheelDegrees = 1.0;
constexpr int kWheelUnit = 120;

// A tick every kDialSnap degrees, longer every quarter turn: what Shift
// snaps to, so the dial reads like a knob with detents.
void PaintTicks(QPainter* painter, QPointF middle, double radius) {
  assert(painter != nullptr);
  assert(radius > 4.0);
  painter->setPen(QPen(theme::kFaceLight, 1.0));
  const int count = static_cast<int>(360.0 / kDialSnap);
  for (int i = 1; i < count; ++i) {
    const double radians = i * kDialSnap * std::numbers::pi / 180.0;
    const bool is_quarter = i % (count / 4) == 0;
    const double inner = radius - (is_quarter ? 5.0 : 3.0);
    const QPointF direction(std::sin(radians), -std::cos(radians));
    painter->drawLine(middle + direction * inner,
                      middle + direction * radius);
  }
}

}  // namespace

AngleDial::AngleDial(QDoubleSpinBox* box, LiveEdit* live, QWidget* parent,
                     int diameter)
    : QWidget(parent), box_(box), live_(live), diameter_(diameter) {
  assert(box_ != nullptr);
  assert(sizeHint().width() > 0);
  setFixedSize(sizeHint());
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::PointingHandCursor);
  setToolTip(tr("Drag to turn. Shift snaps to 15 degrees; Escape cancels."));
  // The field shows a change made without a signal by its text.
  const QLineEdit* text = box_->findChild<QLineEdit*>();
  assert(text != nullptr);
  connect(text, &QLineEdit::textChanged, this, [this] { update(); });
}

void AngleDial::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() > 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QPointF middle(width() / 2.0, height() / 2.0);
  const double radius = std::min(width(), height()) / 2.0 - 2.0;
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(box_->isEnabled() ? theme::kFaceDark : theme::kFace);
  painter.drawEllipse(middle, radius, radius);
  // A tick at 0, so a turned needle reads against it.
  painter.setPen(QPen(theme::kFaceLight, 1.0));
  painter.drawLine(middle + QPointF(0, -radius),
                   middle + QPointF(0, -radius + 3));
  const bool has_ticks = diameter_ >= kTickedDiameter;
  if (has_ticks) {
    PaintTicks(&painter, middle, radius);
  }
  const double radians = box_->value() * std::numbers::pi / 180.0;
  const QPointF tip = middle + QPointF(std::sin(radians), -std::cos(radians)) *
                                   (radius - 2.0);
  painter.setPen(QPen(is_turning_ ? theme::kPick.lighter(130) : theme::kPick,
                      2.0, Qt::SolidLine, Qt::RoundCap));
  painter.drawLine(middle, tip);
}

void AngleDial::Turn(const QMouseEvent* event) {
  assert(event != nullptr);
  assert(is_turning_);
  const QPointF centre(width() / 2.0, height() / 2.0);
  const bool is_snapping = event->modifiers().testFlag(Qt::ShiftModifier);
  const double angle =
      DialAngle(event->position() - centre, box_->value(), is_snapping);
  box_->setValue(angle);
  update();
}

void AngleDial::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!is_turning_);
  const bool is_left = event->button() == Qt::LeftButton;
  const bool can_turn = is_left && box_->isEnabled();
  if (!can_turn) {
    QWidget::mousePressEvent(event);
    return;
  }
  is_pressed_ = true;
  press_at_ = event->position();
  setFocus();
}

void AngleDial::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(box_->value()));
  const bool is_far =
      is_pressed_ && !is_turning_ &&
      (event->position() - press_at_).manhattanLength() >= kStartPx;
  if (is_far) {
    start_ = box_->value();
    is_turning_ = true;
    grabKeyboard();
  }
  if (is_turning_) {
    Turn(event);
  }
}

void AngleDial::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(box_->value()));
  const bool was_turning = is_turning_;
  is_pressed_ = false;
  is_turning_ = false;
  releaseKeyboard();
  update();
  if (was_turning) {
    emit box_->editingFinished();
  }
}

void AngleDial::SetAngle(double angle) {
  assert(std::isfinite(angle));
  assert(box_ != nullptr);
  box_->setValue(angle);
  emit box_->editingFinished();
  update();
}

void AngleDial::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!is_turning_);
  const bool can_reset = box_->isEnabled();
  if (can_reset) {
    SetAngle(0.0);
  }
}

void AngleDial::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(!is_turning_);
  // Half way through scrolling the panel the wheel belongs to it.
  const bool can_turn = box_->isEnabled() && !IsMidScroll(this);
  if (!can_turn) {
    event->ignore();
    return;
  }
  const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
  const double notches = event->angleDelta().y() / double(kWheelUnit);
  // Wheel up turns counterclockwise, like a knob (as on the stage).
  // Ticks in a row are one undo step.
  const auto burst = live_ != nullptr ? live_->Burst() : nullptr;
  SetAngle(box_->value() - notches * (is_fine ? kFineWheelDegrees
                                              : kWheelDegrees));
  event->accept();
}

void AngleDial::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(start_));
  const bool is_cancel = event->key() == Qt::Key_Escape && is_turning_;
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
  is_pressed_ = false;
  is_turning_ = false;
  releaseKeyboard();
  const bool has_live = live_ != nullptr;
  if (has_live) {
    live_->Cancel();
  }
  const QSignalBlocker quiet(box_);
  box_->setValue(start_);
  update();
}

}  // namespace snapper
