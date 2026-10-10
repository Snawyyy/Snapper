#ifndef SNAPPER_ANIM_MIRROR_H_
#define SNAPPER_ANIM_MIRROR_H_

#include <QSize>
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

// One pose (of a piece warped by from over a drawing of from_size)
// seen in a mirror, for a piece warped by to over to_size: turns, leans
// and sideways moves flip, and the warp flips left to right. Onto the
// same grid each point takes its mirrored point's push; otherwise
// (meshes, or twins whose warps differ) each of to's points takes the
// push found at its spot mirrored onto from's drawing.
PiecePose MirrorPose(const PiecePose& pose, const WarpGrid& from,
                     QSize from_size, const WarpGrid& to, QSize to_size);

// The whole doll's pose flipped: each side takes the mirrored pose of
// the other, and pieces without a partner mirror in place.
PoseMap MirrorPoses(const Doll& doll, const PoseMap& poses);

}  // namespace snapper

#endif  // SNAPPER_ANIM_MIRROR_H_
