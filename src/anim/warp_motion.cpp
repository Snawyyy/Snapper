#include "anim/warp_motion.h"

#include <cassert>

namespace snapper {

std::vector<QPointF> MotionPushes(QSize size, WarpGrid grid,
                                  const WarpMotion& motion, Frame frame) {
  assert(frame.index() >= 0);
  assert(motion.cycle >= kMinWarpCycle && motion.cycle <= kMaxWarpCycle);
  std::vector<QPointF> pushes(static_cast<size_t>(grid.PointCount()));
  const bool is_moving = motion.IsOn() && grid.IsOn() && !size.isEmpty();
  if (!is_moving) {
    return pushes;
  }
  switch (motion.kind) {
    case WarpMotionKind::kNone:
      break;
  }
  assert(pushes.size() <= static_cast<size_t>(kMaxWarpPoints));
  return pushes;
}

}  // namespace snapper
