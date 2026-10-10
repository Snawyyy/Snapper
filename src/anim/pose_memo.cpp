#include "anim/pose_memo.h"

#include <algorithm>
#include <cassert>
#include <utility>

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
  const bool is_heavy = base_.size() > kMaxMemoWork ||
                        pushes_.size() > kMaxMemoWork;
  const bool is_stale = !is_same || is_heavy;
  if (is_stale) {
    poses_.clear();
    trails_.clear();
    base_.clear();
    pushes_.clear();
    shifts_.clear();
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

DragTrail* PoseMemo::TrailOf(LayerId layer) {
  assert(layer.value() >= 0);
  assert(shot_ != nullptr);
  return &trails_[layer.value()];
}

void PoseMemo::SetOverride(LayerId layer, PoseMap poses) {
  assert(layer.value() >= 0);
  assert(overrides_.size() < 4096);
  overrides_[layer.value()] = std::move(poses);
}

void PoseMemo::ClearOverride(LayerId layer) {
  assert(layer.value() >= 0);
  assert(overrides_.size() < 4096);
  overrides_.erase(layer.value());
}

const PoseMap* PoseMemo::OverrideOf(LayerId layer) const {
  assert(layer.value() >= 0);
  assert(overrides_.size() < 4096);
  const auto found = overrides_.find(layer.value());
  const bool is_set = found != overrides_.end();
  return is_set ? &found->second : nullptr;
}

}  // namespace snapper
