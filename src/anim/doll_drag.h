#ifndef SNAPPER_ANIM_DOLL_DRAG_H_
#define SNAPPER_ANIM_DOLL_DRAG_H_

#include <QPointF>
#include <QString>

#include <map>
#include <vector>

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// How far links carry each piece in shot space, by piece name and then
// frame from 0. A piece or frame missing from it is not carried.
using LinkShifts = std::map<QString, std::vector<QPointF>>;

// The pieces' keys at frame with every drag node's trail added to their
// warp: each drag node is a spring that follows where its grid point is
// carried (by the pose, the layer's own move and shifts, the links
// that carry it) from the shot's first
// frame on, lagging and bouncing as its lag and bounce say. The pull
// spreads to neighbouring points as far as the point's rubber reach.
// The trail only changes on the doll's own beat (its keys, each frame
// of an ease, and the same spacing through a long hold), so a doll on
// 3s drags on 3s. Settled at the first frame, so every render of a
// frame is the same. Each piece's warp motion (a wave, a pulse) is
// added on top, on the same beat.
PoseMap DraggedPoses(const Doll& doll, const Layer& layer, Frame frame,
                     const LinkShifts& shifts = {});

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_DRAG_H_
