#ifndef SNAPPER_UI_TIMELINE_LAYOUT_H_
#define SNAPPER_UI_TIMELINE_LAYOUT_H_

#include <QColor>

#include <array>

namespace snapper {

// Sizes of the timeline, in pixels.
inline constexpr int kNameWidth = 120;
inline constexpr int kRulerHeight = 18;
inline constexpr int kWaveHeight = 28;
inline constexpr int kRowHeight = 22;
inline constexpr double kDefaultFrameWidth = 12.0;
inline constexpr double kMinFrameWidth = 3.0;
inline constexpr double kMaxFrameWidth = 40.0;
// Keys look small but grab big.
inline constexpr double kKeyReach = 6.0;
inline constexpr double kKeySize = 5.0;
// Ruler numbers stay this many pixels apart, numbering every frame, or
// every 2, 3, 6... as the timeline zooms out.
inline constexpr double kMinLabelGap = 24.0;
inline constexpr std::array<int, 9> kLabelSteps = {1,  2,  3,  6,  12,
                                                   24, 48, 96, 240};
// Below this frame width the grid only marks numbered frames.
inline constexpr double kMinGridWidth = 5.0;
// Grid lines: each frame, numbered frames, seconds.
inline constexpr QColor kGridFrame{255, 255, 255, 14};
inline constexpr QColor kGridLabel{255, 255, 255, 34};
inline constexpr QColor kGridSecond{255, 255, 255, 70};
// Frames a wheel notch scrolls.
inline constexpr double kScrollFrames = 6.0;

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_LAYOUT_H_
