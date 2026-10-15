#include "ui/section_bar.h"

#include <QFontMetrics>

#include <cassert>

#include "ui/snapper_accents.h"
#include "ui/style_bevel.h"
#include "ui/theme.h"

namespace snapper {
namespace {

// Room above and below the title's text.
constexpr int kBarPad = 4;
// The title's start, past the red strip.
constexpr int kTitleIndent = 10;
// The red strip's width down the bar's left.
constexpr int kStripWidth = 3;
// Room inside the well, past kIndent on the left.
constexpr int kWellPad = 6;

}  // namespace

int SectionBarHeight(const QFont& font) {
  const int text = QFontMetrics(font).height();
  assert(text > 0);
  assert(kBarPad >= 0);
  return text + 2 * kBarPad;
}

QRect SectionPart(QStyle::SubControl part, const QRect& box,
                  const QFont& font) {
  assert(box.isValid() || box.isEmpty());
  assert(theme::kIndent > 0);
  const int bar = SectionBarHeight(font);
  const QRect title(box.left(), box.top(), box.width(), bar);
  const QRect well(box.left(), box.top() + bar, box.width(),
                   box.height() - bar);
  const bool is_title = part == QStyle::SC_GroupBoxLabel;
  const bool is_well = part == QStyle::SC_GroupBoxFrame;
  if (is_title) {
    return title;
  }
  if (is_well) {
    return well;
  }
  return well.adjusted(theme::kIndent, kWellPad, -kWellPad, -kWellPad);
}

void DrawSectionBar(QPainter* painter, const QRect& bar,
                    const QString& title, const QFont& font) {
  assert(painter != nullptr);
  assert(kStripWidth > 0);
  const bool has_room = bar.isValid();
  if (!has_room) {
    return;
  }
  DrawBevel(painter, bar, theme::kRaised, false);
  DrawAccentStrip(painter, QRect(bar.left() + 1, bar.top() + 1, kStripWidth,
                                 bar.height() - 2));
  painter->save();
  QFont bold = font;
  bold.setBold(true);
  painter->setFont(bold);
  painter->setPen(theme::kText);
  painter->drawText(bar.adjusted(kTitleIndent, 0, -kBarPad, 0),
                    Qt::AlignLeft | Qt::AlignVCenter,
                    QFontMetrics(bold).elidedText(
                        title, Qt::ElideRight, bar.width() - kTitleIndent));
  painter->restore();
}

void DrawSection(QPainter* painter, const QStyleOptionGroupBox& box,
                 const QFont& font) {
  assert(painter != nullptr);
  assert(box.rect.isValid() || box.rect.isEmpty());
  const QRect well = SectionPart(QStyle::SC_GroupBoxFrame, box.rect, font);
  const bool has_well = well.isValid();
  if (has_well) {
    painter->fillRect(well, theme::kWell);
    DrawSunkenFrame(painter, well.adjusted(0, 0, -1, -1));
  }
  DrawSectionBar(painter, SectionPart(QStyle::SC_GroupBoxLabel, box.rect, font),
                 box.text, font);
}

}  // namespace snapper
