#include "ui/deck_paint.h"

#include <QLinearGradient>
#include <QPen>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper::deck {
namespace {

// Grooves and brush lines painted at most, however tall the area.
constexpr int kMaxGrooves = 400;
constexpr int kMaxBrushLines = 1200;
// Brushed plastic: a faint light line every this many pixels.
constexpr double kBrushGap = 3.0;
constexpr QColor kBrush{0xff, 0xf0, 0xe0, 7};
constexpr QColor kShell{0x16, 0x15, 0x14};

}  // namespace

void PaintPlastic(QPainter* painter, const QRectF& area, double end,
                  double row_height) {
  assert(painter != nullptr);
  assert(row_height > 0.0);
  QLinearGradient body(area.topLeft(), area.bottomLeft());
  body.setColorAt(0.0, kPlasticLight);
  body.setColorAt(0.08, kPlastic);
  body.setColorAt(1.0, kPlastic);
  painter->fillRect(area, body);
  const double from = std::clamp(end, area.left(), area.right());
  painter->fillRect(QRectF(from, area.top(), area.right() - from,
                           area.height()),
                    kPlasticDark);
  // Fine brush marks so wide empty rows read as plastic, not a hole.
  painter->setPen(kBrush);
  double line = area.top() + kBrushGap;
  for (int i = 0; i < kMaxBrushLines && line < area.bottom(); ++i) {
    painter->drawLine(QPointF(area.left(), line), QPointF(from, line));
    line += kBrushGap;
  }
  // Each row is a moulded slot: a dark groove with a faint lit ridge
  // under it, as on a deck's front panel.
  double y = area.top() + row_height;
  for (int i = 0; i < kMaxGrooves && y < area.bottom(); ++i) {
    painter->setPen(kGroove);
    painter->drawLine(QPointF(area.left(), y - 1.0),
                      QPointF(area.right(), y - 1.0));
    painter->setPen(kRidge);
    painter->drawLine(QPointF(area.left(), y), QPointF(area.right(), y));
    y += row_height;
  }
}

void PaintLabel(QPainter* painter, const QRectF& strip, const QString& text,
                bool is_current, bool is_shut) {
  assert(painter != nullptr);
  assert(strip.width() > 8.0 && strip.height() > 4.0);
  // A flat tag in the panel's own colours, so names read as part of the
  // app rather than paper stuck on it; the rust rule keeps the cassette.
  const QRectF tag = strip.adjusted(2.0, 1.0, -2.0, -1.0);
  painter->fillRect(tag, is_current ? theme::kFaceLight : theme::kRaised);
  painter->setPen(QPen(kLabelRule, 1.0));
  painter->drawLine(QPointF(tag.left(), tag.bottom()),
                    QPointF(tag.right(), tag.bottom()));
  if (is_current) {
    painter->fillRect(QRectF(tag.left(), tag.top(), 3.0, tag.height()),
                      theme::kPick);
  }
  painter->setPen(is_shut ? theme::kTextOff : theme::kText);
  painter->drawText(tag.adjusted(8.0, 0.0, -3.0, 0.0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    painter->fontMetrics().elidedText(
                        text, Qt::ElideRight,
                        static_cast<int>(tag.width()) - 11));
}

void PaintTape(QPainter* painter, const QRectF& run) {
  assert(painter != nullptr);
  assert(std::isfinite(run.left()) && std::isfinite(run.top()));
  const bool is_empty = run.width() <= 0.0 || run.height() <= 0.0;
  if (is_empty) {
    return;
  }
  QLinearGradient oxide(run.topLeft(), run.bottomLeft());
  oxide.setColorAt(0.0, kTapeLight);
  oxide.setColorAt(0.3, kTape);
  oxide.setColorAt(1.0, kTapeDark);
  painter->fillRect(run, oxide);
}

void PaintColorTape(QPainter* painter, const QRectF& run,
                    const QColor& color) {
  assert(painter != nullptr);
  assert(color.isValid());
  const bool is_empty = run.width() <= 0.0 || run.height() <= 0.0;
  if (is_empty) {
    return;
  }
  QLinearGradient oxide(run.topLeft(), run.bottomLeft());
  oxide.setColorAt(0.0, color.lighter(125));
  oxide.setColorAt(0.3, color);
  oxide.setColorAt(1.0, color.darker(170));
  painter->fillRect(run, oxide);
}

void PaintCassette(QPainter* painter, const QRectF& box, const QColor& band,
                   const QString& name, bool is_shut) {
  assert(painter != nullptr);
  assert(band.isValid());
  // A black shell, lit along its top edge.
  painter->fillRect(box, kShell);
  painter->setPen(kPlasticLight);
  painter->drawLine(box.topLeft(), box.topRight());
  const QRectF label = box.adjusted(3.0, 3.0, -2.0, -3.0);
  const bool has_label = label.width() > 4.0 && label.height() > 4.0;
  if (!has_label) {
    return;
  }
  // The label fills the spine; a colored band across its top tells the
  // kind, as the stripe on a tape's label tells its length.
  painter->fillRect(label, is_shut ? kLabelShut : kLabel);
  const double band_height = std::min(5.0, label.height() / 3.0);
  painter->fillRect(QRectF(label.left(), label.top(), label.width(),
                           band_height),
                    is_shut ? band.darker(150) : band);
  const QRectF line(label.left() + 5.0, label.top() + band_height,
                    label.width() - 8.0, label.height() - band_height);
  const bool has_room = !name.isEmpty() && line.width() > 16.0 &&
                        line.height() >= painter->fontMetrics().height();
  if (has_room) {
    painter->setPen(kInk);
    painter->drawText(line, Qt::AlignVCenter | Qt::AlignLeft,
                      painter->fontMetrics().elidedText(
                          name, Qt::ElideRight,
                          static_cast<int>(line.width())));
  }
}

void PaintDisplay(QPainter* painter, const QRectF& glass) {
  assert(painter != nullptr);
  assert(glass.width() > 2.0 && glass.height() > 2.0);
  painter->fillRect(glass, kGlass);
  // Sunk in: shadow on the top and left edge, light on the bottom.
  painter->setPen(kPlasticDark);
  painter->drawLine(glass.topLeft(), glass.topRight());
  painter->drawLine(glass.topLeft(), glass.bottomLeft());
  painter->setPen(kPlasticLight);
  painter->drawLine(glass.bottomLeft(), glass.bottomRight());
  QLinearGradient sheen(glass.topLeft(), glass.bottomLeft());
  sheen.setColorAt(0.0, QColor(255, 255, 255, 18));
  sheen.setColorAt(0.5, QColor(255, 255, 255, 0));
  painter->fillRect(glass, sheen);
}

void PaintGlowText(QPainter* painter, const QRectF& area,
                   const QString& text, bool is_lit) {
  assert(painter != nullptr);
  assert(area.width() > 0.0);
  const int flags = Qt::AlignVCenter | Qt::AlignLeft;
  if (is_lit) {
    // A faint halo a pixel round the letters, as phosphor bleeds.
    QColor halo = kGlow;
    halo.setAlpha(60);
    painter->setPen(halo);
    painter->drawText(area.translated(1.0, 0.0), flags, text);
    painter->drawText(area.translated(-1.0, 0.0), flags, text);
  }
  painter->setPen(is_lit ? kGlow : kGlowDim);
  painter->drawText(area, flags, text);
}

void PaintScrew(QPainter* painter, QPointF center, double radius) {
  assert(painter != nullptr);
  assert(radius > 0.0);
  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(QPen(kPlasticDark, 1.0));
  painter->setBrush(QColor(0x80, 0x7c, 0x78));
  painter->drawEllipse(center, radius, radius);
  painter->setPen(QPen(kPlasticDark, 1.0));
  painter->drawLine(center + QPointF(-radius * 0.6, radius * 0.6),
                    center + QPointF(radius * 0.6, -radius * 0.6));
  painter->restore();
}

void PaintPanel(QPainter* painter, const QRectF& area) {
  assert(painter != nullptr);
  assert(area.height() > 2.0);
  PaintPlastic(painter, area, area.right(), area.height() + 1.0);
  painter->setPen(kPlasticLight.lighter(130));
  painter->drawLine(area.topLeft(), area.topRight());
  painter->setPen(kPlasticDark);
  painter->drawLine(area.bottomLeft(), area.bottomRight());
  // Screws at the ends, as if the panel were fixed to the deck.
  const double radius = std::min(3.0, area.height() / 6.0);
  const double y = area.center().y();
  PaintScrew(painter, QPointF(area.left() + 2.0 * radius + 1.0, y), radius);
  PaintScrew(painter, QPointF(area.right() - 2.0 * radius - 1.0, y), radius);
}

}  // namespace snapper::deck
