#ifndef SNAPPER_UI_SHOT_STRIP_LAYOUT_H_
#define SNAPPER_UI_SHOT_STRIP_LAYOUT_H_

#include <algorithm>
#include <cassert>

#include "model/shot.h"

namespace snapper {

// Sizes of the shot strip, in pixels.
inline constexpr double kPixelsPerFrame = 2.0;
// Wide enough for the first frame and a name beside it.
inline constexpr double kMinBlock = 150.0;
inline constexpr double kAddWidth = 28.0;
// Room for a polaroid: its picture, its border and the pin above it.
inline constexpr int kStripHeight = 58;
// How wide the first-frame picture of a chip is drawn (sourced larger).
inline constexpr int kStripThumbWidth = 64;
inline constexpr double kPictureWidth = 60.0;

// How wide shot's chip is.
inline double WidthOf(const Shot& shot) {
  assert(shot.length.index() >= 1);
  const double width = shot.length.index() * kPixelsPerFrame;
  assert(width > 0.0);
  return std::max(width, kMinBlock);
}

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_STRIP_LAYOUT_H_
