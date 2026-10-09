#ifndef SNAPPER_ANIM_DOLL_DRAG_H_
#define SNAPPER_ANIM_DOLL_DRAG_H_

#include <QPointF>
#include <QString>

#include <QTransform>

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/doll.h"
#include "model/layer.h"

namespace snapper {

// How far links carry each piece in shot space, by piece name and then
// frame from 0. A piece or frame missing from it is not carried.
using LinkShifts = std::map<QString, std::vector<QPointF>>;

// Where the springs of one doll layer stood after each frame from the
// first on, so a later frame carries on from the last one worked out
// instead of running the springs from the start again. Only good for
// the doll, keys and links it was made with: whoever keeps one drops it
// when any of them is edited (see PoseMemo).
class DragTrail final {
 public:
  using Carry = std::optional<std::pair<QPointF, QTransform>>;
  // The springs after one frame, in the doll's spring order.
  struct State final {
    std::vector<QPointF> at;
    std::vector<QPointF> speed;
    std::vector<Carry> carried;
  };

  // frames()[f] is the state after frame f; no gaps.
  const std::vector<State>& frames() const { return frames_; }
  void Add(State state);
  void Clear() { frames_.clear(); }

 private:
  std::vector<State> frames_;
};

// The pieces' keys at frame with every drag node's trail added to their
// warp: each drag node is a spring that follows where its grid point is
// carried (by the pose, the layer's own move and shifts, the links
// that carry it) from the shot's first
// frame on, lagging and bouncing as its lag and bounce say. The pull
// spreads to neighbouring points as far as the point's rubber reach.
// The trail only changes on the doll's own beat (its keys, each frame
// of an ease, and the same spacing through a long hold), so a doll on
// 3s drags on 3s. Settled at the first frame, so every render of a
// frame is the same. Each point motion (a wave, a pulse) is added
// on top, on the same beat, spread by its point's reach.
// With a trail, the springs resume from its last frame at or before
// the beat and add the frames they run to it; the poses come out the
// same either way.
PoseMap DraggedPoses(const Doll& doll, const Layer& layer, Frame frame,
                     const LinkShifts& shifts = {},
                     DragTrail* trail = nullptr);

}  // namespace snapper

#endif  // SNAPPER_ANIM_DOLL_DRAG_H_
