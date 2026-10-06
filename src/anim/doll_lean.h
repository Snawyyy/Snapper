#ifndef SNAPPER_ANIM_DOLL_LEAN_H_
#define SNAPPER_ANIM_DOLL_LEAN_H_

#include "anim/doll_pose.h"
#include "model/doll.h"

namespace snapper {

// The lean amount stays inside this, so no piece shrinks to nothing.
constexpr double kMaxLean = 0.9;

// Fakes the doll tipping toward the camera (amount above 0) or away
// (below 0) around the middle of its box. Each piece gets a weight from
// where its joint sits in the rest pose: 0 on the box's middle line, 1
// at the top edge, -1 at the bottom. A piece then grows by
// 1 + amount * weight and moves the same way away from the middle of
// the posed box, so the top spreads while the bottom pulls in. Only
// scale and offset change; turns, drawings and warps stay.
PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double amount);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_LEAN_H_
