#ifndef SNAPPER_UI_MODE_ICONS_H_
#define SNAPPER_UI_MODE_ICONS_H_

#include <QColor>
#include <QIcon>

namespace snapper {

// The small picture on each mode tab, drawn with a painter so the app
// ships no image files: a puppet for Pose, two joined bones for Rig, a
// film frame with a play mark for Video. index is the tab's place
// (0 to 2). The picked tab's icon is tinted Teto red, the rest are
// plain text colour.
QIcon ModeIcon(int index, bool is_picked);

// Side of the drawn icon in pixels.
inline constexpr int kModeIconPx = 16;

}  // namespace snapper

#endif  // SNAPPER_UI_MODE_ICONS_H_
