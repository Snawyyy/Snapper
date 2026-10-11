#include "ui/angle_dial.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>

#include <cassert>
#include <cmath>
#include <numbers>

#include "base/dial_angle.h"
#include "ui/live_edit.h"
#include "ui/theme.h"

namespace snapper {

AngleDial::AngleDial(QDoubleSpinBox* box, LiveEdit* live, QWidget* parent)
    : QWidget(parent), box_(box), live_(live) {
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
  start_ = box_->value();
  is_turning_ = true;
  setFocus();
  grabKeyboard();
  Turn(event);
}

void AngleDial::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(box_->value()));
  if (is_turning_) {
    Turn(event);
  }
}

void AngleDial::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(box_->value()));
  const bool was_turning = is_turning_;
  is_turning_ = false;
  releaseKeyboard();
  update();
  if (was_turning) {
    emit box_->editingFinished();
  }
}

void AngleDial::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(start_));
  const bool is_cancel = event->key() == Qt::Key_Escape && is_turning_;
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
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
