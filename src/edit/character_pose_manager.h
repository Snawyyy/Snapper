#ifndef SNAPPER_EDIT_CHARACTER_POSE_MANAGER_H_
#define SNAPPER_EDIT_CHARACTER_POSE_MANAGER_H_

#include <QString>

#include <functional>

#include "base/error.h"
#include "base/frame.h"
#include "model/character.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

// Owns posing characters on a shot's layers: every pose change keys the
// whole pose at the frame (a key holds the full drawing of the moment),
// starting from what the layer shows there. Inside an EditScope a drag
// previews live and lands as one step.
class CharacterPoseManager final {
 public:
  explicit CharacterPoseManager(HistoryManager* history);

  // The pose layer's keys give at frame, without smart actions: what the
  // animator posed by hand. Rest when the layer is gone.
  CharacterPose HandPose(ShotId shot, LayerId layer, Frame frame) const;
  // The character a layer shows, or nullptr.
  const Character* CharacterOf(ShotId shot, LayerId layer) const;

  // Keys layer at frame with change(hand pose there), one undo step named
  // label. A change that leaves the pose as it was records nothing.
  Result<void> Pose(ShotId shot, LayerId layer, Frame frame,
                    const QString& label,
                    const std::function<CharacterPose(const CharacterPose&)>&
                        change);
  // Takes the key at frame away (refused when there is none).
  Result<void> RemoveKey(ShotId shot, LayerId layer, Frame frame);
  // Keys the rest pose at frame.
  Result<void> ResetPose(ShotId shot, LayerId layer, Frame frame);
  // Flips the character left to right on the stage.
  Result<void> SetFlipped(ShotId shot, LayerId layer, bool is_flipped);

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_CHARACTER_POSE_MANAGER_H_
