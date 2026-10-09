#ifndef SNAPPER_ANIM_POSE_MEMO_H_
#define SNAPPER_ANIM_POSE_MEMO_H_

#include <QString>

#include <map>
#include <memory>
#include <utility>

#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/project.h"

namespace snapper {

// At most this many solved poses are kept; past it the oldest shot's
// worth is dropped whole.
constexpr size_t kMaxMemoPoses = 512;

// The poses a shot's doll layers show, kept by layer and frame so a
// frame is solved once however many paints, hover tests and handle
// overlays ask for it. A solve with drag nodes runs springs from the
// first frame, so it is the costliest thing a paint does.
//
// It holds one shot at a time, by identity: shots and dolls are shared
// read-only pointers, so the same pointers mean the same poses, and any
// edit makes new ones and empties the memo. Not shared between threads.
class PoseMemo final {
 public:
  // Points the memo at shot of project, emptying it if that is not the
  // shot (or the dolls) it kept poses for. False when shot is not one
  // of project's shots, so nothing can be kept for it.
  bool Bind(const Project& project, const Shot& shot);
  // The poses kept for layer at frame, or nullptr.
  const PoseMap* Find(LayerId layer, Frame frame) const;
  void Store(LayerId layer, Frame frame, const PoseMap& poses);
  // Where layer's drag-node springs have been run to so far; goes with
  // the poses when the shot changes.
  DragTrail* TrailOf(LayerId layer);
  size_t size() const { return poses_.size(); }

 private:
  std::shared_ptr<const Shot> shot_;
  std::map<QString, std::shared_ptr<const Doll>> dolls_;
  std::map<std::pair<int, int>, PoseMap> poses_;
  std::map<int, DragTrail> trails_;
};

}  // namespace snapper

#endif  // SNAPPER_ANIM_POSE_MEMO_H_
