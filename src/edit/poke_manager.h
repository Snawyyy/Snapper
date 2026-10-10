#ifndef SNAPPER_EDIT_POKE_MANAGER_H_
#define SNAPPER_EDIT_POKE_MANAGER_H_

#include <QObject>
#include <QPointF>
#include <QString>
#include <QTimer>

#include <vector>

#include "anim/doll_drag.h"
#include "anim/doll_pose.h"
#include "base/frame.h"
#include "model/layer.h"
#include "model/shot.h"

namespace snapper {

class HistoryManager;
class PlaybackManager;

// Poking a doll like a toy: a live shove that makes its drag, cloth and
// jelly dots wobble and settle, with nothing keyed and nothing in
// history. The doll is run on a copy of its layer that holds the pose
// of the frame it was poked on and is shoved a little more each virtual
// frame after it (see PokeLayer), so the springs answer the shove the
// way they would a keyed move. A timer ticks one virtual frame at the
// film rate while a poke lives, and the poke ends by itself once the
// dots have settled after the mouse is let go, after a hard cap, when
// the playhead moves, or when the doll is gone. An edit does not end
// it: the poke is run again on the edited doll, so a changed setting is
// felt at once.
class PokeManager final : public QObject {
  Q_OBJECT

 public:
  PokeManager(HistoryManager* history, PlaybackManager* playback);

  // Empty when layer of shot can be poked, else why not, in words for
  // a toast. An invalid layer (nothing under the cursor) says to hover
  // a doll.
  QString WhyNot(ShotId shot, LayerId layer) const;

  // Starts poking layer on shot-local frame at, with the mouse at
  // point (shot space). Returns why not, or empty. Replaces a poke in
  // progress.
  QString Begin(ShotId shot, LayerId layer, Frame at, QPointF point);
  // The mouse moved to point (shot space); no effect once let go.
  void Move(QPointF point);
  // The mouse let go: the doll settles and the poke ends by itself.
  void Release();
  // Stops at once; the doll shows its real pose again.
  void End();
  // Runs one virtual frame. The timer calls this; a test can too.
  void Tick();

  bool IsActive() const { return is_active_; }
  bool IsHeld() const { return is_held_; }
  LayerId layer() const { return layer_; }
  // The poses to show for layer() while active.
  const PoseMap& poses() const { return poses_; }
  // Virtual frames run so far.
  int ticks() const { return static_cast<int>(shoves_.size()); }

 signals:
  // A poke started, moved on a frame, or ended.
  void Changed();

 private:
  // Works the poses out for the virtual frame reached; false when the
  // doll is gone.
  bool Solve();
  void OnProjectChanged();
  void OnFrameChanged();

  HistoryManager* history_;
  PlaybackManager* playback_;
  QTimer clock_;
  bool is_active_ = false;
  bool is_held_ = false;
  ShotId shot_;
  LayerId layer_;
  Frame at_;
  QPointF anchor_;
  QPointF shove_;
  std::vector<QPointF> shoves_;
  DragTrail trail_;
  PoseMap poses_;
  int calm_ticks_ = 0;
};

}  // namespace snapper

#endif  // SNAPPER_EDIT_POKE_MANAGER_H_
