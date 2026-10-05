#ifndef SNAPPER_EDIT_SHOT_MANAGER_H_
#define SNAPPER_EDIT_SHOT_MANAGER_H_

#include <QColor>
#include <QString>

#include <vector>

#include "base/error.h"
#include "base/frame.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;

QString TransitionName(TransitionKind kind);

// The master track: which shots there are, in what order, how long,
// and how each hands over to the next.
class ShotManager final {
 public:
  explicit ShotManager(HistoryManager* history);

  // A blank two-second shot at index (-1 for the end). Returns its id.
  Result<ShotId> Add(int index);
  // A copy right after the original, with fresh ids throughout.
  Result<ShotId> Duplicate(ShotId shot);
  Result<void> Remove(ShotId shot);
  // Moves a shot to index on the master track.
  Result<void> Move(ShotId shot, int index);
  // At least one frame.
  Result<void> SetLength(ShotId shot, Frame length);
  Result<void> Rename(ShotId shot, const QString& name);
  Result<void> SetBackground(ShotId shot, QColor color);
  // How shot hands over to the one after it.
  Result<void> SetTransition(ShotId shot, Transition transition);

  // The same change to many shots at once, as one undo step; lengths
  // are added to each shot's own.
  Result<void> RemoveAll(const std::vector<ShotId>& shots);
  Result<void> DuplicateAll(const std::vector<ShotId>& shots);
  Result<void> ShiftLength(const std::vector<ShotId>& shots, int delta);
  Result<void> SetBackgroundAll(const std::vector<ShotId>& shots,
                                QColor color);
  Result<void> SetTransitionAll(const std::vector<ShotId>& shots,
                                Transition transition);

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SHOT_MANAGER_H_
