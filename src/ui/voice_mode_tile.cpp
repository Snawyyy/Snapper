#include "ui/voice_mode_tile.h"

#include <QPainter>
#include <QPolygonF>

#include <cassert>
#include <cmath>
#include <numbers>

#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kTileWidth = 120;
constexpr int kTileHeight = 72;

// A square speech box with a tail at its lower left and three lines of
// talk in it.
void PaintSpeechBox(QPainter* painter, const QRectF& box, const QColor& ink,
                    const QColor& fill) {
  assert(painter != nullptr);
  assert(box.isValid());
  const double tail = box.height() * 0.3;
  QPolygonF shape;
  shape << box.topLeft() << box.topRight() << box.bottomRight()
        << QPointF(box.left() + tail * 1.4, box.bottom())
        << QPointF(box.left() + tail * 0.4, box.bottom() + tail)
        << QPointF(box.left() + tail * 0.6, box.bottom()) << box.bottomLeft();
  painter->setPen(QPen(ink, 1.5));
  painter->setBrush(fill);
  painter->drawPolygon(shape);
  for (int line = 1; line <= 3; ++line) {
    const double y = box.top() + box.height() * line / 4.0;
    const double end = line == 3 ? 0.55 : 0.8;
    painter->drawLine(QPointF(box.left() + box.width() * 0.18, y),
                      QPointF(box.left() + box.width() * end, y));
  }
}

// A four point sparkle of radius round center.
void PaintSparkle(QPainter* painter, QPointF center, double radius,
                  const QColor& color) {
  assert(painter != nullptr);
  assert(radius >= 0.0);
  const double waist = radius * 0.28;
  QPolygonF star;
  star << center + QPointF(0, -radius) << center + QPointF(waist, -waist)
       << center + QPointF(radius, 0) << center + QPointF(waist, waist)
       << center + QPointF(0, radius) << center + QPointF(-waist, waist)
       << center + QPointF(-radius, 0) << center + QPointF(-waist, -waist);
  painter->setPen(Qt::NoPen);
  painter->setBrush(color);
  painter->drawPolygon(star);
}

}  // namespace

VoiceModeTile::VoiceModeTile(bool is_design, const QString& label,
                             MotionManager* motion, QWidget* parent)
    : LoopTile(label, motion, parent), is_design_(is_design) {
  assert(motion != nullptr);
  assert(!label.isEmpty());
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize VoiceModeTile::sizeHint() const {
  assert(kTileWidth > 0);
  assert(kTileHeight > 0);
  return {kTileWidth, kTileHeight};
}

void VoiceModeTile::PaintArt(QPainter* painter, const QRectF& art,
                             double phase, const QColor& ink) const {
  assert(painter != nullptr);
  assert(phase >= 0.0 && phase <= 1.0);
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  const double swing = std::sin(phase * 2.0 * std::numbers::pi);
  const double side = std::min(art.height() * 0.62, art.width() * 0.4);
  const QPointF middle = art.center();
  if (is_design_) {
    const QPointF start(middle.x() - side * 0.9, middle.y() + side * 0.6);
    const QPointF tip(middle.x() + side * 0.3, middle.y() + side * 0.12);
    painter->setPen(QPen(ink, 2.0));
    painter->drawLine(start, tip);
    const QColor gold = isEnabled() ? theme::kHandle : ink;
    PaintSparkle(painter, tip + QPointF(side * 0.35, -side * 0.3),
                 side * (0.38 + 0.12 * swing), gold);
    PaintSparkle(painter, tip + QPointF(side * 0.95, side * 0.25),
                 side * (0.18 - 0.08 * swing), gold);
    PaintSparkle(painter, start + QPointF(side * 0.2, -side * 0.75),
                 side * (0.14 + 0.07 * swing), gold);
  } else {
    const QRectF front(middle.x() - side * 0.95, middle.y() - side * 0.5,
                       side * 1.2, side * 0.8);
    // The copy slides out from behind and back as the loop runs.
    const double out = side * (0.45 + 0.15 * swing);
    const QColor copy = isEnabled() ? theme::kPick : ink;
    PaintSpeechBox(painter, front.translated(out, -side * 0.12), copy,
                   theme::kFaceDark);
    PaintSpeechBox(painter, front, ink, theme::kField);
  }
  painter->restore();
}

}  // namespace snapper
