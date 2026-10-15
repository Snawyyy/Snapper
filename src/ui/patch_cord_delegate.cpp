#include "ui/patch_cord_delegate.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cassert>

#include "ui/deck_paint.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kRowHeight = 30;
constexpr double kJack = 5.0;
// Room for each piece's name beside its jack.
constexpr int kNameRoom = 70;
constexpr double kArc = 7.0;

// A jack socket: a dark nut with a silver ring.
void PaintJack(QPainter* painter, QPointF at) {
  painter->setPen(QPen(QColor(0xb8, 0xb2, 0xaa), 1.5));
  painter->setBrush(theme::kShadow);
  painter->drawEllipse(at, kJack, kJack);
  painter->setBrush(QColor(0x50, 0x48, 0x44));
  painter->setPen(Qt::NoPen);
  painter->drawEllipse(at, kJack / 2.5, kJack / 2.5);
}

// A half turn arrow round at, clockwise or not, for the bend.
void PaintBend(QPainter* painter, QPointF at, bool is_clockwise,
               const QColor& ink) {
  const QRectF ring(at - QPointF(kArc, kArc), QSizeF(2 * kArc, 2 * kArc));
  QPainterPath arc;
  const double from = is_clockwise ? 160.0 : 20.0;
  const double sweep = is_clockwise ? -140.0 : 140.0;
  arc.arcMoveTo(ring, from);
  arc.arcTo(ring, from, sweep);
  painter->setPen(QPen(ink, 1.5));
  painter->setBrush(Qt::NoBrush);
  painter->drawPath(arc);
  // The head at the arc's end, pointing on.
  const QPointF tip = arc.currentPosition();
  const double side = is_clockwise ? 1.0 : -1.0;
  painter->setBrush(ink);
  painter->setPen(Qt::NoPen);
  painter->drawPolygon(QPolygonF({tip + QPointF(side * 3.5, -1.5),
                                  tip + QPointF(-side * 1.0, 3.5),
                                  tip + QPointF(-side * 2.5, -3.0)}));
}

}  // namespace

int PatchCordDelegate::RowHeight() {
  assert(kRowHeight > 0);
  assert(kNameRoom > 0);
  return kRowHeight;
}

void PatchCordDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  assert(painter != nullptr);
  assert(index.isValid());
  painter->save();
  const QRect row = option.rect.adjusted(0, 1, 0, -1);
  const bool is_picked = option.state.testFlag(QStyle::State_Selected);
  painter->fillRect(row, is_picked ? theme::kFace : theme::kWell);
  painter->setRenderHint(QPainter::Antialiasing, true);
  const double middle = row.center().y();
  const QPointF left(row.left() + kNameRoom, middle);
  const QPointF right(row.right() - kNameRoom, middle);
  // The cord sags between the jacks.
  QPainterPath cord(left);
  const double sag = std::min(10.0, row.height() / 2.0 - 2.0);
  cord.cubicTo(left + QPointF(20, sag), right + QPointF(-20, sag), right);
  const QColor red = is_picked ? theme::kPick : theme::kPick.darker(170);
  painter->setPen(QPen(QColor(0, 0, 0, 120), 4.0, Qt::SolidLine,
                       Qt::RoundCap));
  painter->drawPath(cord.translated(0, 1.5));
  painter->setPen(QPen(red, 3.0, Qt::SolidLine, Qt::RoundCap));
  painter->drawPath(cord);
  PaintJack(painter, left);
  PaintJack(painter, right);
  PaintBend(painter, cord.pointAtPercent(0.5) - QPointF(0, kArc + 2),
            index.data(kChainClockwiseRole).toBool(),
            is_picked ? theme::kText : theme::kTextOff);
  painter->setPen(is_picked ? theme::kText : theme::kTextOff);
  const auto name = [&](const QString& text) {
    return option.fontMetrics.elidedText(text, Qt::ElideRight,
                                         kNameRoom - 14);
  };
  painter->drawText(QRectF(row.left() + 4, row.top(), kNameRoom - 12,
                           row.height()),
                    Qt::AlignRight | Qt::AlignVCenter,
                    name(index.data(kChainUpperRole).toString()));
  painter->drawText(QRectF(row.right() - kNameRoom + 10, row.top(),
                           kNameRoom - 12, row.height()),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    name(index.data(kChainLowerRole).toString()));
  painter->restore();
}

QSize PatchCordDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
  assert(index.isValid());
  assert(option.rect.width() >= 0);
  return QSize(0, kRowHeight);
}

}  // namespace snapper
