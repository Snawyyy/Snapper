#include "base/frame.h"

#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// A count of frames worked out from seconds, kept to 0 to kMaxFrame.
Frame FrameOf(double frames) {
  assert(std::isfinite(frames));
  assert(frames >= 0.0);
  const bool is_past_end = frames >= static_cast<double>(kMaxFrame);
  if (is_past_end) {
    return Frame(kMaxFrame);
  }
  const Frame frame(static_cast<int>(frames));
  assert(frame.index() < kMaxFrame);
  return frame;
}

// NaN or negative seconds (media files can report them) mean none.
bool IsUsable(double seconds) {
  return std::isfinite(seconds) && seconds > 0.0;
}

}  // namespace

Frame FrameAtSeconds(double seconds) {
  const bool is_usable = IsUsable(seconds);
  if (!is_usable) {
    return Frame(0);
  }
  return FrameOf(std::floor(seconds * kFramesPerSecond));
}

Frame FramesNearSeconds(double seconds) {
  const bool is_usable = IsUsable(seconds);
  if (!is_usable) {
    return Frame(0);
  }
  return FrameOf(std::round(seconds * kFramesPerSecond));
}

double SecondsAtFrame(Frame frame) {
  assert(frame.index() >= 0);
  assert(frame.index() <= kMaxFrame);
  return static_cast<double>(frame.index()) / kFramesPerSecond;
}

}  // namespace snapper
