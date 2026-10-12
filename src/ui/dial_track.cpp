#include "ui/dial_track.h"

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "anim/smart_dial.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kSide = 10.0;
constexpr double kGrooveY = 12.0;
constexpr double kKnob = 6.0;
// How far a tick may be from a right click to count as hit.
constexpr double kTickGrab = 7.0;
// One wheel notch, and with Shift.
constexpr double kWheelStep = 0.05;
constexpr double kFineStep = 0.01;
// Shift drags this much slower.
constexpr double kFineDrag = 0.2;
constexpr int kWheelUnit = 120;

}  // namespace

DialTrack::DialTrack(QWidget* parent) : QWidget(parent) {
  setFocusPolicy(Qt::ClickFocus);
  setMouseTracking(false);
  setMinimumHeight(40);
  assert(parent != nullptr);
}

void DialTrack::Show(const SmartDial& dial, double value) {
  assert(std::isfinite(value));
  assert(dial.stops.size() <= static_cast<size_t>(kMaxDialStops));
  const bool is_same = dial == dial_ && value == value_;
  if (is_same) {
    return;
  }
  dial_ = dial;
  if (!is_dragging_) {
    value_ = std::clamp(value, 0.0, 1.0);
  }
  update();
}

double DialTrack::Left() const { return kSide; }

double DialTrack::Right() const {
  return std::max(kSide + 1.0, width() - kSide);
}

double DialTrack::XOf(double value) const {
  return Left() + (Right() - Left()) * value;
}

double DialTrack::ValueAt(double x, bool is_free) const {
  const double raw = std::clamp((x - Left()) / (Right() - Left()), 0.0, 1.0);
  return is_free ? raw : SnappedDialValue(dial_, raw);
}

int DialTrack::StopAt(double x) const {
  assert(std::isfinite(x));
  for (size_t i = 0; i < dial_.stops.size(); ++i) {
    const bool is_hit = std::abs(XOf(dial_.stops[i].at) - x) <= kTickGrab;
    if (is_hit) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

void DialTrack::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  // The sunken groove.
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(theme::kField);
  painter.drawRoundedRect(QRectF(QPointF(Left() - 3, kGrooveY - 3),
                                 QPointF(Right() + 3, kGrooveY + 3)),
                          3, 3);
  QFont small = font();
  small.setPointSizeF(std::max(6.0, small.pointSizeF() - 2.0));
  painter.setFont(small);
  for (size_t i = 0; i < dial_.stops.size(); ++i) {
    const DialStop& stop = dial_.stops[i];
    const double x = XOf(stop.at);
    const bool is_rest = static_cast<int>(i) == dial_.rest;
    painter.setPen(QPen(theme::kText, 1.0));
    painter.setBrush(is_rest ? theme::kText : Qt::NoBrush);
    painter.drawRect(QRectF(x - 2.0, kGrooveY + 5.0, 4.0, 6.0));
    const QRectF label(x - 40.0, kGrooveY + 12.0, 80.0, 14.0);
    painter.setPen(theme::kTextOff);
    painter.drawText(label, Qt::AlignHCenter | Qt::AlignTop, stop.name);
  }
  const QPointF knob(XOf(value_), kGrooveY);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(isEnabled() ? theme::kHandle : theme::kTextOff);
  painter.drawEllipse(knob, kKnob, kKnob);
}

void DialTrack::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool is_left = event->button() == Qt::LeftButton;
  if (!is_left) {
    QWidget::mousePressEvent(event);
    return;
  }
  is_dragging_ = true;
  press_x_ = event->position().x();
  emit Started();
  const bool is_free = event->modifiers().testFlag(Qt::ControlModifier);
  value_ = ValueAt(press_x_, is_free);
  press_value_ = value_;
  emit Turned(value_);
  update();
}

void DialTrack::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  if (!is_dragging_) {
    return;
  }
  const bool is_free = event->modifiers().testFlag(Qt::ControlModifier);
  const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
  const double x = event->position().x();
  const double fine_x = press_x_ + (x - press_x_) * kFineDrag;
  // Fine steps start from where the knob was pressed.
  const double at = is_fine ? XOf(press_value_) + (fine_x - press_x_) : x;
  value_ = ValueAt(at, is_free);
  emit Turned(value_);
  update();
}

void DialTrack::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  const bool was_dragging =
      is_dragging_ && event->button() == Qt::LeftButton;
  if (was_dragging) {
    is_dragging_ = false;
    emit Landed();
  }
}

void DialTrack::mouseDoubleClickEvent(QMouseEvent* event) {
  assert(event != nullptr);
  is_dragging_ = false;
  value_ = RestValue(dial_);
  emit Rested();
  update();
}

void DialTrack::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  const QPoint turn = event->angleDelta();
  const int delta = turn.y() != 0 ? turn.y() : turn.x();
  const double notches = delta / static_cast<double>(kWheelUnit);
  const bool is_fine = event->modifiers().testFlag(Qt::ShiftModifier);
  const bool is_free = event->modifiers().testFlag(Qt::ControlModifier);
  const double step = is_fine ? kFineStep : kWheelStep;
  const double raw = std::clamp(value_ + notches * step, 0.0, 1.0);
  value_ = is_free ? raw : SnappedDialValue(dial_, raw);
  emit Started();
  emit Turned(value_);
  emit Landed();
  update();
  event->accept();
}

void DialTrack::keyPressEvent(QKeyEvent* event) {
  assert(event != nullptr);
  const bool is_cancel = is_dragging_ && event->key() == Qt::Key_Escape;
  if (is_cancel) {
    is_dragging_ = false;
    emit Cancelled();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void DialTrack::contextMenuEvent(QContextMenuEvent* event) {
  assert(event != nullptr);
  emit MenuAsked(StopAt(event->pos().x()), event->globalPos());
  event->accept();
}

}  // namespace snapper
