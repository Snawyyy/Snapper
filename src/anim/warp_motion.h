#ifndef SNAPPER_ANIM_WARP_MOTION_H_
#define SNAPPER_ANIM_WARP_MOTION_H_

#include <QPointF>
#include <QSize>

#include <vector>

#include "base/frame.h"
#include "model/doll.h"

namespace snapper {

// How far motion pushes each point of grid, over a drawing of size, at
// frame of the shot: one push per point, row by row, in drawing pixels.
// The same frame always gives the same pushes. All zero when the motion
// or the grid is off.
std::vector<QPointF> MotionPushes(QSize size, WarpGrid grid,
                                  const WarpMotion& motion, Frame frame);

}  // namespace snapper

#endif  // SNAPPER_ANIM_WARP_MOTION_H_
