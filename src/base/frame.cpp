#include "base/frame.h"

#include <cassert>
#include <cmath>

namespace snapper {
namespace {

// A count of frames worked out from seconds, kept to 0 to kMaxFrame.
// Huge finite seconds times the rate can overflow to infinity, which the
// clamp below handles like any count past the end.
Frame FrameOf(double frames) {
  assert(!std::isnan(frames));
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
  // A positive rate keeps usable seconds a count of frames above 0.
  static_assert(kFramesPerSecond > 0, "frames per second must be positive");
  const bool is_usable = std::isfinite(seconds) && seconds > 0.0;
  assert(!is_usable || seconds * kFramesPerSecond > 0.0);
  return is_usable;
}

}  // namespace

Frame FrameAtSeconds(double seconds) {
  const bool is_usable = IsUsable(seconds);
  if (!is_usable) {
    return Frame(0);
  }
  const double frames = std::floor(seconds * kFramesPerSecond);
  // FrameOf needs a real count of frames, never NaN or below 0.
  assert(!std::isnan(frames));
  assert(frames >= 0.0);
  return FrameOf(frames);
}

Frame FramesNearSeconds(double seconds) {
  const bool is_usable = IsUsable(seconds);
  if (!is_usable) {
    return Frame(0);
  }
  const double frames = std::round(seconds * kFramesPerSecond);
  // FrameOf needs a real count of frames, never NaN or below 0.
  assert(!std::isnan(frames));
  assert(frames >= 0.0);
  return FrameOf(frames);
}

double SecondsAtFrame(Frame frame) {
  assert(frame.index() >= 0);
  assert(frame.index() <= kMaxFrame);
  return static_cast<double>(frame.index()) / kFramesPerSecond;
}

}  // namespace snapper
