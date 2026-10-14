#ifndef SNAPPER_UI_TIMELINE_LAYOUT_H_
#define SNAPPER_UI_TIMELINE_LAYOUT_H_

#include <QColor>

#include "ui/grab.h"

namespace snapper {

// Sizes of the timelines, in pixels. The ruler is the same height on the
// Pose, Video and Comp timelines.
inline constexpr int kNameWidth = 120;
inline constexpr int kRulerHeight = 18;
inline constexpr int kWaveHeight = 28;
inline constexpr int kRowHeight = 22;
inline constexpr double kDefaultFrameWidth = 12.0;
// Keys look small (like every handle) but grab big (kGrabReach).
inline constexpr double kKeySize = kHandleDraw / 2;
// Below this frame width the grid only marks numbered frames.
inline constexpr double kMinGridWidth = 5.0;
// Grid lines: each frame, numbered frames, seconds.
inline constexpr QColor kGridFrame{255, 255, 255, 14};
inline constexpr QColor kGridLabel{255, 255, 255, 34};
inline constexpr QColor kGridSecond{255, 255, 255, 70};

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_LAYOUT_H_
