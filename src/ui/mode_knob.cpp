#include "ui/mode_knob.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QRadialGradient>
#include <QWheelEvent>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "ui/deck_paint.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr double kKnobRadius = 20.0;
// The settings spread over this many degrees, centred on straight up.
constexpr double kSweep = 110.0;
// From the knob's centre to the middle of a setting's name.
constexpr double kLabelReach = 44.0;
constexpr int kRidges = 24;
constexpr int kMaxModes = 8;

QPointF Along(QPointF from, double degrees, double reach) {
  const double radians = degrees * std::numbers::pi / 180.0;
  return from + QPointF(std::sin(radians) * reach,
                        -std::cos(radians) * reach);
}

}  // namespace

ModeKnob::ModeKnob(const QStringList& modes) : modes_(modes) {
  assert(modes_.size() >= 2);
  assert(modes_.size() <= kMaxModes);
  setCursor(Qt::PointingHandCursor);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ModeKnob::SetValue(int value) {
  assert(value >= 0);
  assert(value < modes_.size());
  value_ = value;
  update();
}

QSize ModeKnob::sizeHint() const {
  const int height = static_cast<int>(kLabelReach + kKnobRadius) +
                     fontMetrics().height() + 8;
  assert(height > 0);
  return QSize(static_cast<int>(2 * kLabelReach) + 80, height);
}

QRectF ModeKnob::KnobRect() const {
  assert(width() >= 0);
  const QPointF centre(width() / 2.0, height() - kKnobRadius - 3.0);
  return QRectF(centre - QPointF(kKnobRadius, kKnobRadius),
                QSizeF(2 * kKnobRadius, 2 * kKnobRadius));
}

double ModeKnob::AngleOf(int i) const {
  assert(i >= 0 && i < modes_.size());
  assert(modes_.size() >= 2);
  return -kSweep / 2.0 +
         kSweep * i / static_cast<double>(modes_.size() - 1);
}

QRectF ModeKnob::LabelRect(int i) const {
  assert(i >= 0 && i < modes_.size());
  const QFontMetrics metrics(font());
  const QPointF at = Along(KnobRect().center(), AngleOf(i), kLabelReach);
  const double wide = metrics.horizontalAdvance(modes_[i]) + 8.0;
  const double tall = metrics.height() + 2.0;
  return QRectF(at.x() - wide / 2.0, at.y() - tall / 2.0, wide, tall);
}

void ModeKnob::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(value_ >= 0 && value_ < modes_.size());
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);
  const QRectF knob = KnobRect();
  const QPointF centre = knob.center();
  // The printed settings, each with a tick on the scale; the one the
  // pointer is on lights red, as what is on does everywhere.
  for (int i = 0; i < modes_.size(); ++i) {
    const bool is_on = i == value_;
    painter.setPen(QPen(is_on ? theme::kPick : theme::kTextOff, 1.5));
    painter.drawLine(Along(centre, AngleOf(i), kKnobRadius + 4.0),
                     Along(centre, AngleOf(i), kKnobRadius + 10.0));
    painter.setPen(is_on ? theme::kText : theme::kTextOff);
    painter.drawText(LabelRect(i), Qt::AlignCenter, modes_[i]);
  }
  // The knob: ridged black plastic with a lit top, a cream pointer.
  QRadialGradient body(centre - QPointF(0, kKnobRadius / 2.0),
                       kKnobRadius * 1.6);
  body.setColorAt(0.0, deck::kPlasticLight.lighter(150));
  body.setColorAt(1.0, deck::kPlasticDark);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(body);
  painter.drawEllipse(knob);
  painter.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
  for (int r = 0; r < kRidges; ++r) {
    const double degrees = 360.0 * r / kRidges;
    painter.drawLine(Along(centre, degrees, kKnobRadius - 3.0),
                     Along(centre, degrees, kKnobRadius - 0.5));
  }
  painter.setPen(QPen(deck::kLabel, 3.0, Qt::SolidLine, Qt::RoundCap));
  painter.drawLine(Along(centre, AngleOf(value_), 4.0),
                   Along(centre, AngleOf(value_), kKnobRadius - 5.0));
}

void ModeKnob::Turn(int value) {
  assert(modes_.size() >= 2);
  const int clamped = std::clamp(value, 0, static_cast<int>(modes_.size()) - 1);
  const bool is_new = clamped != value_;
  if (!is_new) {
    return;
  }
  value_ = clamped;
  update();
  emit Picked(value_);
}

void ModeKnob::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(!modes_.isEmpty());
  const QPointF at = event->position();
  for (int i = 0; i < modes_.size(); ++i) {
    const bool is_label = LabelRect(i).adjusted(-4, -4, 4, 4).contains(at);
    if (is_label) {
      Turn(i);
      return;
    }
  }
  const bool is_knob = KnobRect().contains(at);
  if (is_knob) {
    // Round and round: past the last setting it comes back to the first.
    Turn((value_ + 1) % static_cast<int>(modes_.size()));
  }
}

void ModeKnob::wheelEvent(QWheelEvent* event) {
  assert(event != nullptr);
  assert(!modes_.isEmpty());
  const int steps = event->angleDelta().y() > 0 ? -1 : 1;
  Turn(value_ + steps);
  event->accept();
}

}  // namespace snapper
