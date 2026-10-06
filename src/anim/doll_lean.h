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
// Drawings squash by the spacing's squash to this power, so they keep
// more of their height than the gaps between them.
constexpr double kDrawingSquash = 0.5;
// A leaning piece draws in front of another only when it is nearer by
// this much of the doll's height.
constexpr double kRestackGap = 0.12;

// Fakes the doll turning in depth around the middle of its box, as a
// camera would see it. lean turns it like a wheel facing the camera
// (above 0 the top swings toward the camera); swivel turns it like a
// door on an upright hinge (above 0 its right side swings toward the
// camera). Any amount works and a whole turn is the start again.
// Spacing shrinks along the turn, so leaning in brings the head down;
// past a quarter turn the doll is upside down or seen from behind.
// How far a piece's joint sits from the middle in the rest pose sets
// how near the camera it swings: nearer pieces grow and spread out,
// further ones shrink and pull in. Drawings squash a little less than
// the spacing, and not at all when the rig says a piece keeps its
// shape. Pieces clearly nearer the camera than their parent, children
// or siblings draw in front.
PoseMap LeanPoses(const Doll& doll, const PoseMap& poses, double lean,
                  double swivel = 0.0);

// The poses a doll layer shows at frame: its pieces' keys, drag nodes
// trailing (DraggedPoses), turned by its own move's lean and swivel.
// Everything that draws or hit-tests a doll on stage uses this.
PoseMap ShownPoses(const Doll& doll, const Layer& layer, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_LEAN_H_
