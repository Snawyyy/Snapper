#ifndef SNAPPER_ANIM_LINKS_H_
#define SNAPPER_ANIM_LINKS_H_

#include <QPointF>
#include <QTransform>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "anim/pose_memo.h"
#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where each thing in a shot is once its links are added. A follower
// gets strength x how far its leader's point (a layer's origin, a
// piece's joint) has moved in shot space since the link's from frame,
// so at that frame it sits where its keys put it and from then on it
// copies the leader's x and y. A piece's share is turned into its
// parent's space and added to its offset, so it carries its children.
// A leader that itself follows something passes that on: chains work.
// Every place that draws or hit-tests the stage asks this.
//
// Made per shot and dropped after use; it remembers poses it worked out
// so a frame is solved once. Not shared between threads.
class LinkSolver final {
 public:
  // memo, when given, keeps the poses of doll layers across solvers
  // (see PoseMemo); the shared pieces are the same either way.
  LinkSolver(const Project& project, const Shot& shot,
             PoseMemo* memo = nullptr);

  // Layer space to shot space at frame, moved by the hold and the link
  // on the layer.
  QTransform LayerTransform(const Layer& layer, Frame frame);
  // What holding does to an object layer at frame, in shot space: after
  // its own keys, before any link. Identity for a layer never held.
  // Holding by a piece makes it copy the piece's place, turn and scale
  // since the hold began (see Hold); a drop keeps the move it had then.
  QTransform HeldMove(const Layer& layer, Frame frame);
  // The poses a doll layer shows at frame (ShownPoses), with the links
  // on its pieces added to their offsets.
  PoseMap Poses(const Doll& doll, const Layer& layer, Frame frame);
  // How far link moves its follower at frame, in shot space.
  QPointF Push(const Link& link, Frame frame);

 private:
  // Where end's point is in shot space at frame, links included;
  // nothing when its layer, doll or piece is gone.
  std::optional<QPointF> PointOf(const LinkEnd& end, Frame frame);
  // How far the links on end and on whatever carries it move it.
  QPointF Carried(const LinkEnd& end, Frame frame);
  // Layer space to shot space with the hold but without the link.
  QTransform Placed(const Layer& layer, Frame frame);
  // A holder's drawing pixels (a layer's own space for a whole layer)
  // to shot space at frame, holds and links included; nothing when it
  // is gone.
  std::optional<QTransform> HolderTransform(const LinkEnd& holder,
                                            Frame frame);
  const PoseMap& BasePoses(const Doll& doll, const Layer& layer,
                           Frame frame);
  // How far links carry each of layer's drag-node pieces, frame 0 to
  // frame, so their trails feel the links too.
  LinkShifts Shifts(const Doll& doll, const Layer& layer, Frame frame);

  const Project& project_;
  const Shot& shot_;
  // Null when poses are not kept across solvers.
  PoseMemo* memo_;
  // What this solver and, with a memo, earlier ones worked out.
  std::map<std::pair<int, int>, PoseMap> own_poses_;
  std::map<std::pair<const Link*, int>, QPointF> own_pushes_;
  LinkShifts own_shifts_;
  // ShownPoses by layer id and frame.
  std::map<std::pair<int, int>, PoseMap>* poses_;
  // Pushes worked out, by link and frame, and those being worked out:
  // meeting one of those again means a loop got into the file, and it
  // counts as no push.
  std::map<std::pair<const Link*, int>, QPointF>* pushes_;
  std::set<std::pair<const Link*, int>> solving_;
  // Held moves being worked out, by layer id and frame: meeting one
  // again means objects hold each other, and it counts as not held.
  std::set<std::pair<int, int>> holding_;
};

// Whatever moves end along: end itself, its layer, and the pieces it
// hangs from, nearest first.
std::vector<LinkEnd> CarriersOf(const Project& project, const Shot& shot,
                                const LinkEnd& end);
// True when follower copying leader would feed back into itself: the
// leader moves with the follower, directly or through other links.
// follower's own current link does not count; it would be replaced.
bool WouldLoop(const Project& project, const Shot& shot,
               const LinkEnd& follower, const LinkEnd& leader);

}  // namespace snapper

#endif  // SNAPPER_ANIM_LINKS_H_
