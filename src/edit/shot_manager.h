#ifndef SNAPPER_EDIT_SHOT_MANAGER_H_
#define SNAPPER_EDIT_SHOT_MANAGER_H_

#include <QColor>
#include <QString>

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

 private:
  HistoryManager* history_;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_SHOT_MANAGER_H_
