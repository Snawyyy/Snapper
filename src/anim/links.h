#ifndef SNAPPER_ANIM_LINKS_H_
#define SNAPPER_ANIM_LINKS_H_

#include <QPointF>
#include <QTransform>

#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// Where each layer in a shot is once its links are added. A follower
// gets strength x how far its leader's origin has moved in shot space
// since the link's from frame, so at that frame it sits where its keys
// put it and from then on it copies the leader's x and y. A leader that
// itself follows something passes that on: chains work. Every place
// that draws or hit-tests the stage asks this.
//
// Made per shot and dropped after use; it remembers pushes it worked
// out so a frame is solved once. Not shared between threads.
class LinkSolver final {
 public:
  LinkSolver(const Project& project, const Shot& shot);

  // Layer space to shot space at frame, moved by the link on the layer.
  QTransform LayerTransform(const Layer& layer, Frame frame);
  // How far link moves its follower at frame, in shot space.
  QPointF Push(const Link& link, Frame frame);

 private:
  // Where end's origin is in shot space at frame, links included;
  // nothing when its layer is gone.
  std::optional<QPointF> PointOf(const LinkEnd& end, Frame frame);

  const Project& project_;
  const Shot& shot_;
  // Pushes worked out, by link and frame, and those being worked out:
  // meeting one of those again means a loop got into the file, and it
  // counts as no push.
  std::map<std::pair<const Link*, int>, QPointF> pushes_;
  std::set<std::pair<const Link*, int>> solving_;
};

// True when follower copying leader would feed back into itself: the
// leader moves with the follower, directly or through other links.
// follower's own current link does not count; it would be replaced.
bool WouldLoop(const Project& project, const Shot& shot,
               const LinkEnd& follower, const LinkEnd& leader);

}  // namespace snapper

#endif  // SNAPPER_ANIM_LINKS_H_
