#ifndef SNAPPER_ANIM_DOLL_LEAN_H_
#define SNAPPER_ANIM_DOLL_LEAN_H_

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// How far the make-believe camera stands, in doll heights. Nearer
// makes the near end grow more.
constexpr double kCameraDistance = 1.5;
// A leaning piece draws in front of another only when it is nearer by
// this much of the doll's height.
constexpr double kRestackGap = 0.12;

// Fakes the doll turning degrees around the middle line of its box,
// like a wheel facing the camera, as the camera would see it: above 0
// the top swings toward the camera, below 0 away, and any amount works
// (a whole turn is the start again). Heights shrink toward the middle
// line, so leaning in brings the head down; past a quarter turn the
// doll is upside down. How far up or down a piece's joint sits in the
// rest pose sets how near the camera it swings: nearer pieces grow and
// spread out, further ones shrink and pull in. Drawings grow without
// squashing, so faces keep their shape. Pieces clearly nearer the
// camera than their parent, children or siblings draw in front.
PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double degrees);

// The poses a doll layer shows at frame: its pieces' keys, leaned by
// its own move's lean. Everything that draws or hit-tests a doll on
// stage uses this.
PoseMap ShownPoses(const Doll& doll, const Layer& layer, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_LEAN_H_
