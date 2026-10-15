#include "ui/effect_row_delegate.h"

#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPolygonF>

#include <cassert>

#include "model/comp.h"
#include "ui/comp_drag.h"
#include "ui/snapper_accents.h"
#include "ui/style_bevel.h"
#include "ui/theme.h"

namespace snapper {
namespace {

constexpr int kRowHeight = 22;
constexpr int kHeaderHeight = 24;
// Space above a header, so each group stands apart.
constexpr int kHeaderGap = 6;
constexpr int kBandWidth = 4;
constexpr int kTextIndent = 12;
constexpr double kArrowSize = 7.0;

QFont CaptionFont(const QFont& base) {
  QFont caption = base;
  caption.setBold(true);
  caption.setPointSizeF(caption.pointSizeF() * 0.85);
  caption.setCapitalization(QFont::AllUppercase);
  caption.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
  return caption;
}

// A solid triangle pointing right when folded, down when open.
QPolygonF Arrow(QPointF center, bool is_folded) {
  const double half = kArrowSize / 2.0;
  if (is_folded) {
    return QPolygonF({center + QPointF(-half * 0.6, -half),
                      center + QPointF(half * 0.8, 0.0),
                      center + QPointF(-half * 0.6, half)});
  }
  return QPolygonF({center + QPointF(-half, -half * 0.6),
                    center + QPointF(half, -half * 0.6),
                    center + QPointF(0.0, half * 0.8)});
}

}  // namespace

void EffectRowDelegate::paint(QPainter* painter,
                              const QStyleOptionViewItem& option,
                              const QModelIndex& index) const {
  assert(painter != nullptr);
  assert(index.isValid());
  painter->save();
  const bool is_header = index.data(kEffectHeaderRole).toBool();
  if (is_header) {
    PaintHeader(painter, option, index);
  } else {
    PaintEffect(painter, option, index);
  }
  painter->restore();
}

QSize EffectRowDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const {
  assert(index.isValid());
  assert(option.rect.width() >= 0);
  const bool is_header = index.data(kEffectHeaderRole).toBool();
  return QSize(0, is_header ? kHeaderHeight + kHeaderGap : kRowHeight);
}

void EffectRowDelegate::PaintHeader(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) {
  assert(painter != nullptr);
  assert(index.isValid());
  const QRect bar = option.rect.adjusted(0, kHeaderGap, 0, 0);
  DrawBevel(painter, bar, theme::kRaised, false);
  const bool is_folded = index.data(kEffectFoldedRole).toBool();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::NoPen);
  painter->setBrush(theme::kText);
  painter->drawPolygon(
      Arrow(QPointF(bar.left() + kTextIndent / 2.0 + 2.0,
                    bar.center().y() + 0.5),
            is_folded));
  painter->setFont(CaptionFont(option.font));
  painter->setPen(theme::kText);
  const QRect words = bar.adjusted(kTextIndent + 6, 0, -8, 0);
  painter->drawText(words, Qt::AlignLeft | Qt::AlignVCenter,
                    index.data(Qt::DisplayRole).toString());
  painter->setPen(theme::kTextOff);
  painter->drawText(words, Qt::AlignRight | Qt::AlignVCenter,
                    QString::number(index.data(kEffectCountRole).toInt()));
}

void EffectRowDelegate::PaintEffect(QPainter* painter,
                                    const QStyleOptionViewItem& option,
                                    const QModelIndex& index) {
  assert(painter != nullptr);
  assert(index.isValid());
  const QRect row = option.rect.adjusted(0, 1, 0, -1);
  const bool can_add = option.state.testFlag(QStyle::State_Enabled);
  const bool is_hovered =
      can_add && option.state.testFlag(QStyle::State_MouseOver);
  const bool is_picked = option.state.testFlag(QStyle::State_Selected);
  painter->fillRect(row, is_hovered ? theme::kFace : theme::kWell);
  const auto kind =
      static_cast<CompKind>(index.data(kEffectKindRole).toInt());
  const QColor mark = KindMark(kind);
  painter->fillRect(QRect(row.left(), row.top(), kBandWidth, row.height()),
                    can_add ? mark : mark.darker(250));
  painter->setPen(can_add ? theme::kText : theme::kTextOff);
  painter->drawText(row.adjusted(kTextIndent, 0, -4, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    option.fontMetrics.elidedText(
                        index.data(Qt::DisplayRole).toString(),
                        Qt::ElideRight, row.width() - kTextIndent - 4));
  if (is_picked) {
    DrawFocusRing(painter, row);
  }
}

}  // namespace snapper
