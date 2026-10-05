// PresetManager's saved poses.

#include <algorithm>
#include <cassert>

#include "anim/doll_pose.h"
#include "edit/edit_scope.h"
#include "edit/history_manager.h"
#include "edit/pose_edits.h"
#include "edit/preset_manager.h"

namespace snapper {

QStringList PresetManager::SavedPoses() const {
  assert(poses_.size() <= static_cast<size_t>(kMaxSavedPoses));
  QStringList names;
  for (const SavedPose& pose : poses_) {
    names.append(pose.name);
  }
  names.sort(Qt::CaseInsensitive);
  assert(names.size() == static_cast<qsizetype>(poses_.size()));
  return names;
}

Result<void> PresetManager::SavePose(const QString& name, ShotId shot,
                                     LayerId layer, Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const QString trimmed = name.trimmed();
  const DollLayer* posed = PosedLayerOf(history_->current(), shot, layer);
  const bool is_valid = !trimmed.isEmpty() && posed != nullptr;
  if (!is_valid) {
    return std::unexpected(
        Error{Tr("Name the pose and pick a doll layer first.")});
  }
  std::vector<SavedPose> next = poses_;
  std::erase_if(next, [&](const SavedPose& p) { return p.name == trimmed; });
  const bool is_full = next.size() >= static_cast<size_t>(kMaxSavedPoses);
  if (is_full) {
    return std::unexpected(Error{
        Tr("You can keep at most %1 poses.").arg(kMaxSavedPoses)});
  }
  next.push_back({trimmed, posed->doll, SamplePoses(*posed, frame)});
  return Store(std::move(next));
}

Result<void> PresetManager::ApplyPose(const QString& name, ShotId shot,
                                      LayerId layer, Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const auto saved = std::find_if(poses_.begin(), poses_.end(),
                                  [&](const SavedPose& p) {
                                    return p.name == name;
                                  });
  const Doll* doll = DollOfLayer(history_->current(), shot, layer);
  const bool is_ready = saved != poses_.end() && doll != nullptr;
  if (!is_ready) {
    return std::unexpected(
        Error{Tr("Pick a doll layer and a saved pose first.")});
  }
  Project next = history_->current();
  int fitted = 0;
  for (const auto& [piece, pose] : saved->pieces) {
    const bool fits = FindRig(doll->rig, piece) != nullptr;
    if (!fits) {
      continue;
    }
    const TrackRef track{shot, TrackKind::kPiece, layer, piece};
    auto keyed = KeyedAt(next, track, frame, PiecePose(),
                         [&pose](PiecePose* value) {
                           *value = pose;
                           return Result<void>();
                         });
    if (!keyed) {
      return std::unexpected(keyed.error());
    }
    next = std::move(*keyed);
    ++fitted;
  }
  const bool fits_any = fitted > 0;
  if (!fits_any) {
    return std::unexpected(Error{
        Tr("%1 shares no pieces with this doll.").arg(name)});
  }
  return history_->Apply(Tr("Apply pose %1").arg(name), std::move(next));
}

Result<void> PresetManager::ApplyPoseAll(const QString& name, ShotId shot,
                                         const std::vector<LayerId>& layers,
                                         Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  EditScope batch(history_, Tr("Apply pose %1").arg(name));
  int applied = 0;
  QString why;
  for (const LayerId layer : layers) {
    const auto done = ApplyPose(name, shot, layer, frame);
    applied += done.has_value() ? 1 : 0;
    why = done.has_value() ? why : done.error().message;
  }
  const bool is_none = applied == 0;
  if (is_none) {
    batch.Cancel();
    return std::unexpected(
        Error{why.isEmpty() ? Tr("Pick a doll layer first.") : why});
  }
  return {};
}

Result<void> PresetManager::DeletePose(const QString& name) {
  assert(!poses_path_.isEmpty());
  assert(poses_.size() <= static_cast<size_t>(kMaxSavedPoses));
  std::vector<SavedPose> next = poses_;
  const auto removed =
      std::erase_if(next, [&](const SavedPose& p) { return p.name == name; });
  const bool is_removed = removed > 0;
  if (!is_removed) {
    return std::unexpected(Error{Tr("There is no pose called %1.")
                                     .arg(name)});
  }
  return Store(std::move(next));
}

Result<void> PresetManager::Store(std::vector<SavedPose> poses) {
  assert(!poses_path_.isEmpty());
  assert(poses.size() <= static_cast<size_t>(kMaxSavedPoses));
  const bool is_damaged = !load_error_.isEmpty();
  if (is_damaged) {
    return std::unexpected(Error{
        Tr("Your saved poses can't be read (%1). Move %2 away to start a "
           "fresh list.").arg(load_error_, poses_path_)});
  }
  auto written = WritePoses(poses_path_, poses);
  if (!written) {
    return written;
  }
  poses_ = std::move(poses);
  emit SavedPosesChanged();
  return {};
}

}  // namespace snapper
