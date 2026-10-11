#include "ui/motion_art.h"

#include <QPainter>
#include <QPainterPath>

#include <cassert>
#include <cmath>
#include <numbers>

#include "base/text.h"
#include "ui/material_art.h"

namespace snapper {
namespace {

constexpr double kTwoPi = 2.0 * std::numbers::pi;
constexpr double kDot = 3.5;

void DrawDot(QPainter* painter, QPointF at, double radius,
             const QColor& ink) {
  assert(painter != nullptr);
  assert(radius > 0.0);
  painter->setPen(Qt::NoPen);
  painter->setBrush(ink);
  painter->drawEllipse(at, radius, radius);
}

// A dot riding a sine wave that runs across the tile.
void PaintWave(QPainter* painter, const QRectF& area, double phase,
               const QColor& ink) {
  assert(painter != nullptr);
  assert(area.isValid());
  const QRectF box = area.adjusted(6, 0, -6, 0);
  const double mid = area.center().y();
  const double tall = area.height() * 0.22;
  QPainterPath path;
  constexpr int kSteps = 16;
  for (int i = 0; i <= kSteps; ++i) {
    const double along = static_cast<double>(i) / kSteps;
    const QPointF at(box.left() + box.width() * along,
                     mid + tall * std::sin(kTwoPi * (along - phase)));
    const bool is_first = i == 0;
    if (is_first) {
      path.moveTo(at);
    } else {
      path.lineTo(at);
    }
  }
  painter->setPen(QPen(ink, 1.0));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(path);
  const double here = mid + tall * std::sin(kTwoPi * (0.5 - phase));
  DrawDot(painter, QPointF(box.center().x(), here), kDot, ink);
}

}  // namespace

QString MotionName(WarpMotionKind kind) {
  assert(static_cast<int>(kind) >= 0);
  assert(static_cast<int>(kind) < kWarpMotionKindCount);
  switch (kind) {
    case WarpMotionKind::kNone:
      return Tr("None");
    case WarpMotionKind::kWave:
      return Tr("Wave");
    case WarpMotionKind::kPulse:
      return Tr("Pulse");
    case WarpMotionKind::kBreathe:
      return Tr("Breathe");
    case WarpMotionKind::kSway:
      return Tr("Sway");
    case WarpMotionKind::kShiver:
      return Tr("Shiver");
  }
  return QString();
}

void PaintMotion(QPainter* painter, const QRectF& area, WarpMotionKind kind,
                 double phase, const QColor& ink) {
  assert(painter != nullptr);
  assert(area.isValid());
  painter->setRenderHint(QPainter::Antialiasing);
  const QPointF centre = area.center();
  const double swing = Swing(phase);
  switch (kind) {
    case WarpMotionKind::kNone:
      DrawDot(painter, centre, kDot, ink);
      break;
    case WarpMotionKind::kWave:
      PaintWave(painter, area, phase, ink);
      break;
    case WarpMotionKind::kPulse:
      // Quick: swells and snaps back twice a loop.
      DrawDot(painter, centre,
              kDot * (1.0 + 0.7 * std::abs(std::sin(kTwoPi * phase))), ink);
      break;
    case WarpMotionKind::kBreathe:
      DrawDot(painter, centre, kDot * (1.25 + 0.45 * swing), ink);
      break;
    case WarpMotionKind::kSway: {
      // A pendulum from the top of the tile.
      const QPointF pivot(centre.x(), area.top() + 3);
      const double length = area.height() * 0.6;
      const double angle = 0.7 * swing;
      const QPointF bob = pivot + QPointF(std::sin(angle), std::cos(angle)) *
                                      length;
      painter->setPen(QPen(ink, 1.0));
      painter->drawLine(pivot, bob);
      DrawDot(painter, bob, kDot, ink);
      break;
    }
    case WarpMotionKind::kShiver: {
      const double t = kTwoPi * phase * 6.0;
      DrawDot(painter,
              centre + QPointF(2.0 * std::sin(t), 1.5 * std::sin(1.7 * t)),
              kDot, ink);
      break;
    }
  }
}

}  // namespace snapper
