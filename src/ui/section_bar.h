#ifndef SNAPPER_UI_SECTION_BAR_H_
#define SNAPPER_UI_SECTION_BAR_H_

#include <QFont>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QStyle>
#include <QStyleOption>

namespace snapper {

// A section of a panel, the first of the two header levels: a full-width
// raised title bar with the red strip and bold text at its left, over
// one sunken well holding the section's settings. The style draws every
// group box so (folds and PlatedSection match it).

// The title bar's height for text in font.
int SectionBarHeight(const QFont& font);

// True when box is a section of its own: titled and not flat. A box
// folded into a FoldSection is flat with no title, and the fold is the
// section, so it draws nothing round its settings.
bool IsSection(const QStyleOptionGroupBox& box);

// Where part (the label, frame or contents) of an uncheckable group box
// whose whole rect is box goes, for font; with no bar (has_bar false)
// the contents take it all.
QRect SectionPart(QStyle::SubControl part, const QRect& box,
                  const QFont& font, bool has_bar);

// The bar over bar with title, in font made bold.
void DrawSectionBar(QPainter* painter, const QRect& bar,
                    const QString& title, const QFont& font);

// A whole uncheckable group box: the bar, then the well under it, or
// nothing when it is not a section of its own.
void DrawSection(QPainter* painter, const QStyleOptionGroupBox& box,
                 const QFont& font);

}  // namespace snapper

#endif  // SNAPPER_UI_SECTION_BAR_H_
