#include "base/frame.h"

#include <cassert>
#include <cmath>

namespace snapper {

Frame FrameAtSeconds(double seconds) {
  // Media files can report NaN or negative times; they mean the start.
  const bool is_usable = std::isfinite(seconds) && seconds > 0.0;
  if (!is_usable) {
    return Frame(0);
  }
  const double frames = std::floor(seconds * kFramesPerSecond);
  assert(frames >= 0.0);
  const bool is_past_end = frames >= static_cast<double>(kMaxFrame);
  if (is_past_end) {
    return Frame(kMaxFrame);
  }
  const Frame frame(static_cast<int>(frames));
  assert(frame.index() < kMaxFrame);
  return frame;
}

double SecondsAtFrame(Frame frame) {
  assert(frame.index() >= 0);
  assert(frame.index() <= kMaxFrame);
  return static_cast<double>(frame.index()) / kFramesPerSecond;
}

}  // namespace snapper
