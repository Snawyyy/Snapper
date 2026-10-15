#ifndef SNAPPER_UI_THEME_H_
#define SNAPPER_UI_THEME_H_

#include <QColor>

class QApplication;
class QWidget;

namespace snapper {

// Snapper's look: a dark take on Windows 98 dressed as Kasane Teto.
// Raised 3D buttons, sunken fields, square corners, tight spacing, in
// a 60 30 10 split: 60 the warm charcoal of her coat (faces, fields,
// shadows, the same warm grey as the deck plastic), 30 the cream of
// her shirt and the silver of her trim (text and labels), 10 her red
// hair (whatever is picked or active). Every widget draws from these,
// so the look lives in one place.
namespace theme {

// 60: her coat, in four depths so each area sits on its own surface.
// The deeper a level, the more it leans to wine; the higher, the more
// to rose, as her coat shades in light and shadow.
// Gap: the window behind everything, seen between the panels.
inline constexpr QColor kGap{0x10, 0x0b, 0x0c};
// Well: what is set into a panel, the stage surround and sunken frames.
inline constexpr QColor kWell{0x1c, 0x18, 0x18};
// Panel (kFace): the body of every panel and page.
inline constexpr QColor kFace{0x2e, 0x2a, 0x29};
// Raised: title plates, buttons, the top bar: what stands up off a
// panel.
inline constexpr QColor kRaised{0x42, 0x38, 0x37};
// Bevel edges, light and dark, and the deepest shadow line.
inline constexpr QColor kFaceLight{0x5c, 0x50, 0x4e};
inline constexpr QColor kFaceDark{0x22, 0x1e, 0x1e};
inline constexpr QColor kShadow{0x0c, 0x09, 0x0a};
// A field to type in, sunk below the well.
inline constexpr QColor kField{0x15, 0x12, 0x12};
// A field that could not read what was typed, for a moment.
inline constexpr QColor kFieldBad{0x5a, 0x1c, 0x28};
// 30: her shirt and trim. Cream text, silver when off.
inline constexpr QColor kText{0xec, 0xe6, 0xda};
inline constexpr QColor kTextOff{0x8c, 0x86, 0x7e};
// Placeholder text in an empty field: quieter than typed text, still
// easy to read on the dark field.
inline constexpr QColor kTextHint{0xa8, 0xa1, 0x96};
// 10: Teto red, her hair: picks, the playhead, the active tab.
inline constexpr QColor kPick{0xd8, 0x30, 0x4f};
// Handles you can drag.
inline constexpr QColor kHandle{0xf2, 0xc2, 0x30};
// Collider outline dots in the rig: cool, apart from the yellow handles.
inline constexpr QColor kCollider{0x2e, 0xc4, 0xd6};
inline constexpr int kFontPoints = 9;

// Spacing, on an 8 px rhythm. Settings sit kIndent in from their
// title, which stands out at the panel's edge; rows of a panel are
// kRowGap apart, cells that share a row kCellGap.
inline constexpr int kIndent = 12;
inline constexpr int kRowGap = 8;
inline constexpr int kCellGap = 16;

// One em of widget's font in pixels (its line height), so sizes given
// in ems follow the font and the screen's scale.
int Em(const QWidget* widget);

// Puts widget on a depth level: fills its back with level (kGap,
// kWell, kFace or kRaised), and what it holds inherits it.
void SetLevel(QWidget* widget, const QColor& level);

// Sets the style, palette and font on the whole application.
void Apply(QApplication* application);

}  // namespace theme
}  // namespace snapper

#endif  // SNAPPER_UI_THEME_H_
