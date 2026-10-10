#ifndef SNAPPER_ANIM_DOLL_COLLIDE_H_
#define SNAPPER_ANIM_DOLL_COLLIDE_H_

#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// Pushes the warp grid of every piece a collider pushes out of it at
// frame (PushGrid), adding the push to the piece's warp in poses. The
// pieces go back to front, so a piece that was pushed pushes the ones
// in front of it from where it was pushed to. Works on every grid
// point alike, plain, drag or physics, from where poses put it this
// frame: no history, so a frame shows the same however it is reached.
// Nothing changes when the doll has no collider.
void CollidePoses(const Doll& doll, const Layer& layer, Frame frame,
                  const LinkShifts& shifts, PoseMap* poses);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_COLLIDE_H_
