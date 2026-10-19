#ifndef SNAPPER_ANIM_BONE_DYNAMICS_H_
#define SNAPPER_ANIM_BONE_DYNAMICS_H_

#include <functional>

#include "base/frame.h"
#include "model/character.h"

namespace snapper {

// Frames of motion a bounce looks back over: a spring settles well
// within this, and a late frame costs no more than this one.
constexpr int kDynamicsWindow = 96;

// True when some bone of character bounces (Bone::dynamics above 0).
bool HasDynamics(const Character& character);

// pose_at(frame) with Moho's bone dynamics added: each bouncing bone's
// tip trails where the pose puts it on a spring (the more dynamics, the
// looser and longer it rings) and the bone turns toward the trailing
// tip. Run from kDynamicsWindow frames before frame, so every render of
// a frame is the same. Bones are run parents first, so a bouncing
// child rides its bouncing parent.
CharacterPose WithDynamics(
    const Character& character, Frame frame,
    const std::function<CharacterPose(Frame)>& pose_at);

}  // namespace snapper

#endif  // SNAPPER_ANIM_BONE_DYNAMICS_H_
