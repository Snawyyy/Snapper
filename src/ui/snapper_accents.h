#ifndef SNAPPER_UI_SNAPPER_ACCENTS_H_
#define SNAPPER_UI_SNAPPER_ACCENTS_H_

#include <QPainter>
#include <QRect>
#include <QStyleOption>

namespace snapper {

// The Teto red accents the style puts on the look: what is on, picked or
// focused shows it in red, so the eye finds it among the grey.

// A red strip over strip (a tab's top, a plate's side, a toggled button's
// bottom).
void DrawAccentStrip(QPainter* painter, const QRect& strip);
// A one pixel red ring just inside box, for the field that has the keys.
void DrawFocusRing(QPainter* painter, const QRect& box);
// A check box or radio button: a sunken square (round for radio) with a
// red mark inside when on, a short one when half on.
void DrawCheckIndicator(QPainter* painter, const QStyleOption& option,
                        bool is_radio);

}  // namespace snapper

#endif  // SNAPPER_UI_SNAPPER_ACCENTS_H_
