#ifndef SNAPPER_UI_SHOT_STRIP_LAYOUT_H_
#define SNAPPER_UI_SHOT_STRIP_LAYOUT_H_

#include <algorithm>
#include <cassert>

#include "model/shot.h"

namespace snapper {

// Sizes of the shot strip, in pixels.
inline constexpr double kPixelsPerFrame = 2.0;
inline constexpr double kMinBlock = 56.0;
inline constexpr double kAddWidth = 28.0;
inline constexpr int kStripHeight = 40;
// How wide the first-frame picture a long chip shows is drawn (sourced
// larger), and how long a chip must be to show it.
inline constexpr int kStripThumbWidth = 64;
inline constexpr double kPictureWidth = 40.0;
inline constexpr double kPictureFrom = 110.0;

// How wide shot's chip is.
inline double WidthOf(const Shot& shot) {
  assert(shot.length.index() >= 1);
  const double width = shot.length.index() * kPixelsPerFrame;
  assert(width > 0.0);
  return std::max(width, kMinBlock);
}

}  // namespace snapper

#endif  // SNAPPER_UI_SHOT_STRIP_LAYOUT_H_
