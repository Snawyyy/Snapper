#ifndef SNAPPER_ANIM_DOLL_DRAG_H_
#define SNAPPER_ANIM_DOLL_DRAG_H_

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// The pieces' keys at frame with every drag node's trail added to their
// warp: each drag node is a spring that follows where its grid point is
// carried (by the pose and the layer's own move) from the shot's first
// frame on, lagging and bouncing as its lag and bounce say. The pull
// spreads to neighbouring points as far as the point's rubber reach.
// The trail only changes on the doll's own beat (its keys, each frame
// of an ease, and the same spacing through a long hold), so a doll on
// 3s drags on 3s. Settled at the first frame, so every render of a
// frame is the same.
PoseMap DraggedPoses(const Doll& doll, const Layer& layer, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_DRAG_H_
