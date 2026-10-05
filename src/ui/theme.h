#ifndef SNAPPER_UI_THEME_H_
#define SNAPPER_UI_THEME_H_

#include <QColor>

class QApplication;

namespace snapper {

// Snapper's look: a dark take on Windows 98. Raised 3D buttons, sunken
// fields, square corners, tight spacing, and Teto red for whatever is
// picked or active. Every widget draws from these, so the look lives in
// one place.
namespace theme {

inline constexpr QColor kFace{0x3a, 0x3a, 0x3e};
inline constexpr QColor kFaceLight{0x5c, 0x5c, 0x62};
inline constexpr QColor kFaceDark{0x24, 0x24, 0x27};
inline constexpr QColor kShadow{0x10, 0x10, 0x12};
inline constexpr QColor kField{0x1c, 0x1c, 0x1f};
inline constexpr QColor kText{0xe6, 0xe6, 0xe6};
inline constexpr QColor kTextOff{0x80, 0x80, 0x86};
// Teto red: picks, the playhead, the active tab.
inline constexpr QColor kPick{0xd8, 0x30, 0x4f};
// Handles you can drag.
inline constexpr QColor kHandle{0xf2, 0xc2, 0x30};
inline constexpr int kFontPoints = 9;

// Sets the style, palette and font on the whole application.
void Apply(QApplication* application);

}  // namespace theme
}  // namespace snapper

#endif  // SNAPPER_UI_THEME_H_
