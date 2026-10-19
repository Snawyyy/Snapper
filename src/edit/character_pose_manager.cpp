#include "edit/character_pose_manager.h"

#include <cassert>

#include "anim/character_pose.h"
#include "anim/sampler.h"
#include "base/text.h"
#include "edit/character_pose_edits.h"
#include "edit/history_manager.h"
#include "edit/project_edits.h"

namespace snapper {
namespace {

const CharacterLayer* PosedOf(const Project& project, ShotId shot,
                              LayerId layer) {
  assert(shot.value() >= 0);
  assert(layer.value() >= 0);
  const Shot* found = FindShot(project, shot);
  const Layer* item =
      found != nullptr && layer.IsValid() ? FindLayer(*found, layer) : nullptr;
  return item != nullptr ? std::get_if<CharacterLayer>(&item->content)
                         : nullptr;
}

Error NotCharacter() {
  return Error{Tr("That layer is not a character any more.")};
}

}  // namespace

CharacterPoseManager::CharacterPoseManager(HistoryManager* history)
    : history_(history) {
  assert(history_ != nullptr);
  assert(history_->current().shots.size() <= static_cast<size_t>(kMaxShots));
}

const Character* CharacterPoseManager::CharacterOf(ShotId shot,
                                                   LayerId layer) const {
  assert(history_ != nullptr);
  assert(shot.value() >= 0);
  const CharacterLayer* posed = PosedOf(history_->current(), shot, layer);
  return posed != nullptr && !posed->character.isEmpty()
             ? FindCharacter(history_->current(), posed->character)
             : nullptr;
}

CharacterPose CharacterPoseManager::HandPose(ShotId shot, LayerId layer,
                                             Frame frame) const {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const CharacterLayer* posed = PosedOf(history_->current(), shot, layer);
  return posed != nullptr ? Sample(posed->poses, frame, CharacterPose{})
                          : CharacterPose{};
}

Result<void> CharacterPoseManager::Pose(
    ShotId shot, LayerId layer, Frame frame, const QString& label,
    const std::function<CharacterPose(const CharacterPose&)>& change) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  const Project& project = history_->current();
  return history_->Apply(
      label, WithLayer(project, shot, layer, [&](Layer* edited) -> Result<void> {
        auto* posed = std::get_if<CharacterLayer>(&edited->content);
        if (posed == nullptr) {
          return std::unexpected(NotCharacter());
        }
        const CharacterPose before =
            Sample(posed->poses, frame, CharacterPose{});
        const CharacterPose after = Trimmed(change(before));
        const int at = KeyIndexAt(posed->poses, frame);
        const Ease ease = at >= 0 ? posed->poses.keys[static_cast<size_t>(at)].ease
                                  : Ease::kStep;
        if (!SetKey(&posed->poses, Key<CharacterPose>{frame, after, ease})) {
          return std::unexpected(Error{Tr("This layer has too many keys.")});
        }
        return {};
      }));
}

Result<void> CharacterPoseManager::RemoveKey(ShotId shot, LayerId layer,
                                             Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  return history_->Apply(
      Tr("Remove key"),
      WithLayer(history_->current(), shot, layer,
                [&](Layer* edited) -> Result<void> {
                  auto* posed = std::get_if<CharacterLayer>(&edited->content);
                  if (posed == nullptr) {
                    return std::unexpected(NotCharacter());
                  }
                  if (!snapper::RemoveKey(&posed->poses, frame)) {
                    return std::unexpected(
                        Error{Tr("There is no key on this frame.")});
                  }
                  return {};
                }));
}

Result<void> CharacterPoseManager::ResetPose(ShotId shot, LayerId layer,
                                             Frame frame) {
  assert(history_ != nullptr);
  assert(frame.index() >= 0);
  return Pose(shot, layer, frame, Tr("Reset pose"),
              [](const CharacterPose&) { return CharacterPose{}; });
}

}  // namespace snapper
