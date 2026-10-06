#ifndef SNAPPER_ANIM_DOLL_LEAN_H_
#define SNAPPER_ANIM_DOLL_LEAN_H_

#include "anim/doll_pose.h"
#include "model/doll.h"

namespace snapper {

// Lean amounts run from -kMaxLean to kMaxLean, which tips the doll
// kMaxTilt degrees.
constexpr double kMaxLean = 0.9;
constexpr double kMaxTilt = 45.0;
// How far the make-believe camera stands, in doll heights. Nearer
// makes the near end grow more.
constexpr double kCameraDistance = 1.5;
// A leaning piece draws in front of another only when it is nearer by
// this much of the doll's height.
constexpr double kRestackGap = 0.12;

// Fakes the doll tipping toward the camera (amount above 0) or away
// (below 0) around the middle line of its box, as a camera would see
// it. Heights shrink toward the middle line, so leaning in brings the
// head down. How far up or down a piece's joint sits in the rest pose
// sets how near the camera it swings: the top comes nearer, growing and
// spreading out; the bottom goes back, shrinking and pulling in. Turns,
// scale, skew and offset change; drawings and warps stay. Drawings
// grow without squashing, so faces keep their shape. Pieces clearly
// nearer the camera than others draw in front of them.
PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double amount);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_LEAN_H_
