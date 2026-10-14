#include "ui/color_wheel.h"

#include <QConicalGradient>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <numbers>

#include "ui/theme.h"

namespace snapper {
namespace {

// The widest the wheel gets, and the ring's width.
constexpr int kMostWide = 220;
constexpr double kRing = 16.0;
constexpr double kGap = 4.0;
constexpr double kMarker = 5.0;
constexpr int kHueStops = 6;

}  // namespace

ColorWheel::ColorWheel(QWidget* parent) : QWidget(parent) {
  assert(parent != nullptr);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setCursor(Qt::CrossCursor);
  setToolTip(tr("Drag on the ring for the hue and in the square for how "
                "strong and bright; letting go picks the color."));
  assert(hasHeightForWidth());
}

void ColorWheel::SetColor(const QColor& color) {
  assert(hue_ >= 0.0 && hue_ <= 1.0);
  const bool is_dragging = dragging_ != Part::kNone;
  const bool can_show = color.isValid() && !is_dragging;
  if (!can_show) {
    return;
  }
  const QColor hsv = color.toHsv();
  // Greys have no hue: the ring stays where it was.
  const double hue = hsv.hsvHueF();
  hue_ = hue >= 0.0 ? hue : hue_;
  saturation_ = hsv.hsvSaturationF();
  value_ = hsv.valueF();
  assert(hue_ >= 0.0 && hue_ <= 1.0);
  update();
}

QSize ColorWheel::sizeHint() const {
  assert(kMostWide > 0);
  assert(kRing > 0.0);
  return QSize(kMostWide, kMostWide);
}

int ColorWheel::heightForWidth(int width) const {
  assert(width >= 0);
  assert(kMostWide > 0);
  return std::min(width, kMostWide);
}

QPointF ColorWheel::Middle() const {
  assert(width() >= 0);
  assert(height() >= 0);
  return QPointF(width() / 2.0, height() / 2.0);
}

double ColorWheel::Outer() const {
  assert(kRing > 0.0);
  assert(width() >= 0);
  return std::max(kRing + 4.0, std::min(width(), height()) / 2.0 - 1.0);
}

double ColorWheel::Inner() const {
  assert(Outer() > kRing);
  assert(kGap >= 0.0);
  return Outer() - kRing;
}

QRectF ColorWheel::Square() const {
  const double half = (Inner() - kGap) / std::numbers::sqrt2;
  assert(half > 0.0);
  assert(Inner() > kGap);
  return QRectF(Middle() - QPointF(half, half), QSizeF(2 * half, 2 * half));
}

ColorWheel::Part ColorWheel::PartAt(QPointF at) const {
  assert(Outer() > Inner());
  assert(Inner() > 0.0);
  const double reach = std::hypot(at.x() - Middle().x(), at.y() - Middle().y());
  const bool is_ring = reach >= Inner() - 2.0 && reach <= Outer() + 2.0;
  const bool is_square = Square().adjusted(-4, -4, 4, 4).contains(at);
  return is_ring ? Part::kRing : is_square ? Part::kSquare : Part::kNone;
}

QColor ColorWheel::Color() const {
  assert(hue_ >= 0.0 && hue_ <= 1.0);
  assert(saturation_ >= 0.0 && value_ >= 0.0);
  return QColor::fromHsvF(static_cast<float>(hue_),
                          static_cast<float>(saturation_),
                          static_cast<float>(value_));
}

void ColorWheel::DragTo(QPointF at) {
  assert(dragging_ != Part::kNone);
  const QPointF from = at - Middle();
  const bool is_ring = dragging_ == Part::kRing;
  if (is_ring) {
    // Counterclockwise from three o'clock, as the conical gradient runs.
    const double turn = std::atan2(-from.y(), from.x()) /
                        (2.0 * std::numbers::pi);
    hue_ = turn < 0.0 ? turn + 1.0 : turn;
  } else {
    const QRectF square = Square();
    saturation_ =
        std::clamp((at.x() - square.left()) / square.width(), 0.0, 1.0);
    value_ = std::clamp((square.bottom() - at.y()) / square.height(), 0.0,
                        1.0);
  }
  assert(hue_ >= 0.0 && hue_ <= 1.0);
  update();
}

void ColorWheel::paintEvent(QPaintEvent* event) {
  assert(event != nullptr);
  assert(kHueStops > 0);
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  QConicalGradient hues(Middle(), 0.0);
  for (int stop = 0; stop <= kHueStops; ++stop) {
    const double at = static_cast<double>(stop) / kHueStops;
    hues.setColorAt(at, QColor::fromHsvF(static_cast<float>(at), 1.0f, 1.0f));
  }
  QPainterPath ring;
  ring.addEllipse(Middle(), Outer(), Outer());
  ring.addEllipse(Middle(), Inner(), Inner());
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(hues);
  painter.drawPath(ring);
  const QRectF square = Square();
  painter.fillRect(square, QColor::fromHsvF(static_cast<float>(hue_), 1, 1));
  QLinearGradient white(square.topLeft(), square.topRight());
  white.setColorAt(0.0, Qt::white);
  white.setColorAt(1.0, QColor(255, 255, 255, 0));
  painter.fillRect(square, white);
  QLinearGradient black(square.topLeft(), square.bottomLeft());
  black.setColorAt(0.0, QColor(0, 0, 0, 0));
  black.setColorAt(1.0, Qt::black);
  painter.fillRect(square, black);
  painter.setPen(QPen(theme::kShadow, 1.0));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(square);
  // Markers: a ring on the hue, a ring on the strength and brightness.
  const double angle = hue_ * 2.0 * std::numbers::pi;
  const double middle = (Outer() + Inner()) / 2.0;
  const QPointF on_ring = Middle() + QPointF(std::cos(angle) * middle,
                                             -std::sin(angle) * middle);
  const QPointF on_square(square.left() + saturation_ * square.width(),
                          square.bottom() - value_ * square.height());
  for (const QPointF& marker : {on_ring, on_square}) {
    painter.setPen(QPen(Qt::black, 3.0));
    painter.drawEllipse(marker, kMarker, kMarker);
    painter.setPen(QPen(Qt::white, 1.5));
    painter.drawEllipse(marker, kMarker, kMarker);
  }
}

void ColorWheel::mousePressEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(Outer() > Inner());
  // The wheel keeps its presses, so the panel does not move.
  event->accept();
  const bool is_left = event->button() == Qt::LeftButton;
  const Part part = is_left ? PartAt(event->position()) : Part::kNone;
  const bool is_on = part != Part::kNone;
  if (is_on) {
    dragging_ = part;
    DragTo(event->position());
  }
}

void ColorWheel::mouseMoveEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(hue_ >= 0.0 && hue_ <= 1.0);
  event->accept();
  const bool is_dragging = dragging_ != Part::kNone;
  if (is_dragging) {
    DragTo(event->position());
  }
}

void ColorWheel::mouseReleaseEvent(QMouseEvent* event) {
  assert(event != nullptr);
  assert(saturation_ >= 0.0 && saturation_ <= 1.0);
  event->accept();
  const bool was_dragging = dragging_ != Part::kNone;
  dragging_ = Part::kNone;
  if (was_dragging) {
    emit Picked(Color(), event->modifiers());
  }
}

}  // namespace snapper
