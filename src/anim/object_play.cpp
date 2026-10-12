#include "anim/object_play.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

namespace snapper {
namespace {

// What a GIF frame of 0 to 10 ms shows for, as browsers play it.
constexpr int kSlowFrameMs = 100;
constexpr int kFastestFrameMs = 10;

// The frame indices one cycle of mode steps through: 0 1 2 for loop and
// once, 0 1 2 1 for ping-pong.
std::vector<int> Steps(PlayMode mode, int count) {
  assert(count >= 2 && count <= kMaxObjectFrames);
  assert(mode != PlayMode::kHold);
  std::vector<int> steps;
  for (int i = 0; i < count; ++i) {
    steps.push_back(i);
  }
  const bool is_ping_pong = mode == PlayMode::kPingPong;
  if (is_ping_pong) {
    for (int i = count - 2; i >= 1; --i) {
      steps.push_back(i);
    }
  }
  assert(!steps.empty());
  return steps;
}

}  // namespace

int FrameMs(int ms) {
  assert(kSlowFrameMs > kFastestFrameMs);
  const bool is_too_fast = ms <= kFastestFrameMs;
  return is_too_fast ? kSlowFrameMs : std::min(ms, 60 * 1000);
}

int PlayedDrawing(const ObjectClip& clip, const ObjectPlay& play,
                  Frame frame, int keyed) {
  assert(frame.index() >= 0);
  assert(clip.frame_ms.size() <= static_cast<size_t>(kMaxObjectFrames));
  const int count = static_cast<int>(clip.frame_ms.size());
  const bool is_playing = clip.is_object && count >= 2 &&
                          play.mode != PlayMode::kHold &&
                          keyed < count;
  if (!is_playing) {
    return keyed;
  }
  const std::vector<int> steps = Steps(play.mode, count);
  // Each step's length in shot frames.
  std::vector<double> lengths;
  double cycle = 0.0;
  for (const int step : steps) {
    const double length =
        play.frames_per_drawing > 0
            ? static_cast<double>(play.frames_per_drawing)
            : FrameMs(clip.frame_ms[static_cast<size_t>(step)]) *
                  kFramesPerSecond / 1000.0;
    lengths.push_back(length);
    cycle += length;
  }
  assert(cycle > 0.0);
  // The offset starts it that many steps in.
  const int steps_count = static_cast<int>(steps.size());
  const int skipped =
      ((play.offset % steps_count) + steps_count) % steps_count;
  double into = static_cast<double>(frame.index() - play.start.index());
  for (int i = 0; i < skipped; ++i) {
    into += lengths[static_cast<size_t>(i)];
  }
  const bool is_before = frame < play.start;
  if (is_before) {
    return steps[static_cast<size_t>(skipped)];
  }
  const bool is_once = play.mode == PlayMode::kOnce;
  if (is_once && into >= cycle) {
    return steps.back();
  }
  into = std::fmod(into, cycle);
  // A hair over, so a step ending exactly on a frame shows the next.
  for (size_t i = 0; i < steps.size(); ++i) {
    const bool is_in = into < lengths[i] - 1e-9;
    if (is_in) {
      return steps[i];
    }
    into -= lengths[i];
  }
  return steps.back();
}

void ApplyObjectPlay(const Doll& doll, const DollLayer& layer, Frame frame,
                     PoseMap* poses) {
  assert(poses != nullptr);
  assert(frame.index() >= 0);
  const bool is_object = doll.rig.object.is_object;
  if (!is_object) {
    return;
  }
  for (const RigPiece& rig : doll.rig.pieces) {
    PiecePose& pose = (*poses)[rig.name];
    const int keyed = pose.drawing >= 0 ? pose.drawing
                      : rig.default_drawing >= 0 ? rig.default_drawing
                                                 : 0;
    const int played =
        PlayedDrawing(doll.rig.object, layer.play, frame, keyed);
    const bool is_changed = played != keyed;
    if (is_changed) {
      pose.drawing = played;
    }
  }
}

}  // namespace snapper
