#ifndef SNAPPER_UI_TIMELINE_LAYOUT_H_
#define SNAPPER_UI_TIMELINE_LAYOUT_H_

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
// Frames a wheel notch scrolls.
inline constexpr double kScrollFrames = 6.0;

}  // namespace snapper

#endif  // SNAPPER_UI_TIMELINE_LAYOUT_H_
