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

// 60: her coat. Warm greys, red a touch over green over blue.
inline constexpr QColor kFace{0x33, 0x30, 0x2d};
inline constexpr QColor kFaceLight{0x52, 0x4d, 0x48};
inline constexpr QColor kFaceDark{0x21, 0x1f, 0x1d};
inline constexpr QColor kShadow{0x0f, 0x0e, 0x0d};
inline constexpr QColor kField{0x19, 0x17, 0x16};
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

// Sets the style, palette and font on the whole application.
void Apply(QApplication* application);

}  // namespace theme
}  // namespace snapper

#endif  // SNAPPER_UI_THEME_H_
