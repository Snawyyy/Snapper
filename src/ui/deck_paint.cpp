#include "ui/deck_paint.h"

#include <QLinearGradient>
#include <QPen>

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ui/theme.h"

namespace snapper::deck {
namespace {

// Grooves painted at most, however tall the area.
constexpr int kMaxGrooves = 400;

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
  const QRectF paper = strip.adjusted(3.0, 2.0, -3.0, -2.0);
  painter->fillRect(paper.translated(1.0, 1.0), kPlasticDark);
  painter->fillRect(paper, is_shut ? kLabelShut : kLabel);
  // The rust rule along the label's foot, as printed on a cassette.
  painter->setPen(QPen(kLabelRule, 1.0));
  const double rule = paper.bottom() - 3.0;
  painter->drawLine(QPointF(paper.left() + 4.0, rule),
                    QPointF(paper.right() - 3.0, rule));
  if (is_current) {
    painter->fillRect(QRectF(paper.left(), paper.top(), 4.0,
                             paper.height()),
                      theme::kPick);
  }
  painter->setPen(kInk);
  painter->drawText(paper.adjusted(8.0, 0.0, -3.0, -2.0),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    painter->fontMetrics().elidedText(
                        text, Qt::ElideRight,
                        static_cast<int>(paper.width()) - 11));
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

void PaintCassette(QPainter* painter, const QRectF& box, const QColor& body,
                   const QString& name) {
  assert(painter != nullptr);
  assert(body.isValid());
  QLinearGradient shell(box.topLeft(), box.bottomLeft());
  shell.setColorAt(0.0, body.lighter(135));
  shell.setColorAt(0.15, body);
  shell.setColorAt(1.0, body.darker(160));
  painter->fillRect(box, shell);
  // The label sits on the top half of the spine, where a hand-written
  // title goes; too short a clip keeps its bare plastic.
  const double height = std::min(box.height() / 2.0 + 2.0, 22.0);
  const QRectF sticker(box.left() + 4.0, box.top(), box.width() - 6.0,
                       height);
  const bool has_room = sticker.width() > 24.0 && sticker.height() > 10.0;
  if (has_room) {
    PaintLabel(painter, sticker, name, false, false);
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

}  // namespace snapper::deck
