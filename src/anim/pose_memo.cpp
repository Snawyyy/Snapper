#include "anim/pose_memo.h"

#include <algorithm>
#include <cassert>

namespace snapper {

bool PoseMemo::Bind(const Project& project, const Shot& shot) {
  assert(poses_.size() <= kMaxMemoPoses);
  assert(project.shots.size() <= static_cast<size_t>(kMaxShots));
  const auto owner = std::find_if(
      project.shots.begin(), project.shots.end(),
      [&shot](const std::shared_ptr<const Shot>& held) {
        return held.get() == &shot;
      });
  const bool is_owned = owner != project.shots.end();
  if (!is_owned) {
    return false;
  }
  const bool is_same = shot_ == *owner && dolls_ == project.dolls;
  if (!is_same) {
    poses_.clear();
    shot_ = *owner;
    dolls_ = project.dolls;
  }
  return true;
}

const PoseMap* PoseMemo::Find(LayerId layer, Frame frame) const {
  assert(frame.index() >= 0);
  assert(layer.value() >= 0);
  const auto found = poses_.find({layer.value(), frame.index()});
  const bool is_kept = found != poses_.end();
  return is_kept ? &found->second : nullptr;
}

void PoseMemo::Store(LayerId layer, Frame frame, const PoseMap& poses) {
  assert(frame.index() >= 0);
  assert(layer.value() >= 0);
  const bool is_full = poses_.size() >= kMaxMemoPoses;
  if (is_full) {
    poses_.clear();
  }
  poses_[{layer.value(), frame.index()}] = poses;
}

}  // namespace snapper
