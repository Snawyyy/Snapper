#ifndef SNAPPER_ANIM_COMP_REEL_H_
#define SNAPPER_ANIM_COMP_REEL_H_

#include <vector>

#include "model/comp.h"
#include "model/project.h"

namespace snapper {

// Comp effects on the reel: each shot's effects are timed over its
// first clip there, and an effect on the screen runs wherever its span
// reaches on the reel, past its shot's cut too.

// The first clip of shot on the reel's picture tracks; nullptr when the
// shot is not on the reel.
const Clip* FirstClipOf(const Project& project, ShotId shot);

// Where an effect runs on the reel, first frame and one past the last,
// over its shot's clip: the clip's own frames for a whole-shot effect.
struct ReelSpan final {
  int first = 0;
  int end = 0;
};
ReelSpan EffectOnReel(const Clip& clip, const CompEffect& effect);

// The frame of shot its effects are timed by at reel frame: through its
// first clip, kept within the shot; 0 when it is not on the reel.
Frame EffectFrameAt(const Project& project, ShotId shot, Frame reel);

// A screen effect running at a reel frame: its shot, the effect, and the
// shot frame it is timed by there (below 0 or past the shot when it
// reaches past the cut).
struct ReelEffect final {
  const Shot* shot = nullptr;
  const CompEffect* effect = nullptr;
  int at = 0;
};
// Every screen effect running at frame, shots by where their clip
// starts, each shot's in the order they run.
std::vector<ReelEffect> ScreenEffectsAt(const Project& project, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_COMP_REEL_H_
