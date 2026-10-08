#ifndef SNAPPER_ANIM_WARP_MOTION_H_
#define SNAPPER_ANIM_WARP_MOTION_H_

#include <QPointF>

#include <vector>

#include "base/frame.h"
#include "model/doll.h"

namespace snapper {

// How far motion pushes its own point at frame of the shot, in drawing
// pixels, when it starts late by extra of a cycle on top of its delay.
// The same frame always gives the same push.
QPointF MotionPush(const PointMotion& motion, Frame frame,
                   double extra = 0.0);

// motion's push on every point of grid at frame, row by row: its own
// point moves by MotionPush and the rest follow by reach, as they
// follow a drag node; a wave reaches them later the further they are.
// All zero when motion's point is not on grid.
std::vector<QPointF> PointMotionPushes(WarpGrid grid,
                                       const PointMotion& motion,
                                       double reach, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_WARP_MOTION_H_
