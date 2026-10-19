#include "ui/bone_glyph.h"

#include <QLineF>
#include <QPen>
#include <QPolygonF>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper {

void PaintBoneGlyph(QPainter* painter, QPointF base, QPointF tip,
                    const QColor& body, double grow) {
  assert(painter != nullptr);
  assert(grow > 0.0);
  const QLineF line(base, tip);
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);
  painter->setPen(QPen(theme::kShadow, 1.0));
  if (line.length() > 1.0) {
    const double width = std::clamp(line.length() * 0.12, 3.0, 9.0);
    const QLineF normal = line.normalVector().unitVector();
    const QPointF side = QPointF(normal.dx(), normal.dy()) * width;
    const QPointF shoulder = line.pointAt(0.18);
    painter->setBrush(body);
    painter->drawPolygon(QPolygonF{base, shoulder + side, tip, shoulder - side});
  }
  painter->setBrush(theme::kText);
  painter->drawEllipse(base, 3.0, 3.0);
  painter->setBrush(theme::kHandle);
  painter->drawEllipse(tip, 3.5 * grow, 3.5 * grow);
  painter->restore();
}

}  // namespace snapper
