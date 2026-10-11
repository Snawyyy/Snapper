#include "ui/fader.h"

#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/scroll_guard.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kCapWidth = 10.0;
constexpr double kGrooveHeight = 6.0;
// Shift slows the cap this many times.
constexpr double kFineDivisor = 10.0;
constexpr int kWheelUnit = 120;
// A press changes nothing until the mouse moves this far.
constexpr double kStartPx = 3.0;

}  // namespace

Fader::Fader(QDoubleSpinBox* box, LiveEdit* live, double shown_high,
             double neutral, QWidget* parent)
    : QWidget(parent),
      box_(box),
      drag_({box}, live),
      shown_high_(shown_high),
      neutral_(neutral) {
  assert(box_ != nullptr);
  assert(shown_high_ > box_->minimum());
  setFixedHeight(sizeHint().height());
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setCursor(Qt::PointingHandCursor);
  setToolTip(tr("Drag to set. Shift for fine, wheel to step, double click "
                "to reset, Escape cancels."));
  // The field shows a change made without a signal by its text.
  const QLineEdit* text = box_->findChild<QLineEdit*>();
  assert(text != nullptr);
  connect(text, &QLineEdit::textChanged, this, [this] { update(); });
}

double Fader::XOf(double value) const {
  assert(shown_high_ > box_->minimum());
  assert(width() > kCapWidth);
  const double span = shown_high_ - box_->minimum();
  const double t = std::clamp((value - box_->minimum()) / span, 0.0, 1.0);
  return kCapWidth / 2 + t * (width() - kCapWidth);
}

double Fader::ValueAt(double x) const {
  assert(width() > kCapWidth);
  assert(shown_high_ > box_->minimum());
  const double t =
      std::clamp((x - kCapWidth / 2) / (width() - kCapWidth), 0.0, 1.0);
  return box_->minimum() + t * (shown_high_ - box_->minimum());
}

void Fader::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(width() > kCapWidth);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const bool is_on = box_->isEnabled();
  const double middle = height() / 2.0;
  const QRectF groove(kCapWidth / 2, middle - kGrooveHeight / 2,
                      width() - kCapWidth, kGrooveHeight);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(theme::kField);
  painter.drawRect(groove);
  const double cap_x = XOf(box_->value());
  if (is_on) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::kPick);
    painter.drawRect(QRectF(groove.left() + 1, groove.top() + 1,
                            cap_x - groove.left() - 1,
                            kGrooveHeight - 2));
  }
  // A tick at the neutral value.
  const double tick_x = XOf(neutral_);
  painter.setPen(QPen(theme::kFaceLight, 1.0));
  painter.drawLine(QPointF(tick_x, middle - 8), QPointF(tick_x, middle - 5));
  const QRectF cap(cap_x - kCapWidth / 2, 2, kCapWidth, height() - 4.0);
  const bool is_lit = is_on && (drag_.is_active() || is_hot_);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(is_lit ? theme::kFaceLight : theme::kFace);
  painter.drawRect(cap);
  painter.setPen(QPen(is_on ? theme::kHandle : theme::kTextOff, 1.0));
  painter.drawLine(QPointF(cap_x, cap.top() + 3),
                   QPointF(cap_x, cap.bottom() - 3));
}

void Fader::Drag(const QMouseEvent* event) {
  assert(event != nullptr);
  assert(drag_.is_active());
  const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
  const double span = shown_high_ - box_->minimum();
  const double moved = (event->position().x() - press_x_) /
                       (width() - kCapWidth) * span;
  const double value = is_fine ? drag_.start(0) + moved / kFineDivisor
                               : ValueAt(event->position().x());
  drag_.Set(0, value);
  update();
}

void Fader::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!drag_.is_active());
  const bool can_drag =
      event->button() == Qt::LeftButton && drag_.is_enabled();
  if (!can_drag) {
    QWidget::mousePressEvent(event);
    return;
  }
  // Nothing changes until the mouse moves a few pixels, so a double
  // click resets cleanly instead of jumping the cap first.
  is_pressed_ = true;
  press_x_ = event->position().x();
  setFocus();
}

void Fader::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(press_x_));
  const bool is_far = is_pressed_ && !drag_.is_active() &&
                      std::abs(event->position().x() - press_x_) >= kStartPx;
  if (is_far) {
    drag_.Begin();
    grabKeyboard();
  }
  const bool is_dragging = drag_.is_active();
  if (is_dragging) {
    Drag(event);
  }
}

void Fader::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(press_x_));
  is_pressed_ = false;
  releaseKeyboard();
  drag_.Finish();
  update();
}

void Fader::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(std::isfinite(neutral_));
  const bool can_reset = drag_.is_enabled();
  if (can_reset) {
    drag_.SetAll({neutral_});
  }
}

void Fader::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  // Half way through scrolling the panel the wheel belongs to it.
  const bool can_step = drag_.is_enabled() && !IsMidScroll(this);
  if (!can_step) {
    event->ignore();
    return;
  }
  const double notches = event->angleDelta().y() / double(kWheelUnit);
  const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
  const double step = box_->singleStep() * (is_fine ? 0.2 : 1.0);
  // Ticks in a row are one undo step.
  const auto burst = drag_.Burst();
  drag_.SetAll({drag_.value(0) + notches * step});
  event->accept();
}

void Fader::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  const bool is_cancel = event->key() == Qt::Key_Escape && drag_.is_active();
  if (!is_cancel) {
    QWidget::keyPressEvent(event);
    return;
  }
  is_pressed_ = false;
  releaseKeyboard();
  drag_.Cancel();
  update();
}

void Fader::enterEvent(QEnterEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  is_hot_ = true;
  update();
}

void Fader::leaveEvent(QEvent* event) {
  assert(event != nullptr);
  assert(box_ != nullptr);
  is_hot_ = false;
  update();
}

}  // namespace snapper
