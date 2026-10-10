#include "ui/material_art.h"

#include <QPainter>
#include <QPainterPath>

#include <cassert>
#include <cmath>

namespace snapper {
namespace {

constexpr double kTwoPi = 6.283185307179586;

void PaintStill(QPainter* painter, const QRectF& area) {
  assert(painter != nullptr);
  assert(area.isValid());
  const double r = area.height() * 0.16;
  painter->drawLine(
      QPointF(area.left() + 4, area.center().y() + r + 3),
      QPointF(area.right() - 4, area.center().y() + r + 3));
  painter->drawEllipse(area.center(), r, r);
}

void PaintDrag(QPainter* painter, const QRectF& area, double phase) {
  assert(painter != nullptr);
  assert(area.isValid());
  const double r = area.height() * 0.14;
  const double reach = area.width() * 0.3;
  const double y = area.center().y();
  // Each tail dot is the head a moment ago.
  for (int i = 3; i >= 0; --i) {
    const double x = area.center().x() + reach * Swing(phase - 0.07 * i);
    const double size = r * (1.0 - 0.17 * i);
    painter->drawEllipse(QPointF(x, y), size, size);
  }
}

void PaintCloth(QPainter* painter, const QRectF& area, double phase) {
  assert(painter != nullptr);
  assert(area.isValid());
  constexpr int kSteps = 8;
  const QRectF sheet = area.adjusted(6, 4, -6, -4);
  QPainterPath path;
  path.moveTo(sheet.topLeft());
  path.lineTo(sheet.topRight());
  for (int i = kSteps; i >= 0; --i) {
    const double along = static_cast<double>(i) / kSteps;
    const double wave = Swing(phase - along * 0.8) * sheet.height() * 0.12;
    path.lineTo(sheet.left() + sheet.width() * along + wave * along,
                sheet.bottom() + wave);
  }
  path.closeSubpath();
  painter->drawPath(path);
}

void PaintJelly(QPainter* painter, const QRectF& area, double phase) {
  assert(painter != nullptr);
  assert(area.isValid());
  const double squash = 1.0 + 0.22 * Swing(phase);
  const double rx = area.width() * 0.2 * squash;
  const double ry = area.height() * 0.24 / squash;
  const QPointF centre(area.center().x(), area.bottom() - ry - 3);
  painter->drawEllipse(centre, rx, ry);
}

}  // namespace

double Swing(double phase) {
  assert(std::isfinite(phase));
  assert(kTwoPi > 6.0);
  return std::sin(kTwoPi * phase);
}

void PaintMaterial(QPainter* painter, const QRectF& area, Material material,
                   double phase, const QColor& ink) {
  assert(painter != nullptr);
  assert(area.isValid());
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(ink, 1.5));
  painter->setBrush(ink);
  switch (material) {
    case Material::kStill:
      PaintStill(painter, area);
      break;
    case Material::kDrag:
      PaintDrag(painter, area, phase);
      break;
    case Material::kCloth:
      PaintCloth(painter, area, phase);
      break;
    case Material::kJelly:
      PaintJelly(painter, area, phase);
      break;
  }
  painter->restore();
}

}  // namespace snapper
