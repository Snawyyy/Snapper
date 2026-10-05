#ifndef SNAPPER_ANIM_MIRROR_H_
#define SNAPPER_ANIM_MIRROR_H_

#include <QString>

#include "anim/doll_pose.h"
#include "model/doll.h"

namespace snapper {

// The other side's name: "arm_l" to "arm_r", "Left hand" to "Right
// hand", "L_leg" to "R_leg", "eye.L" to "eye.R". Unsided names come
// back as they are.
QString MirrorName(const QString& name);

// Which side a name marks: -1 left, 1 right, 0 neither.
int SideOf(const QString& name);

// One pose seen in a mirror: turns, leans and sideways moves flip, and
// the warp grid flips left to right.
PiecePose MirrorPose(const PiecePose& pose, WarpGrid grid);

// The whole doll's pose flipped: each side takes the mirrored pose of
// the other, and pieces without a partner mirror in place.
PoseMap MirrorPoses(const Doll& doll, const PoseMap& poses);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MIRROR_H_
